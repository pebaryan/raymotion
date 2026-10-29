// Malaria metabolomics explainer: FRAME-style flat cards, 13 scenes, Qwen3-TTS voice.
// Source brief: project/001-malaria-metabolomics-explainer.md
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace rmtn { namespace {
constexpr int W = 1920, H = 1080, FPS = 30, N = 13;
constexpr float LEAD = 0.35f;
const std::string VO_DIR = "vo/malaria-sentences-norm/";
const std::string SFX_DIR = "audio/austria/";
const std::string HIT_DIR = "audio/malaria/";      // procedural impact + stamp
const std::string POP = "audio/flags/pop.wav";

const Color CREAM{ 242, 233, 214, 255 }, INK{ 21, 21, 21, 255 }, HGREEN{ 63, 174, 106, 255 };
const Color HPINK{ 239, 91, 123, 255 }, HYELLOW{ 243, 198, 78, 255 }, HORANGE{ 231, 111, 81, 255 };
const Color MUTED{ 122, 116, 104, 255 };

struct Line {
    std::string file, text;
    double seconds = 0;
    int at = 0, frames = 0, origScene = 0;
};
struct Scene {
    std::string section, pill;
    std::vector<Line> lines;
    int len = 0, start = 0;
};

double wavSeconds(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return 0;
    unsigned char h[12]; double secs = 0; unsigned byteRate = 0;
    if (std::fread(h, 1, 12, f) == 12 && !std::memcmp(h, "RIFF", 4) && !std::memcmp(h + 8, "WAVE", 4)) {
        unsigned char c[8];
        while (std::fread(c, 1, 8, f) == 8) {
            const unsigned size = c[4] | c[5] << 8 | c[6] << 16 | (unsigned)c[7] << 24;
            if (!std::memcmp(c, "fmt ", 4)) {
                unsigned char fmt[16];
                if (size < 16 || std::fread(fmt, 1, 16, f) != 16) break;
                byteRate = fmt[8] | fmt[9] << 8 | fmt[10] << 16 | (unsigned)fmt[11] << 24;
                std::fseek(f, (long)(size - 16 + (size & 1)), SEEK_CUR);
            } else if (!std::memcmp(c, "data", 4)) {
                if (byteRate) secs = (double)size / byteRate;
                break;
            } else std::fseek(f, (long)(size + (size & 1)), SEEK_CUR);
        }
    }
    std::fclose(f); return secs;
}

std::vector<Scene> makeScenes() {
    const char* sections[N] = { "MALARIA / METABOLOMICS", "THE STAKES", "THE IDEA", "THE TRAP",
        "THE DATA", "CLEANING", "NARROWING", "THE HONEST TEST", "THE RESULT", "THE CANDIDATES",
        "THE DEMO", "WHAT IT IS / IS NOT", "NEXT" };
    const char* pills[N] = { "PROOF OF CONCEPT", "WHO 2024", "3D SPECIMENS", "OVERFITTING",
        "PUBLIC DATA", "PCA / COMBAT", "FEATURE SELECTION", "LODO-CV", "AUC-ROC",
        "220 -> 11", "OPEN SOURCE", "HONEST LIMITS", "3 STEPS" };
    const float tails[N] = { 1.2f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 2.0f, 1.5f, 1.5f, 1.5f, 4.0f };
    struct Spec { int scene, line; std::string text, file; };
    std::vector<Spec> specs;
    std::string manifest;
    try { manifest = media::resolve(VO_DIR + "lines.json"); } catch (...) {}
    if (!manifest.empty()) {
        std::ifstream in(manifest, std::ios::binary);
        if (in) {
            std::stringstream ss; ss << in.rdbuf();
            Json spec = Json::parse(ss.str());
            for (const Json& l : spec["lines"].asArray()) {
                Spec x;
                x.scene = (int)l.num("scene", 0);
                x.line = (int)l.num("line", 0);
                x.text = l.str("text", "");
                x.file = l.str("file", "");
                if (x.scene >= 1 && x.scene <= N && !x.file.empty()) specs.push_back(std::move(x));
            }
        }
    }
    const int leadFrames = (int)std::lround(LEAD * FPS);
    const int gapFrames = 8;
    std::vector<Scene> out;
    int cursor = 0;
    for (int si = 0; si < N; ++si) {
        Scene s;
        s.section = sections[si]; s.pill = pills[si];
        int at = leadFrames;
        for (const Spec& x : specs) {
            if (x.scene != si + 1) continue;
            Line l;
            l.file = x.file; l.text = x.text; l.origScene = x.scene;
            std::string vp;
            try { vp = media::resolve(VO_DIR + l.file); } catch (...) {}
            l.seconds = vp.empty() ? 0 : wavSeconds(vp);
            if (l.seconds <= 0) l.seconds = std::max(1.0, l.text.size() / 14.0);
            l.frames = (int)std::ceil(l.seconds * FPS);
            l.at = at;
            at += l.frames + gapFrames;
            s.lines.push_back(std::move(l));
        }
        s.len = at - gapFrames + (int)std::lround(tails[si] * FPS);
        s.start = cursor;
        cursor += s.len;
        out.push_back(std::move(s));
    }
    return out;
}

// ------------------------------------------------------------------ paper tools
float smooth01(float x) { x = std::clamp(x, 0.f, 1.f); return x * x * (3 - 2 * x); }
float ramp(float t, float a, float b) { return smooth01((t - a) / std::max(.001f, b - a)); }
Font sans() { return fonts::load({ "ArchivoBlack-Regular.ttf", "Inter-Bold.ttf" }, 96); }
Font body() { return fonts::load({ "SpaceGrotesk-Variable.ttf", "Inter-Regular.ttf" }, 64); }
Font mono() { return fonts::load({ "JetBrainsMono-Bold.ttf" }, 64); }

void drawBurkinaMap(const Ctx& cx, Rectangle box) {
    const geo::Layer africa = geo::countries().filter(
        [](const geo::Feature& f) { return f.properties.str("continent", "") == "Africa"; });
    auto p = geo::Projection::orthographic().rotate({ -1.5, 12.5 }).fit(box, africa.merged(), 12);
    vg::fill(cx, p.path(africa.merged()), withAlpha(CREAM, .55f), vg::FillRule::EvenOdd);
    vg::stroke(cx, p.lines(africa.merged()), vg::Stroke(withAlpha(INK, .35f), 1.0f));
    vg::fill(cx, p.path(geo::countries().get("BFA").geometry), withAlpha(HPINK, .82f), vg::FillRule::EvenOdd);
    vg::stroke(cx, p.lines(geo::countries().get("BFA").geometry), vg::Stroke(INK, 2.0f));
}
void drawIndonesiaMap(const Ctx& cx, Rectangle box) {
    const geo::Feature& idn = geo::countries().get("IDN");
    auto p = geo::Projection::equalEarth().fit(box, idn.geometry, 4);
    vg::fill(cx, p.path(idn.geometry), withAlpha(HPINK, .16f), vg::FillRule::EvenOdd);
    vg::stroke(cx, p.lines(idn.geometry), vg::Stroke(HPINK, 2.0f));
}

float mw(Font f, const char* s, float size) { return MeasureTextEx(f, s, size, 0).x; }
void text(Font f, const char* s, float x, float y, float size, Color c, float ax = 0) {
    DrawTextEx(f, s, { x, y }, size, 0, c);
    (void)ax;
}
void textC(Font f, const char* s, float x, float y, float size, Color c) {
    DrawTextEx(f, s, { x - mw(f, s, size) * .5f, y }, size, 0, c);
}
void textR(Font f, const char* s, float x, float y, float size, Color c) {
    DrawTextEx(f, s, { x - mw(f, s, size), y }, size, 0, c);
}
std::vector<std::string> wrap(Font f, const std::string& s, float size, float width) {
    std::vector<std::string> lines; std::string line, word;
    auto flush = [&] {
        if (word.empty()) return;
        const std::string next = line.empty() ? word : line + " " + word;
        if (!line.empty() && mw(f, next.c_str(), size) > width) { lines.push_back(line); line = word; }
        else line = next;
        word.clear();
    };
    for (char ch : s) {
        if (ch == ' ') flush(); else word += ch;
    }
    flush(); if (!line.empty()) lines.push_back(line);
    if (lines.empty()) lines.emplace_back();
    return lines;
}
void textBlock(Font f, const std::string& s, float x, float y, float size, float width, Color c,
               float lineH = 1.25f, int maxLines = 99) {
    auto lines = wrap(f, s, size, width);
    for (size_t i = 0; i < lines.size() && (int)i < maxLines; ++i)
        text(f, lines[i].c_str(), x, y + i * size * lineH, size, c);
}
void textA(Font f, const char* s, float x, float y, float size, Color c, float a, float dy = 0) {
    if (a > 0) text(f, s, x, y + dy, size, withAlpha(c, a));
}
void textBlockA(Font f, const std::string& s, float x, float y, float size, float width, Color c,
                float a, float lineH = 1.25f, int maxLines = 99, float dy = 0) {
    if (a <= 0) return;
    auto lines = wrap(f, s, size, width);
    for (size_t i = 0; i < lines.size() && (int)i < maxLines; ++i)
        text(f, lines[i].c_str(), x, y + dy + i * size * lineH, size, withAlpha(c, a));
}
void boxA(float x, float y, float w, float h, Color fill, float a, bool shadow = false) {
    if (a <= 0) return;
    if (shadow) DrawRectangle((int)(x + 12), (int)(y + 12), (int)w, (int)h, withAlpha(HORANGE, a));
    DrawRectangle((int)x, (int)y, (int)w, (int)h, withAlpha(fill, a));
    DrawRectangleLinesEx({ x, y, w, h }, 6, withAlpha(INK, a));
}
void shadowBox(float x, float y, float w, float h, Color fill, bool shadow = false) {
    if (shadow) DrawRectangle((int)(x + 12), (int)(y + 12), (int)w, (int)h, HORANGE);
    DrawRectangle((int)x, (int)y, (int)w, (int)h, fill);
    DrawRectangleLinesEx({ x, y, w, h }, 6, INK);
}
void box(float x, float y, float w, float h, Color fill) { shadowBox(x, y, w, h, fill, false); }
void arrow(float x0, float y0, float x1, float y1, Color c, float th = 4) {
    DrawLineEx({ x0, y0 }, { x1, y1 }, th, c);
    const float a = std::atan2(y1 - y0, x1 - x0), s = 14;
    DrawTriangle({ x1, y1 }, { x1 - s * std::cos(a - .5f), y1 - s * std::sin(a - .5f) },
                 { x1 - s * std::cos(a + .5f), y1 - s * std::sin(a + .5f) }, c);
}
void check(float x, float y, Color c = HGREEN) {
    DrawLineEx({ x, y + 14 }, { x + 10, y + 25 }, 6, c);
    DrawLineEx({ x + 10, y + 25 }, { x + 34, y }, 6, c);
}
void cross(float x, float y, Color c = HPINK) {
    DrawLineEx({ x, y }, { x + 25, y + 25 }, 6, c);
    DrawLineEx({ x + 25, y }, { x, y + 25 }, 6, c);
}
void stamp(const char* s, float x, float y, float size, Color fill, Color ink, float rot = -4) {
    const float w = mw(mono(), s, size) + 44, h = size * 1.45f;
    DrawRectanglePro({ x + 8, y + 8, w, h }, { 0, 0 }, rot, HORANGE);
    DrawRectanglePro({ x, y, w, h }, { 0, 0 }, rot, fill);
    const Vector2 origin{ 0, 0 };
    DrawTextPro(mono(), s, { x + 22, y + size * .18f }, origin, rot, size, 0, ink);
}
void pill(const char* s, Color fill = INK, Color ink = CREAM) {
    const Font f = mono(); const float w = mw(f, s, 19) + 34;
    DrawRectangleRounded({ 1856 - w, 24, w, 38 }, .5f, 8, fill);
    text(f, s, 1856 - w + 17, 34, 19, ink);
}
void topbar(const Scene& s) {
    DrawRectangle(64, 34, 1792, 2, INK);
    text(mono(), s.section.c_str(), 64, 42, 19, INK);
    pill(s.pill.c_str());
}
void footer(int ch, float local, Color c = INK) {
    DrawRectangle(64, 1012, 1792, 2, c);
    text(mono(), "PRASETYO & RUSDI", 64, 1028, 17, withAlpha(c, .8f));
    char buf[32]; std::snprintf(buf, sizeof buf, "%02d / %02d", ch + 1, N);
    textR(mono(), buf, 1856, 1028, 17, withAlpha(c, .8f));
}
void caption(const Scene& s, float local, bool greenGround) {
    if (s.lines.empty()) return;
    int li = -1;
    for (int i = 0; i < (int)s.lines.size(); ++i)
        if (local * FPS >= s.lines[i].at) li = i;
    if (li < 0) return;
    const Line& l = s.lines[li];
    const float a = l.at / (float)FPS;
    const float alpha = std::min(1.f, std::max(0.f, (local - a) / .15f));
    const Font f = body();
    const Color col = greenGround ? CREAM : INK;
    auto lines = wrap(f, l.text, 25, 1700);
    int nwords = 0;
    bool inWord = false;
    for (char ch : l.text) {
        if (std::isspace((unsigned char)ch)) inWord = false;
        else if (!inWord) { inWord = true; ++nwords; }
    }
    const double u = std::clamp((double)(local - a) * FPS / std::max(1, l.frames), 0.0, 1.0);
    const int spoken = std::clamp((int)std::floor(u * nwords) + 1, 0, nwords);
    const float space = MeasureTextEx(f, " ", 25, 0).x;
    const float y0 = 896;
    DrawRectangle(64, 886, 1792, 2, withAlpha(col, .45f * alpha));
    int idx = 0;
    for (size_t k = 0; k < lines.size() && k < 3; ++k) {
        const std::string& line = lines[k];
        float x = 64;
        size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && std::isspace((unsigned char)line[i])) ++i;
            const size_t j = i;
            while (i < line.size() && !std::isspace((unsigned char)line[i])) ++i;
            if (j == i) break;
            const std::string word = line.substr(j, i - j);
            const Color wc = idx < spoken ? withAlpha(col, .92f * alpha) : withAlpha(col, .28f * alpha);
            text(f, word.c_str(), x, y0 + k * 29, 23, wc);
            x += MeasureTextEx(f, word.c_str(), 23, 0).x + space;
            ++idx;
        }
    }
}

// ------------------------------------------------------------------ 3D specimens
struct Specimen { std::shared_ptr<g3d::Scene> scene; };
struct Specimens { Specimen phe, s1p, ret; };

void applyGooch(g3d::Object3D& o) {
    o.material.shading = g3d::Shading::Gooch;
    o.material.gooch.warm = Color{ 255, 214, 170, 255 };
    o.material.gooch.cool = Color{ 55, 92, 105, 255 };
    o.material.lineColor = INK;
    o.material.lineWidth = 1.25f;
    o.material.roughness = .55f;
    o.material.metalness = 0;
    for (auto& c : o.children) applyGooch(*c);
}
Specimen makeSpecimen(const char* file, float zoom = 1.f, float rotBias = 0.f) {
    Specimen out;
    try {
        auto scene = std::make_shared<g3d::Scene>();
        scene->camera.fov = 34;
        scene->camera.position = { 0, 2.3f, 9.5f };
        scene->camera.target = { 0, 0, 0 };
        scene->camera.up = [](int t, g3d::Camera& cam) {
            const float a = .8f + t * .0045f;
            cam.position = { 9.5f * std::cos(a), 3.2f, 9.5f * std::sin(a) };
            cam.target = { 0, 0, 0 };
        };
        g3d::Light key = g3d::Light::directional({ -4, 7, 6 }, CREAM, 1.15f, true);
        key.shadowExtent = 8; key.shadowBias = .06f;
        scene->lights.push_back(key);
        scene->skyColor = CREAM;
        scene->groundColor = Color{ 150, 184, 173, 255 };
        scene->ambient = .5f;
        scene->lines.enabled = true;
        scene->lines.color = INK;
        scene->lines.width = 1.8f;
        scene->lines.wobble = 1.1f;
        scene->lines.boilEvery = 3;
        chem::Style3D st;
        st.mode = chem::Style3D::Mode::BallAndStick;
        st.atomScale = .30f;
        st.bondRadius = .13f;
        st.hydrogens = true;
        chem::Molecule m;
        if (std::string(file).find("sphingosine") != std::string::npos)
            m = chem::smiles("CCCCCCCCCCCCC=CC(C(COP(=O)(O)O)N)O");
        else
            m = chem::loadMolecule(file);
        if (!m.coords3d) chem::embed3d(m);
        Vector3 centroid{ 0, 0, 0 }, bboxCenter{ 0, 0, 0 };
        if (!m.atoms.empty()) {
            float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
            for (const auto& a : m.atoms) {
                centroid.x += a.pos.x; centroid.y += a.pos.y; centroid.z += a.pos.z;
                x0 = std::min(x0, a.pos.x); x1 = std::max(x1, a.pos.x);
                y0 = std::min(y0, a.pos.y); y1 = std::max(y1, a.pos.y);
                z0 = std::min(z0, a.pos.z); z1 = std::max(z1, a.pos.z);
            }
            centroid.x /= (float)m.atoms.size(); centroid.y /= (float)m.atoms.size(); centroid.z /= (float)m.atoms.size();
            bboxCenter = { (x0 + x1) * .5f, (y0 + y1) * .5f, (z0 + z1) * .5f };
        }
        g3d::Object3D* mol = chem::addMolecule(*scene, m, st);
        mol->position = { centroid.x - bboxCenter.x, centroid.y - bboxCenter.y, centroid.z - bboxCenter.z };
        const float r = std::max(1.f, m.radius());
        mol->scale = { 2.9f * zoom / r, 2.9f * zoom / r, 2.9f * zoom / r };
        applyGooch(*mol);
        chem::buildIn(mol, 0, 2, 14);
        mol->up = [rotBias](int t, g3d::Object3D& o) { o.rotation = { 12, t * .55f + rotBias, 0 }; };
        out.scene = scene;
    } catch (...) {}
    return out;
}
void drawSpecimen(const Ctx& cx, int t, const Specimen& sp, Rectangle box,
                  const char* name, const char* role) {
    dr::rect(cx, box.x, box.y, box.width, box.height, CREAM);
    if (sp.scene) g3d::draw(sp.scene, box)(cx, t);
    dr::rect(cx, box.x, box.y, box.width, 6, INK);
    dr::rect(cx, box.x, box.y + box.height - 6, box.width, 6, INK);
    dr::rect(cx, box.x, box.y, 6, box.height, INK);
    dr::rect(cx, box.x + box.width - 6, box.y, 6, box.height, INK);
    text(mono(), name, box.x + 14, box.y + box.height + 16, 20, INK);
    text(mono(), role, box.x + 14, box.y + box.height + 42, 17, MUTED);
}
void scene01(const Scene&, float t) {
    const float u1 = ramp(t, .48f, .80f);
    const float u2 = ramp(t, .88f, 1.20f);
    const float u3 = ramp(t, 2.72f, 3.10f);
    const float u4 = ramp(t, 3.44f, 3.82f);
    const float u5 = ramp(t, 4.80f, 5.20f);
    const float e1 = (float)ease::out(ease::quad)(u1);
    const float e2 = (float)ease::out(ease::quad)(u2);
    const float e3 = (float)ease::out(ease::quad)(u3);
    const float e4 = (float)ease::out(ease::quad)(u4);
    text(sans(), "DOES IT", 64, 238 + (1 - e1) * 22, 184, withAlpha(INK, u1));
    text(sans(), "TRAVEL?", 64, 418 + (1 - e2) * 22, 184, withAlpha(HPINK, u2));
    text(mono(), "CROSS-DATASET MACHINE LEARNING FOR MALARIA METABOLOMICS",
         68, 674 + (1 - e3) * 14, 25, withAlpha(MUTED, u3));
    if (u4 > 0)
        textBlock(mono(), "A 3-minute proof-of-concept, told without ML jargon.",
                  68, 718 + (1 - e4) * 14, 26, 1100, withAlpha(INK, u4), 1.2f, 2);
    if (u5 > 0) text(mono(), "ONE QUESTION. TWO DATASETS. THIRTEEN SCENES.", 68, 810, 22, withAlpha(MUTED, u5));
}

void scene03v2(const Ctx& cx, float t, const Specimens& sp) {
    const float tu = ramp(t, .48f, .80f);
    const float su = ramp(t, .88f, 1.20f);
    textA(sans(), "THE CHEMICAL FINGERPRINT.", 64, 118, 56, INK, tu, (1 - (float)ease::out(ease::quad)(tu)) * 18);
    textA(mono(), "SERUM / 3D MOLECULAR SPECIMENS / ILLUSTRATIVE", 68, 188, 20, MUTED, su, (1 - (float)ease::out(ease::quad)(su)) * 12);
    const int ft = (int)(t * FPS);
    const float at[3] = { 3.60f, 4.72f, 5.44f };
    const Specimen* sps[3] = { &sp.phe, &sp.ret, &sp.s1p };
    const char* names[3] = { "PHENYLALANINE", "RETINOL", "SPHINGOSINE-1-PHOSPHATE" };
    const char* roles[3] = { "AMINO ACID", "VITAMIN A", "LIPID" };
    for (int i = 0; i < 3; ++i) {
        const float u = std::clamp((t - at[i]) / .42f, 0.f, 1.f);
        if (u <= 0) continue;
        const float e = (float)ease::out(ease::back(1.2))(u);
        drawSpecimen(cx, ft, *sps[i], { 410.f + i * 520.f, 226.f + (1 - e) * 38.f, 400, 410 }, names[i], roles[i]);
    }

    const float hu = ramp(t, 1.00f, 1.45f);
    const float mix = (float)ease::out(ease::quad)(ramp(t, 6.50f, 8.20f));
    if (hu > 0) {
        const float y0 = 825;
        textA(mono(), "BLOOD HISTOGRAM / METABOLITE MIX", 64, 700, 17, MUTED, hu);
        textA(mono(), mix < .5f ? "HEALTHY MIX" : "INFECTED MIX", 1600, 700, 18,
              mix < .5f ? HGREEN : HPINK, hu);
        DrawLineEx({ 64, y0 }, { 1856, y0 }, 3, withAlpha(INK, .65f * hu));
        for (int i = 0; i < 32; ++i) {
            const float healthy = 16 + 62 * std::exp(-std::pow((i - 7) / 4.5f, 2))
                                  + 24 * std::exp(-std::pow((i - 18) / 5.2f, 2));
            const float infected = 16 + 54 * std::exp(-std::pow((i - 20) / 4.0f, 2))
                                   + 30 * std::exp(-std::pow((i - 4) / 3.8f, 2));
            const float h = healthy + (infected - healthy) * mix;
            const Color c = ColorLerp(HGREEN, HPINK, mix);
            DrawRectangle((int)(64 + i * 56), (int)(y0 - h), 52, (int)h, withAlpha(c, .62f * hu));
        }
    }

    textA(mono(), "STRUCTURES: PUBCOMPOUND CIDs 6140 / 5283560 / 445354", 64, 846, 16, MUTED, ramp(t, 9.60f, 10.00f));
}void scene02(const Scene&, float t) {
    textA(sans(), "A STALLED FIGHT.", 64, 118, 64, INK, ramp(t, .48f, .80f), (1 - (float)ease::out(ease::quad)(ramp(t, .48f, .80f))) * 18);
    struct Stat { const char* value; const char* label; const char* sub; Color fill; };
    const Stat st[3] = {
        { "282M", "CASES", "2024 estimate", HGREEN },
        { "610K", "DEATHS", "2024 estimate", HPINK },
        { "US$5.4B", "FUNDING GAP", "whole response", HORANGE },
    };
    const float appear[3] = { 2.40f, 5.68f, 12.27f };
    for (int i = 0; i < 3; ++i) {
        const float x = 64.f + i * 604.f, y = 224;
        const float u = ramp(t, appear[i], appear[i] + .40f);
        if (u <= 0) continue;
        const float e = (float)ease::out(ease::quad)(u);
        shadowBox(x, y + (1 - e) * 18, 560, 316, st[i].fill, i == 1);
        textA(mono(), st[i].label, x + 24, y + 26, 20, INK, u);
        textA(sans(), st[i].value, x + 24, y + 110, 92, INK, u);
        textA(mono(), st[i].sub, x + 24, y + 244, 20, withAlpha(INK, .8f), u);
    }
    const float pu = ramp(t, 9.55f, 9.95f);
    const float gw = mw(sans(), "pfhrp2", 30);
    textA(sans(), "pfhrp2", 64, 620, 30, HPINK, pu);
    if (pu > 0) DrawRectangle(64, 658, (int)(gw * pu), 3, withAlpha(HPINK, pu));
    textA(body(), "gene deletions can compromise rapid tests.", 64 + gw + 12, 626, 24, INK, ramp(t, 10.20f, 10.60f));
    textA(mono(), "Reported in 46 endemic countries.", 64, 690, 22, INK, ramp(t, 11.07f, 11.47f));
    textA(mono(), "Funding gap: US$9.3B target - US$3.9B funded; not research funding.", 64, 720, 22, INK, ramp(t, 12.75f, 13.15f));
    textA(mono(), "SOURCE: WHO WORLD MALARIA REPORT 2024, VIA SUPPLIED BRIEF", 64, 766, 17, MUTED, ramp(t, 13.55f, 13.95f));
}
void scene04(const Scene&, float t) {
    const float bu = ramp(t, .10f, .35f);
    if (bu > 0) {
        DrawRectangle(64, 100, 1792, 780, withAlpha(HORANGE, bu));
        DrawRectangleLinesEx({ 64, 100, 1792, 780 }, 6, withAlpha(INK, bu));
    }
    textA(sans(), "MEMORISING ISN'T LEARNING.", 96, 132, 72, CREAM, ramp(t, .48f, .82f), (1 - (float)ease::out(ease::quad)(ramp(t, .48f, .82f))) * 16);
    textA(mono(), "OVERFITTING", 100, 222, 22, INK, ramp(t, 1.00f, 1.30f));

    const float mu = ramp(t, 3.35f, 3.70f);
    boxA(100, 286, 900, 390, CREAM, mu, false);
    textA(mono(), "220 METABOLITES", 124, 306, 19, INK, mu);
    textA(mono(), "~300 SAMPLES", 124, 334, 19, INK, mu);

    const int cols = 220, rows = 40;
    const float x0 = 124, y0 = 372, cw = 3.9f, ch = 6.4f;
    const float topStart = 3.55f, topSpan = 1.70f;
    for (int c = 0; c < cols; ++c) {
        const float u = std::clamp((t - (topStart + c / (float)cols * topSpan)) / .18f, 0.f, 1.f);
        if (u <= 0) continue;
        const float e = (float)ease::out(ease::back(1.4))(u);
        const float y = y0 + (1 - e) * -18.f;
        DrawRectangle((int)(x0 + c * cw), (int)y, (int)(cw - .6f), (int)(ch - .8f),
                      withAlpha(c % 13 == 0 ? HYELLOW : INK, .62f * u));
    }
    const float rowStart = 5.55f, rowSpan = 2.00f;
    for (int r = 1; r < rows; ++r) {
        const float u = std::clamp((t - (rowStart + (r - 1) / (float)(rows - 1) * rowSpan)) / .22f, 0.f, 1.f);
        if (u <= 0) continue;
        const float e = (float)ease::out(ease::quad)(u);
        const float y = y0 + r * ch + (1 - e) * -10.f;
        for (int c = 0; c < cols; ++c) {
            DrawRectangle((int)(x0 + c * cw), (int)y, (int)(cw - .6f), (int)(ch - .8f),
                          withAlpha(INK, .20f * u));
        }
    }

    const float au = ramp(t, 7.40f, 7.75f);
    boxA(1150, 270, 610, 470, CREAM, au, true);
    textA(mono(), "ANSWER KEY", 1180, 296, 22, INK, au);
    const float la = ramp(t, 7.62f, 8.00f);
    for (int i = 0; i < 9; ++i) DrawLineEx({ 1180, 344.f + i * 28 }, { 1660, 344.f + i * 28 }, 4, withAlpha(INK, .35f * la));
    const float nu = ramp(t, 7.80f, 8.15f);
    textBlockA(mono(), "With many columns and few samples, a model can fit the noise and still fail on new data.",
               100, 710, 22, 930, CREAM, nu, 1.25f, 2,
               (1 - (float)ease::out(ease::quad)(nu)) * 12);
    const float u = ramp(t, 9.78f, 10.18f);
    if (u > 0) stamp("MEMORISED != LEARNED", 1100, 590 + (1 - u) * 16, 28, HPINK, CREAM, -4);
}void scene05(const Ctx& cx, const Scene&, float t) {
    textA(sans(), "TWO DATASETS. ONE QUESTION.", 64, 118, 52, INK, ramp(t, .48f, .80f), (1 - (float)ease::out(ease::quad)(ramp(t, .48f, .80f))) * 16);

    const float x = 64, y = 190, w = 1250, head = 64, row = 80;
    const float fu = ramp(t, 1.12f, 1.47f);
    if (fu > 0) DrawRectangleLinesEx({ x, y, w, head + row * 4 }, 5, withAlpha(INK, fu));
    const float hu = ramp(t, 2.08f, 2.43f);
    if (hu > 0) DrawRectangle((int)x, (int)y, (int)w, (int)head, withAlpha(INK, hu));
    textA(mono(), "PUBLIC SERUM COHORTS / BURKINA FASO", x + 24, y + 22, 18, withAlpha(CREAM, .85f), hu);
    textA(sans(), "ST001516", x + 650, y + 17, 25, CREAM, hu);
    textA(sans(), "ST001517", x + 1000, y + 17, 25, CREAM, hu);

    const char* rows[4][3] = {
        { "OBSERVATIONS", "199", "106" }, { "METABOLITES", "263", "253" },
        { "INFECTED / NOT", "100 / 99", "53 / 53" }, { "CHILDREN", "Gouin, 2-12", "Fulani, 5-12" } };
    for (int r = 0; r < 4; ++r) {
        const float ru = ramp(t, 2.64f + r * .18f, 2.94f + r * .18f);
        if (ru <= 0) continue;
        const float yy = y + head + r * row;
        DrawLineEx({ x + 24, yy }, { x + w - 24, yy }, 2, withAlpha(INK, .35f * ru));
        textA(body(), rows[r][0], x + 28, yy + 22, 22, MUTED, ru);
        textA(sans(), rows[r][1], x + 650, yy + 18, 28, INK, ru);
        textA(sans(), rows[r][2], x + 1000, yy + 18, 28, INK, ru);
    }

    const float sx = 1340, sy = 190, sw = 516;
    const float su = ramp(t, 3.36f, 3.71f);
    if (su > 0) {
        DrawRectangle((int)(sx + 10), (int)(sy + 10), (int)sw, 520, withAlpha(HORANGE, .9f * su));
        DrawRectangle((int)sx, (int)sy, (int)sw, 520, withAlpha(CREAM, su));
        DrawRectangleLinesEx({ sx, sy, sw, 520 }, 5, withAlpha(INK, su));
        textA(mono(), "SOURCE", sx + 24, sy + 22, 18, MUTED, su);
        textA(mono(), "WHERE", sx + 24, sy + 120, 18, MUTED, su);
        drawBurkinaMap(cx, { sx + 30, sy + 204, sw - 60, 220 });
        textA(mono(), "BURKINA FASO / COHORTS", sx + 24, sy + 438, 18, INK, su);
    }

    const float cu = ramp(t, 4.72f, 5.12f);
    if (cu > 0) {
        DrawRectangle((int)(x + 12), (int)(y + head + row * 4 + 10), (int)w, 82, withAlpha(HORANGE, cu));
        DrawRectangle((int)x, (int)(y + head + row * 4 + 2), (int)w, 82, withAlpha(HPINK, cu));
        DrawRectangleLinesEx({ x, y + head + row * 4 + 2, w, 82 }, 5, withAlpha(INK, cu));
        textA(sans(), "305 SAMPLES", x + 24, y + head + row * 4 + 16, 38, INK, cu);
    }
    textA(body(), "220 shared metabolites / repeat visits are not new children", x + 360,
          y + head + row * 4 + 34, 22, INK, ramp(t, 6.96f, 7.36f));

    textA(body(), "Metabolomics Workbench", sx + 24, sy + 52, 23, INK, ramp(t, 7.68f, 8.08f));
    textA(mono(), "ST001516 / ST001517", sx + 24, sy + 82, 18, INK, ramp(t, 7.68f, 8.08f));
    textA(body(), "Burkina Faso, West Africa", sx + 24, sy + 150, 23, INK, ramp(t, 7.68f, 8.08f));
    textA(mono(), "ILLUSTRATIVE MAP", sx + 24, sy + 184, 14, MUTED, ramp(t, 7.68f, 8.08f));

    const float bu = ramp(t, 9.65f, 10.05f);
    if (bu > 0) stamp("DATASET = POPULATION + SITE", 64, 728 + (1 - bu) * 12, 22, HYELLOW, INK, -3);
}

void scene06(const Scene&, float t) {
    textA(sans(), "CLEANING, THEN TWO ISLANDS.", 64, 118, 54, INK, ramp(t, .48f, .80f), (1 - (float)ease::out(ease::quad)(ramp(t, .48f, .80f))) * 16);
    const char* steps[4] = { "LOG SCALE", "NORMALISE / PQN", "FILL GAPS / KNN ~8%", "STANDARDISE / Z-SCORE" };
    const float sa[4] = { 1.20f, 2.24f, 3.12f, 4.00f };
    for (int i = 0; i < 4; ++i) {
        const float su = ramp(t, sa[i], sa[i] + .35f);
        if (su <= 0) continue;
        const float y = 180 + i * 144;
        boxA(64, y, 760, 122, CREAM, su, false);
        textA(mono(), ("0" + std::to_string(i + 1)).c_str(), 88, y + 20, 20, MUTED, su);
        textA(mono(), steps[i], 150, y + 18, 26, INK, su);
        const float bu = ramp(t, sa[i] + .20f, sa[i] + .60f);
        if (bu > 0) DrawRectangle(144, y + 82, (int)(600 * bu), 8, withAlpha(i == 3 ? HGREEN : HYELLOW, su));
    }

    const float pu = ramp(t, 5.16f, 5.56f);
    if (pu > 0) {
        boxA(880, 180, 976, 500, CREAM, pu, true);
        textA(mono(), "PCA / SCHEMATIC", 908, 204, 20, MUTED, pu);
    }
    const float iu = ramp(t, 7.00f, 7.50f);
    const float sep = 220 - 180 * ramp(t, 10.33f, 12.30f);
    const float cL = 1370 - sep, cR = 1370 + sep;
    if (iu > 0) {
        for (int k = 0; k < 2; ++k) {
            const float cx = k ? cR : cL;
            for (int i = 0; i < 38; ++i) {
                const float a = i * 2.399f, r = 40 + (i % 9) * 10;
                const float x = cx + std::cos(a) * r;
                const float y = 410 + std::sin(a) * r * .7f + ((i * 37) % 30);
                DrawCircle((int)x, (int)y, 7, withAlpha(k ? HGREEN : HPINK, iu));
            }
            textA(mono(), k ? "ST001517" : "ST001516", cx - 50, 530, 19, INK, iu);
        }
    }
    const float gu = ramp(t, 7.80f, 8.20f);
    textA(mono(), t < 12.3f ? "PC1 GAP 19.33" : "AFTER COMBAT 0.04", 930, 590, 22, INK, gu);
    textA(mono(), "COMBAT: DIAGNOSTIC ONLY - NOT USED IN THE PREDICTIVE PIPELINE", 908, 646, 18, MUTED, ramp(t, 13.61f, 14.00f));

    const float hu = ramp(t, 12.40f, 12.80f);
    if (hu > 0) {
        boxA(880, 704, 976, 154, CREAM, hu, false);
        textA(mono(), "HISTOGRAM / BEFORE -> AFTER", 908, 726, 18, MUTED, hu);
        const float before = ramp(t, 12.60f, 13.20f);
        const float copy = ramp(t, 13.10f, 13.45f);
        const float move = (float)ease::out(ease::quad)(ramp(t, 13.35f, 14.20f));
        const float morph = (float)ease::out(ease::quad)(ramp(t, 14.00f, 15.10f));
        for (int i = 0; i < 12; ++i) {
            const float h1 = 18 + (i < 6 ? 80 - i * 8 : 24 + i * 2);
            const float h2 = 18 + 44 * std::exp(-std::pow((i - 5.5f) / 3.2f, 2));
            const float bx = 916 + i * 30, ax = 1270 + i * 30;
            if (before > 0) DrawRectangle((int)bx, (int)(838 - h1), 18, (int)h1, withAlpha(INK, .28f * before));
            if (copy > 0) {
                const float x = bx + (ax - bx) * move;
                const float h = h1 + (h2 - h1) * morph;
                const Color c = ColorLerp(withAlpha(INK, .28f), HYELLOW, copy);
                DrawRectangle((int)x, (int)(838 - h), 18, (int)h, withAlpha(c, copy));
            }
        }
    }
}

void scene07(const Scene&, float t) {
    const float title = ramp(t, .48f, .84f);
    textA(sans(), "BOTH METHODS MUST PICK.", 64, 118, 58, INK, title, (1 - (float)ease::out(ease::quad)(title)) * 16);

    const float ru = ramp(t, 1.70f, 2.05f);
    if (ru > 0) {
        boxA(1136, 200, 720, 590, CREAM, ru, true);
        textA(mono(), "FEATURE SELECTION", 1164, 228, 20, MUTED, ru);
    }
    textBlockA(sans(), "ONE VOTE IS NOT ENOUGH.", 1164, 282, 52, 650, INK,
               ramp(t, 2.05f, 2.45f), 1.08f, 2, 0);

    const float lu = ramp(t, 3.00f, 3.35f);
    if (lu > 0) boxA(64, 200, 1040, 590, CREAM, lu, true);
    const float left = ramp(t, 4.48f, 4.84f);
    const float right = ramp(t, 5.28f, 5.64f);
    const float cx1 = 430, cx2 = 700, cy = 480, r = 220;
    if (left > 0) {
        DrawCircle((int)cx1, (int)cy, (int)r, withAlpha(HYELLOW, .25f * left));
        DrawCircleLines((int)cx1, (int)cy, r, withAlpha(INK, left));
        textA(mono(), "LASSO", 300, 250, 24, HORANGE, left);
        textA(sans(), "55", 320, 468, 72, INK, left);
        textA(mono(), "5-FOLD", 320, 560, 18, MUTED, left);
        check(1164, 540); textA(mono(), "LINEAR", 1210, 540, 22, INK, left);
    }
    if (right > 0) {
        DrawCircle((int)cx2, (int)cy, (int)r, withAlpha(HGREEN, .25f * right));
        DrawCircleLines((int)cx2, (int)cy, r, withAlpha(INK, right));
        textA(mono(), "RANDOM FOREST", 650, 250, 24, HGREEN, right);
        textA(sans(), "33", 770, 468, 72, INK, right);
        textA(mono(), "5-FOLD", 770, 560, 18, MUTED, right);
        check(1164, 596); textA(mono(), "TREE-BASED", 1210, 596, 22, INK, right);
    }

    const float both = ramp(t, 7.00f, 7.36f);
    textA(sans(), "20", 545, 468, 72, INK, both);
    textA(mono(), "SHARED", 525, 560, 18, MUTED, both);
    textA(mono(), "BOTH", 545, 588, 18, HPINK, both);
    if (both > 0) stamp("YES / YES = KEEP", 500, 660 + (1 - both) * 14, 25, HGREEN, INK, -4);

    textBlockA(mono(), "Two independent reviewers. A metabolite survives only if both say yes.",
               1164, 410, 24, 630, INK, ramp(t, 7.55f, 7.95f), 1.25f, 3);
    textA(mono(), "FINAL POOLED SET: 11", 1164, 700, 22, HGREEN, ramp(t, 8.80f, 9.20f));
    textA(mono(), "ILLUSTRATIVE: ONE TRAINING DATASET / ACTUAL FOLDS 20 + 34 / FINAL 11", 88, 820, 17, MUTED, ramp(t, 8.80f, 9.20f));
}
void scene08(const Scene&, float t) {
    textA(sans(), "TRAIN ON ONE. TEST ON THE OTHER.", 64, 118, 54, INK, ramp(t, .48f, .80f), (1 - (float)ease::out(ease::quad)(ramp(t, .48f, .80f))) * 16);
    textA(mono(), "LEAVE-ONE-DATASET-OUT", 64, 180, 20, MUTED, ramp(t, 1.20f, 1.50f));
    for (int lane = 0; lane < 2; ++lane) {
        const float base = lane == 0 ? 2.27f : 6.35f;
        const float y = 250 + lane * 270;
        const char* a = lane == 0 ? "ST001516" : "ST001517";
        const char* b = lane == 0 ? "ST001517" : "ST001516";
        const float tu = ramp(t, base, base + .35f);
        if (tu > 0) {
            boxA(64, y, 650, 190, CREAM, tu, lane == 0);
            textA(mono(), "TRAIN", 92, y + 22, 20, HGREEN, tu);
            textA(mono(), a, 92, y + 64, 38, INK, tu);
            const float foldStart = lane == 0 ? 2.47f : 6.10f;
            const float step = lane == 0 ? .12f : .06f;
            const float span = lane == 0 ? .20f : .14f;
            for (int i = 0; i < 5; ++i) {
                const float u = ramp(t, foldStart + i * step, foldStart + i * step + span);
                if (u <= 0) continue;
                const float e = (float)ease::out(ease::back(1.25))(u);
                const float from = 92.f + (i - 1) * 40.f, to = 92.f + i * 40.f;
                const float x = i == 0 ? to : from + (to - from) * e;
                DrawRectangle((int)x, (int)(y + 128), 32, 32, withAlpha(HYELLOW, .16f * u));
                DrawRectangleLinesEx({ x, y + 128, 32, 32 }, 3, withAlpha(INK, u));
            }
            textA(mono(), "TUNE / 5-FOLD HERE", 330, y + 134, 18, MUTED, tu);
        }
        const float testAt = lane == 0 ? 3.95f : 6.65f;
        const float au = ramp(t, testAt - .25f, testAt + .05f);
        if (au > 0) arrow(730, y + 95, 876, y + 95, withAlpha(INK, au));
        const float xu = ramp(t, testAt, testAt + .35f);
        if (xu > 0) {
            boxA(900, y, 620, 190, CREAM, xu, false);
            textA(mono(), "TEST / LOCKED", 928, y + 22, 20, HPINK, xu);
            textA(mono(), b, 928, y + 64, 38, INK, xu);
        }
    }
    const float u = ramp(t, 9.60f, 10.00f);
    if (u > 0) stamp("CLEANING + TUNING FIT ONLY ON TRAIN", 64, 790 + (1 - u) * 12, 22, HYELLOW, INK, -3);
    textA(mono(), "2 FOLDS / DESCRIPTIVE / INNER-CV IS OPTIMISTIC", 1020, 800, 18, MUTED, ramp(t, 9.90f, 10.30f));
    textA(mono(), "NO PEEKING", 1210, 380, 18, HPINK, ramp(t, 11.08f, 11.40f));
}

void scene09(const Scene&, float t) {
    textA(sans(), "PRACTICE TEST vs REAL EXAM.", 64, 118, 56, INK, ramp(t, .48f, .80f), (1 - (float)ease::out(ease::quad)(ramp(t, .48f, .80f))) * 16);
    textA(mono(), "AUC-ROC / 0.5 = COIN FLIP / 1.0 = PERFECT", 68, 190, 20, MUTED, ramp(t, 1.00f, 1.30f));
    const float pu = ramp(t, 1.20f, 1.55f);
    if (pu > 0) boxA(64, 224, 1792, 650, CREAM, pu, true);
    const float x0 = 250, x1 = 1680, y0 = 620;
    auto X = [&](double auc) { return x0 + (float)((auc - .5) / .5) * (x1 - x0); };
    const float au = ramp(t, 1.60f, 1.95f);
    if (au > 0) {
        DrawLineEx({ x0, y0 }, { x1, y0 }, 4, withAlpha(INK, au));
        const double ticks[6] = { .5, .6, .7, .8, .9, 1.0 };
        for (double v : ticks) {
            DrawLineEx({ X(v), y0 }, { X(v), y0 + 16 }, 3, withAlpha(INK, au));
            char b[16]; std::snprintf(b, sizeof b, "%.1f", v);
            textC(mono(), b, X(v), y0 + 26, 18, withAlpha(MUTED, au));
        }
        DrawLineEx({ X(.5), 320 }, { X(.5), 680 }, 3, withAlpha(INK, .4f * au));
        textC(mono(), "COIN FLIP", X(.5), 660, 17, withAlpha(MUTED, au));
    }
    textA(mono(), "PRACTICE TEST / INNER-CV", 96, 278, 20, HGREEN, ramp(t, 2.40f, 2.80f));
    textA(mono(), "REAL EXAM / HELD-OUT", 96, 474, 20, HPINK, ramp(t, 7.81f, 8.15f));
    const float nu = ramp(t, 2.30f, 2.65f);
    if (nu > 0) {
        for (float y = 310; y < 620; y += 18) {
            DrawLineEx({ X(.99), y }, { X(.99), std::min(620.f, y + 10) }, 2, withAlpha(INK, .55f * nu));
        }
        textA(mono(), "0.99", X(.99) - 22, 292, 18, MUTED, nu);
    }
    const double inner[4] = { .931, .956, .981, .990 };
    const double held[10] = { .569, .647, .499, .633, .646, .638, .587, .574, .699, .644 };
    for (int i = 0; i < 4; ++i) {
        const float u = ramp(t, 2.40f + i * .22f, 2.80f + i * .22f);
        if (u > 0) DrawCircle((int)X(inner[i]), (int)(330 + (i % 3) * 18), 13, withAlpha(HGREEN, u));
    }
    const float iu = ramp(t, 9.55f, 10.00f);
    if (iu > 0) {
        DrawRectangle((int)X(.5), 310, (int)(X(.7) - X(.5)), 310, withAlpha(HPINK, .07f * iu));
        DrawRectangleLinesEx({ X(.5), 310, X(.7) - X(.5), 310 }, 2, withAlpha(HPINK, .30f * iu));
    }
    for (int i = 0; i < 10; ++i) {
        const float u = ramp(t, 7.81f + i * .15f, 8.21f + i * .15f);
        if (u > 0) DrawCircle((int)X(held[i]), (int)(488 + (i % 4) * 22), 12, withAlpha(HPINK, u));
    }
    const float gu = ramp(t, 19.60f, 20.00f);
    if (gu > 0) {
        DrawRectanglePro({ 1040, 250 + (1 - gu) * 18, 330, 54 }, { 0, 0 }, -4, withAlpha(HYELLOW, gu));
        DrawTextPro(mono(), "GAP 0.293-0.357", { 1062, 264 + (1 - gu) * 18 }, { 0, 0 }, -4, 23, 0, withAlpha(INK, gu));
    }
    const float mu = ramp(t, 16.90f, 17.30f);
    if (mu > 0) {
        DrawLineEx({ X(.64), 380 }, { X(.64), 600 }, 4, withAlpha(INK, .5f * mu));
        textA(mono(), "~0.64", X(.64) - 40, 350, 28, INK, mu);
        textA(mono(), "BEST AVERAGES: LR 0.642 / TUNED XGB 0.643", 1060, 710, 18, MUTED, mu);
    }
    textA(mono(), "2 FOLDS / NO RANKING CLAIM", 96, 768, 19, MUTED, ramp(t, 20.00f, 20.40f));
    textA(mono(), "THE GAP IS THE FINDING", 1310, 768, 22, HPINK, ramp(t, 20.93f, 21.30f));
}

void scene10v2(const Ctx& cx, float t, const Specimens& sp) {
    textA(sans(), "ELEVEN FOR REVIEW.", 64, 118, 58, INK, ramp(t, .48f, .80f), (1 - (float)ease::out(ease::quad)(ramp(t, .48f, .80f))) * 16);
    textA(mono(), "220 -> 11 / THREE STRONGEST / LITERATURE-COMPATIBLE, NOT NEW", 68, 188, 19, MUTED, ramp(t, 2.80f, 3.20f));
    const int ft = (int)(t * FPS);
    const float at[3] = { 6.05f, 8.29f, 10.13f };
    const Specimen* sps[3] = { &sp.phe, &sp.s1p, &sp.ret };
    const char* names[3] = { "PHENYLALANINE", "SPHINGOSINE-1-PHOSPHATE", "RETINOL (VITAMIN A)" };
    const char* coefs[3] = { "+1.214", "-1.121", "-0.944" };
    for (int i = 0; i < 3; ++i) {
        const float u = std::clamp((t - at[i]) / .42f, 0.f, 1.f);
        if (u <= 0) continue;
        const float e = (float)ease::out(ease::back(1.2))(u);
        const float x = 64.f + i * 580.f, y = 220.f + (1 - e) * 36.f;
        drawSpecimen(cx, ft, *sps[i], { x, y, 500, 320 }, names[i], "");
        textA(mono(), coefs[i], x + 14, y + 366, 22, INK, u);
        const bool up = i == 0;
        const Color col = up ? HORANGE : HPINK;
        if (up) arrow(x + 230, y + 396, x + 230, y + 360, withAlpha(col, u), 4);
        else arrow(x + 230, y + 360, x + 230, y + 396, withAlpha(col, u), 4);
        textA(sans(), up ? "UP" : "DOWN", x + 252, y + 368, 28, col, u);
    }
    textA(mono(), "20:1 FEATURE REDUCTION / NOT A PERFORMANCE GAIN", 64, 650, 18, MUTED, ramp(t, 10.90f, 11.30f));
    const char* rest[8] = { "3-UREIDOPROPIONATE", "TAURINE", "OTHER 1", "OTHER 2", "OTHER 3", "OTHER 4", "OTHER 5", "OTHER 6" };
    for (int i = 0; i < 8; ++i) {
        const float u = std::clamp((t - 11.60f - i * .10f) / .28f, 0.f, 1.f);
        if (u <= 0) continue;
        const float a = std::min(1.f, u * 2.5f);
        const float x = 64 + (i % 4) * 450, y = 704 + (i / 4) * 58;
        box(x, y, 420, 44, withAlpha(INK, .06f * a));
        textA(mono(), rest[i], x + 10, y + 12, 14, withAlpha(INK, .65f), a);
    }
    textA(mono(), "LOPANSRI 2006 / DHANGADAMAJHI 2020 / RAZA 2009", 64, 836, 16, MUTED, ramp(t, 13.03f, 13.43f));
    textA(mono(), "TOP-5 SHAP LISTS DIFFERED BETWEEN FOLDS", 64, 862, 16, MUTED, ramp(t, 13.30f, 13.70f));
}

void drawIsNotRows(const Ctx& cx, int t) {
    const double sec = frameTime(t) / (double)FPS;
    const char* yes[4] = { "A transparent, reusable cross-dataset workflow",
        "An honest report of a modest result", "11 candidates for expert review", "Open code" };
    const char* no[4] = { "A validated biomarker panel", "A diagnostic", "New knowledge about Indonesian malaria", "A commercial clinical product" };
    const float yAppear[4] = { .92f, 3.64f, 4.04f, 4.92f };
    const float nAppear[4] = { 6.09f, 7.61f, 10.65f, 11.32f };
    for (int i = 0; i < 4; ++i) {
        const float yu = std::clamp((float)((sec - yAppear[i]) / .42f), 0.f, 1.f);
        const float nu = std::clamp((float)((sec - nAppear[i]) / .42f), 0.f, 1.f);
        const float y = 306.f + i * 100.f;
        if (yu > 0) {
            const float e = (float)ease::out(ease::back(1.25))(yu);
            const float a = std::min(1.f, yu * 2.4f);
            check(96, y + (1 - e) * 32, withAlpha(HGREEN, a));
            textBlock(body(), yes[i], 140, y - 2 + (1 - e) * 32, 22, 730, withAlpha(INK, a), 1.2f, 2);
        }
        if (nu > 0) {
            const float e = (float)ease::out(ease::back(1.25))(nu);
            const float a = std::min(1.f, nu * 2.4f);
            cross(1028, y + (1 - e) * 32, withAlpha(HPINK, a));
            textBlock(body(), no[i], 1072, y - 2 + (1 - e) * 32, 22, 730, withAlpha(INK, a), 1.2f, 2);
        }
    }
}

void scene12(const Ctx& cx, const Scene&, float t) {
    textA(sans(), "A METHOD, NOT A DIAGNOSTIC.", 64, 118, 56, INK, ramp(t, .48f, .80f), (1 - (float)ease::out(ease::quad)(ramp(t, .48f, .80f))) * 16);
    const float lu = ramp(t, .48f, .82f);
    if (lu > 0) boxA(64, 200, 860, 600, CREAM, lu, true);
    const float ru = ramp(t, .75f, 1.10f);
    if (ru > 0) boxA(996, 200, 860, 600, CREAM, ru, false);
    const float hu = ramp(t, 1.00f, 1.35f);
    if (hu > 0) {
        DrawRectangle(64, 200, 860, 60, withAlpha(HGREEN, hu));
        DrawRectangle(996, 200, 860, 60, withAlpha(HPINK, hu));
        textA(mono(), "WHAT IT IS", 92, 218, 22, INK, hu);
        textA(mono(), "WHAT IT IS NOT", 1024, 218, 22, INK, hu);
    }
    const float mu = ramp(t, 10.93f, 11.33f);
    if (mu > 0) {
        drawIndonesiaMap(cx, { 1450, 694, 360, 92 });
        textA(mono(), "INDONESIA / NO DATA HERE", 1072, 766, 16, MUTED, mu);
    }
    textA(mono(), "THE COHORTS ARE WEST AFRICAN CHILDREN", 64, 838, 18, MUTED, ramp(t, 11.70f, 12.10f));
}

void scene11(const Scene&, float t) {
    textA(sans(), "11 VALUES IN. WHY OUT.", 64, 118, 58, INK, ramp(t, .48f, .80f), (1 - (float)ease::out(ease::quad)(ramp(t, .48f, .80f))) * 16);
    const float pu = ramp(t, 1.28f, 1.63f);
    if (pu > 0) {
        boxA(64, 200, 1792, 610, CREAM, pu, true);
        textA(mono(), "RESEARCH DEMO", 96, 226, 20, MUTED, pu);
    }
    const float vu = ramp(t, 2.16f, 2.52f);
    for (int i = 0; i < 11; ++i) {
        const float u = std::clamp((t - 2.16f - i * .04f) / .22f, 0.f, 1.f);
        if (u <= 0) continue;
        const float x = 96 + (i % 4) * 130, y = 280 + (i / 4) * 90;
        boxA(x, y, 112, 66, withAlpha(INK, .05f), u, false);
        textA(mono(), "VAL", x + 10, y + 10, 14, MUTED, u);
        textA(mono(), ("M" + std::to_string(i + 1)).c_str(), x + 10, y + 30, 20, INK, u);
    }
    const float au = ramp(t, 3.28f, 3.58f);
    if (au > 0) arrow(660, 500, 800, 500, withAlpha(INK, au));
    const float mu = ramp(t, 3.45f, 3.80f);
    if (mu > 0) {
        boxA(820, 300, 300, 360, withAlpha(HGREEN, .18f), mu, false);
        textC(mono(), "MODEL", 970, 460, 28, withAlpha(INK, mu));
        textC(mono(), "11 -> PREDICTION", 970, 500, 18, withAlpha(MUTED, mu));
    }
    const float ao = ramp(t, 4.00f, 4.30f);
    if (ao > 0) arrow(1140, 500, 1280, 500, withAlpha(INK, ao));
    const float ou = ramp(t, 4.24f, 4.58f);
    if (ou > 0) {
        boxA(1300, 300, 500, 360, CREAM, ou, false);
        textA(mono(), "PREDICTION + WHY", 1324, 326, 20, INK, ou);
    }
    for (int i = 0; i < 5; ++i) {
        const float u = ramp(t, 4.40f + i * .15f, 4.70f + i * .15f);
        if (u <= 0) continue;
        const float w = 80 + (i * 53) % 240;
        DrawRectangle(1324, 380 + i * 48, (int)(w * u), 24, withAlpha(i % 2 ? HGREEN : HPINK, u));
        textA(mono(), ("M" + std::to_string(i + 1)).c_str(), 1330 + w + 12, 382 + i * 48, 16, INK, u);
    }
    const float u = ramp(t, 6.45f, 6.85f);
    if (u > 0) stamp("RESEARCH DEMO / NOT A DIAGNOSTIC", 96, 740 + (1 - u) * 12, 22, HYELLOW, INK, -3);
}void approvalStamp(const Ctx& cx, float x, float y, float scale, float alpha, float rot, float split = 0) {
    (void)cx;
    const float w = mw(mono(), "PROOF OF CONCEPT", 38) + 80;
    const float h = 84;
    const Color pink = withAlpha(HPINK, alpha), cream = withAlpha(CREAM, alpha);
    DrawRectanglePro({ x, y, w * scale, h * scale }, { 0, 0 }, rot, pink);
    DrawRectanglePro({ x + 7 * scale, y + 7 * scale, (w - 14) * scale, (h - 14) * scale },
                     { 0, 0 }, rot, cream);
    if (split > 0) {
        DrawTextPro(mono(), "PROOF OF CONCEPT", { x + 30 * scale + split, y + 22 * scale },
                    { 0, 0 }, rot, 38 * scale, 0, withAlpha(CREAM, .45f * alpha));
        DrawTextPro(mono(), "PROOF OF CONCEPT", { x + 30 * scale - split, y + 22 * scale },
                    { 0, 0 }, rot, 38 * scale, 0, withAlpha(HYELLOW, .45f * alpha));
    }
    DrawTextPro(mono(), "PROOF OF CONCEPT",
                { x + 30 * scale, y + 22 * scale }, { 0, 0 }, rot, 38 * scale, 0, pink);
}
void drawClosingCards(const Ctx& cx, int t) {
    const double sec = frameTime(t) / (double)FPS;
    const char* steps[3] = { "INDEPENDENT COHORTS", "TARGETED CONFIRMATION", "PROSPECTIVE LOCAL STUDIES" };
    const Color fills[3] = { CREAM, HPINK, HYELLOW };
    const float appear[3] = { .84f, 2.20f, 3.56f };
    const float a = std::min(1.f, (float)ramp((float)sec, .20f, .50f));
    text(mono(), "THE NEXT THREE MOVES", 64, 220, 22, withAlpha(MUTED, a));
    for (int i = 0; i < 3; ++i) {
        const float u = std::clamp((float)((sec - appear[i]) / .42f), 0.f, 1.f);
        if (u <= 0) continue;
        const float e = (float)ease::out(ease::back(1.3))(u);
        const float x = 64.f + i * 604.f;
        const float y = 270.f + (1.f - e) * 72.f;
        const float alpha = std::min(1.f, u * 2.4f);
        if (i == 0) DrawRectangle((int)(x + 12), (int)(y + 12), 560, 220, withAlpha(HORANGE, alpha));
        DrawRectangle((int)x, (int)y, 560, 220, withAlpha(fills[i], alpha));
        DrawRectangleLinesEx({ x, y, 560, 220 }, 6, withAlpha(INK, alpha));
        text(mono(), ("0" + std::to_string(i + 1)).c_str(), x + 24, y + 24, 20, withAlpha(INK, alpha));
        textBlock(sans(), steps[i], x + 24, y + 80, 40, 510, withAlpha(INK, alpha), 1.02f, 2);
    }
}
void scene13(const Ctx& cx, const Scene&, float t) {
    if (t >= 5.47f) {
        DrawRectangle(0, 0, W, H, HGREEN);
        const float u1 = std::clamp((t - 5.63f) / .34f, 0.f, 1.f);
        const float u2 = std::clamp((t - 6.75f) / .34f, 0.f, 1.f);
        const float e1 = (float)ease::out(ease::back(1.5))(u1);
        const float e2 = (float)ease::out(ease::back(1.5))(u2);
        const float a1 = std::min(1.f, u1 * 3.f), a2 = std::min(1.f, u2 * 3.f);
        const float hit = t - 7.55f;
        float sh = 0;
        if (hit >= 0 && hit < .28f) sh = 14.f * (1.f - hit / .28f);
        const int frame = (int)(t * FPS);
        const float sx = sh * ((float)rnd01(101, frame) - .5f);
        const float sy = sh * ((float)rnd01(102, frame) - .5f);
        text(sans(), "MODEST RESULT.", 96 + sx, 220 + sy + (1 - e1) * 24, 136 * (1.16f - .16f * e1), withAlpha(CREAM, a1));
        text(sans(), "HONEST METHOD.", 96 + sx, 370 + sy + (1 - e2) * 24, 136 * (1.16f - .16f * e2), withAlpha(CREAM, a2));
        const float stampU = std::clamp((t - 7.55f) / .20f, 0.f, 1.f);
        if (stampU > 0) {
            const float e = (float)ease::in(ease::quad)(stampU);
            const float w2 = mw(sans(), "HONEST METHOD.", 136);
            const float x = 96 + w2 - 14;
            const float y = 370 + 52;
            float scale = 2.4f - 1.4f * e;
            if (hit > .20f) scale = 1.f + .05f * std::exp(-(hit - .20f) * 5.f) * std::sin((hit - .20f) * 30.f);
            const float alpha = std::min(1.f, stampU * 4.f);
            const float split = (hit >= 0 && hit < .12f) ? 4.f * (1.f - hit / .12f) : 0.f;
            if (hit >= 0 && hit < .30f) {
                const float ru = std::clamp(hit / .30f, 0.f, 1.f);
                const float cx0 = x + w2 * 0.15f, cy0 = y + 42;
                dr::ring(cx, { cx0, cy0 }, 80 + 260 * ru, 4, withAlpha(CREAM, .35f * (1 - ru)));
                dr::ring(cx, { cx0, cy0 }, 40 + 180 * ru, 3, withAlpha(HPINK, .35f * (1 - ru)));
            }
            approvalStamp(cx, x + sx, y + sy - 30 * (1 - e), scale, alpha, -12 + 6 * e, split);
        }
        textA(mono(), "PRASETYO & RUSDI  /  PT GALENIC SYSTEMS INDONESIA  /  UNIVERSITAS ISLAM BANDUNG", 96, 760, 19, CREAM, ramp(t, 8.00f, 8.35f));
        textA(mono(), "DATA: METABOLOMICS WORKBENCH PR000960  /  ABDRABOU ET AL., NAT METAB 2021", 96, 796, 17, withAlpha(CREAM, .8f), ramp(t, 8.20f, 8.55f));
        textA(mono(), "GALENIC-SYSTEMS/MALARIA-METABOLOMICS-DEMO", 96, 832, 19, HYELLOW, ramp(t, 8.40f, 8.75f));
        textA(mono(), "MADE WITH RAYMOTION", 150, 896, 17, withAlpha(CREAM, .72f), ramp(t, 8.00f, 8.35f));
    }
}

void drawFilm(const Ctx& cx, int frame, const std::shared_ptr<std::vector<Scene>>& scenes,
              const std::shared_ptr<Specimens>& sp) {
    const float t = frame / (float)FPS;
    int ch = 0;
    while (ch + 1 < N && t >= (*scenes)[ch + 1].start / (float)FPS) ++ch;
    const Scene& s = (*scenes)[ch];
    const float local = t - s.start / (float)FPS;
    const float vt = local + .28f;
    const bool green = ch == N - 1 && vt >= 5.47f;
    ClearBackground(green ? HGREEN : CREAM);
    if (!green) {
        topbar(s);
        footer(ch, local);
    }
    switch (ch) {
    case 0:
        scene01(s, vt);
        if (vt > 2.05f)
            drawSpecimen(cx, (int)(vt * FPS) + 90, sp->phe, { 1250, 190, 600, 590 },
                         "SERUM MOLECULE", "ILLUSTRATIVE / 3D");
        break;
    case 1: scene02(s, vt); break;
    case 2: scene03v2(cx, vt, *sp); break;
    case 3: scene04(s, vt); break;
    case 4: scene05(cx, s, vt); break;
    case 5: scene06(s, vt); break;
    case 6: scene07(s, vt); break;
    case 7: scene08(s, vt); break;
    case 8: scene09(s, vt); break;
    case 9: scene10v2(cx, vt, *sp); break;
    case 10: scene11(s, vt); break;
    case 11: scene12(cx, s, vt); break;
    default: scene13(cx, s, vt); break;
    }
    if (!green) caption(s, local, green);
}

// ------------------------------------------------------------------ thumbnail
void drawThumb(const Ctx& cx, const Specimens& sp) {
    ClearBackground(CREAM);
    text(sans(), "DOES IT", 72, 150, 132, INK);
    text(sans(), "TRAVEL?", 72, 280, 132, HPINK);
    text(mono(), "CROSS-DATASET MALARIA METABOLOMICS", 78, 492, 23, MUTED);
    textBlock(mono(), "A model can score 0.99 on its practice test. On unseen data: 0.50-0.70. That gap is the finding.",
              78, 542, 27, 900, INK, 1.2f, 3);
    drawSpecimen(cx, 90, sp.phe, { 1120, 150, 700, 600 },
                 "MOLECULAR SPECIMEN", "ILLUSTRATIVE / 3D");
    text(mono(), "PRACTICE 0.99 / REAL 0.64", 1120, 840, 24, INK);
    text(mono(), "THE GAP IS THE FINDING", 1120, 878, 20, HPINK);
    text(mono(), "PROOF OF CONCEPT / INTERNAL PREVIEW", 72, 920, 19, MUTED);
    text(mono(), "PRASETYO & RUSDI / SUPPLIED BRIEF", 72, 964, 18, MUTED);
}

} // namespace

void registerMalariaMetabolomicsV2(Registry& out) {
    auto scenes = std::make_shared<std::vector<Scene>>(makeScenes());
    prefab::add({
        "padlock",
        Json::parse(R"({"size": 58, "color": "#151515", "bg": "#f2e9d6"})"),
        [](TNode& n, const Json& p) {
            const float size = (float)p.num("size", 58);
            const Color col = p.color("color", INK), bg = p.color("bg", CREAM);
            n.fn = [size, col, bg](const Ctx& c, int t) {
                const float u = std::clamp(t / 10.f, 0.f, 1.f);
                if (u <= 0) return;
                const float e = (float)ease::out(ease::back(1.35))(u);
                const float s = size * (.72f + .28f * e);
                const Color ink = withAlpha(col, u), hole = withAlpha(bg, u);
                const float cy = -s * .18f, r = s * .24f, post = s * .075f;
                dr::arc(c, { 0, cy }, r, post, 180, 360, ink);
                dr::rect(c, -r - post * .5f, cy, post, s * .15f, ink);
                dr::rect(c, r - post * .5f, cy, post, s * .15f, ink);
                dr::roundRect(c, -s * .42f, -s * .03f, s * .84f, s * .56f, s * .12f, ink);
                dr::circle(c, { 0, s * .18f }, s * .055f, hole);
                dr::rect(c, -s * .02f, s * .18f, s * .04f, s * .13f, hole);
            };
        },
    });
    auto spec = std::make_shared<Specimens>();
    spec->phe = makeSpecimen("data/malaria/phenylalanine.sdf");
    spec->s1p = makeSpecimen("data/malaria/sphingosine_1-phosphate.sdf", .80f, 0.f);
    spec->ret = makeSpecimen("data/malaria/retinol.sdf");
    const int total = scenes->empty() ? 1 : (*scenes)[N - 1].start + (*scenes)[N - 1].len;
    Comp c;
    c.name = "malaria-metabolomics-v2";
    c.w = W; c.h = H; c.fps = FPS; c.duration = total; c.background = CREAM;
    c.assets = [scenes](const Comp&, const Json&) {
        sub::Track tr;
        for (const Scene& s : *scenes) {
            for (const Line& l : s.lines) {
                const double from = (s.start + l.at) / (double)FPS;
                const double to = (s.start + l.at + l.frames) / (double)FPS;
                tr.cues.push_back({ from, to, l.text, false });
            }
        }
        sub::writeSrt("out/malaria-metabolomics-v2.srt", tr);
    };
    TNode* root = addRoot(c.roots, 0, total);
    root->fn = [scenes, spec](const Ctx& cx, int t) { drawFilm(cx, t, scenes, spec); };
    const Scene& limits = (*scenes)[N - 2];
    const Scene& last = (*scenes)[N - 1];
    TNode* rows = addRoot(c.roots, limits.start, limits.len);
    rows->isolate().motionBlur = { 16, .5f };
    rows->fn = [](const Ctx& cx, int t) { drawIsNotRows(cx, t); };
    TNode* cards = addRoot(c.roots, last.start, 5 * FPS);
    cards->isolate().motionBlur = { 16, .5f };
    cards->fn = [](const Ctx& cx, int t) { drawClosingCards(cx, t); };
    const int logoAt = last.start + (int)std::lround(8.00f * FPS);
    TNode* brand = prefab::use(c.roots, logoAt, total - logoAt, "raymotion-logo",
        Json::parse(R"({"size": 34, "wordmark": false, "tagline": "", "in": "draw", "length": 28, "ink": "#f2e9d6", "accent": "#ffc365"})"));
    brand->props.pos = { 105.f, 892.f };

    const Scene& s8 = (*scenes)[7];
    for (int lane = 0; lane < 2; ++lane) {
        const int at = s8.start + (int)std::lround((lane == 0 ? 4.02f : 6.72f) * FPS);
        TNode* lock = prefab::use(c.roots, at, s8.start + s8.len - at, "padlock");
        lock->props.pos = { 1110.f, lane == 0 ? 370.f : 640.f };
    }

    auto sfx = [&](const std::string& file, int frame, float volume) {
        std::string p;
        try { p = media::resolve(file); } catch (...) { return; }
        media::AudioOptions o; o.volume = volume * .35f;
        media::audio(addRoot(c.roots, frame, 45), p, o);
    };
    for (int i = 0; i < N; ++i) {
        const Scene& s = (*scenes)[i];
        for (const Line& l : s.lines) {
            std::string voice;
            try { voice = media::resolve(VO_DIR + l.file); } catch (...) {}
            if (!voice.empty()) {
                media::AudioOptions vo; vo.volume = .75f;
                media::audio(addRoot(c.roots, s.start + l.at, l.frames + 2), voice, vo);
            }
        }
        if (i == 0) { sfx(SFX_DIR + "stamp.wav", s.start + 8, .3f); sfx(SFX_DIR + "lowhit.wav", s.start + 78, .45f); }
        else if (i == 1) { sfx(SFX_DIR + "stamp.wav", s.start + 64, .25f); sfx(SFX_DIR + "stamp.wav", s.start + 162, .25f); sfx(SFX_DIR + "stamp.wav", s.start + 360, .25f); }
        else if (i == 2) { sfx(POP, s.start + 20, .4f); sfx(POP, s.start + 60, .4f); sfx(SFX_DIR + "whoosh.wav", s.start + 120, .35f); }
        else if (i == 3) { sfx(SFX_DIR + "whoosh.wav", s.start + 40, .3f); sfx(SFX_DIR + "lowhit.wav", s.start + 160, .35f); }
        else if (i == 4) { sfx(SFX_DIR + "whoosh.wav", s.start + 20, .25f); sfx(SFX_DIR + "whoosh.wav", s.start + 100, .25f); }
        else if (i == 5) { for (int k = 0; k < 4; ++k) sfx(POP, s.start + 30 + k * 45, .28f); sfx(SFX_DIR + "whoosh.wav", s.start + 220, .3f); }
        else if (i == 6) { sfx(POP, s.start + 30, .3f); sfx(POP, s.start + 160, .3f); sfx(SFX_DIR + "lowhit.wav", s.start + 230, .35f); }
        else if (i == 7) { sfx(SFX_DIR + "lowhit.wav", s.start + 40, .4f); sfx(SFX_DIR + "whoosh.wav", s.start + 180, .3f); }
        else if (i == 8) { sfx(SFX_DIR + "chime.wav", s.start + 40, .25f); sfx(POP, s.start + 180, .25f); }
        else if (i == 9) { sfx(SFX_DIR + "whoosh.wav", s.start + 40, .3f); sfx(POP, s.start + 180, .28f); sfx(POP, s.start + 250, .28f); }
        else if (i == 10) { sfx(POP, s.start + 30, .3f); sfx(POP, s.start + 100, .3f); }
        else if (i == 11) {
            sfx(POP, s.start + 45, .30f);
            sfx(POP, s.start + 81, .28f);
            sfx(POP, s.start + 117, .28f);
            sfx(POP, s.start + 153, .28f);
            sfx(SFX_DIR + "chime.wav", s.start + 30, .25f);
            sfx(SFX_DIR + "lowhit.wav", s.start + 200, .30f);
        }
        else {
            sfx(POP, s.start + 25, .30f);
            sfx(POP, s.start + 66, .28f);
            sfx(POP, s.start + 107, .28f);
            sfx(SFX_DIR + "whoosh.wav", s.start + 156, .25f);
            sfx(SFX_DIR + "stamp.wav", s.start + 218, .45f);
            sfx(SFX_DIR + "stamp.wav", s.start + 227, .35f);
            sfx(HIT_DIR + "impact.wav", s.start + 218, .50f);
            sfx(HIT_DIR + "stamp.wav", s.start + 218, .40f);
            sfx(SFX_DIR + "lowhit.wav", s.start + 218, .30f);
        }
    }
    out.push_back(std::move(c));

    Comp t;
    t.name = "malaria-metabolomics-v2-thumb";
    t.w = W; t.h = H; t.fps = FPS; t.duration = 60; t.background = CREAM;
    TNode* tn = addRoot(t.roots, 0, 60);
    tn->fn = [spec](const Ctx& cx, int) { drawThumb(cx, *spec); };
    out.push_back(std::move(t));
}
} // namespace rmtn
