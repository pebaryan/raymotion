// flora.cpp -- "flora": an engine showcase (not a film) of the 0.12 vegetation:
//   flora::addGrass / g3d::Field   a meadow in gusting wind
//   flora::addForest               a landscape fly-over: thousands of trees, levels and impostors
//   flora::addGrowingTree          a tree from a sapling, grass coming up round it
//   flora2d::                      a 2D hillside in layers
// The meadow, the tree and the hillside are the built-in prefabs (flora-meadow,
// flora-tree-grow, flora-2d-hill); the fly-over is built here.
// The score is generated (procgen::Score) into out/flora-assets-v2 by the composition's assets.
//   raymotion render flora
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"

#include <filesystem>

namespace rmtn {
namespace {

constexpr int FPS = 30, LEN = 210, XF = 20;
const Color INK = mkColor(0xf2f4ff), SUB = mkColor(0xb4bfcc);

Font bold() { return fonts::ui(true); }
Font regular() { return fonts::ui(false); }

// ---------------------------------------------------------------- the score

const std::string SCORE = "out/flora-assets-v2/score.wav";

// 72 bpm in G major, a bar per chord (G, Em, C, D). Under it all, wind: band-passed noise
// that swells with gusts, strongest in the meadow and the fly-over. The meadow has birds
// (bell pairs); a riser carries into the fly-over; while the oak grows, an arpeggio climbs
// an octave; the 2D hill gets lighter plucks; it ends on G. Chapters start at 0, 6.33,
// 12.67 and 19 s (210-frame scenes, 20-frame transitions).
void genScore(const std::string& path, int frames) {
    using procgen::Score;
    const double end = frames / (double)FPS, bar = 4 * 60.0 / 72, stop = end - 2.6;
    const double ch[4] = { 0, 190.0 / FPS, 380.0 / FPS, 570.0 / FPS };
    Score s(end + 0.5);
    struct Chord { int root; int pad[3]; };
    const Chord prog[4] = { { 43, { 62, 67, 71 } }, { 40, { 64, 67, 71 } }, { 36, { 60, 64, 67 } }, { 38, { 62, 66, 69 } } };
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& c = prog[i % 4];
        const bool fly = t >= ch[1] && t < ch[2];
        for (int q : c.pad) s.pad(t, bar * 0.98, q, 0.06f, fly ? 1300.f : 800.f, 0.5f);
        s.pad(t, bar * 0.98, c.pad[0] - 12, 0.04f, 500, 0.5f);
        s.bass(t, bar * 0.9, c.root, 0.12f);
        if (t >= ch[3])                                             // the 2D hill: light plucks
            for (int e = 0; e < 4; e++)
                if (rnd01(7, (unsigned long long)(i * 4 + e)) < 0.7) s.pluck(t + e * bar / 4, c.pad[e % 3] + 12, 0.03f, e % 2 ? 0.3f : 0.7f, 6);
    }
    // wind: a gust every few seconds, louder in the open chapters
    {
        procgen::Svf f;
        const int n = (int)(end * Score::RATE);
        for (int k = 0; k < n; k++) {
            const double t = (double)k / Score::RATE;
            const float open = t < ch[2] ? 1.f : (t < ch[3] ? 0.55f : 0.35f);
            const float gust = 0.55f + 0.45f * (float)std::sin(t * 1.1 + 0.8 * std::sin(t * 0.37));
            const float fade = (float)std::min({ 1.0, t / 1.5, (end - t) / 2.0 });
            const float x = f.bp(s.noise(), 380 + 520 * gust, 0.6f, Score::RATE);
            s.put(k, x * 0.065f * gust * open * std::max(0.f, fade), 0.5f + 0.2f * (float)std::sin(t * 0.5), 0.3f, 0);
        }
    }
    for (int k = 0; k < 14; k++) {                                  // birds in the meadow
        const double t = 0.8 + (ch[1] - 1.2) * rnd01(11, (unsigned long long)k);
        const int note = 88 + (int)(rnd01(12, (unsigned long long)k) * 6);
        const float pan = (float)rnd01(13, (unsigned long long)k);
        s.bell(t, note, 0.02f, pan);
        s.bell(t + 0.08, note + 3, 0.014f, pan);
    }
    s.riser(ch[1] - 1.6, ch[1] + 0.4, 0.035f);                      // into the fly-over
    for (int e = 0; e < 16; e++) {                                  // the oak grows: an arpeggio climbs
        const int steps[8] = { 55, 59, 62, 67, 71, 74, 79, 83 };
        const double t = ch[2] + 10.0 / FPS + e * (150.0 / FPS) / 16;
        s.pluck(t, steps[std::min(7, e / 2)], 0.045f, e % 2 ? 0.35f : 0.65f, 4);
    }
    for (int q : prog[0].pad) s.pad(stop, 2.6, q, 0.08f, 900, 0.5f);
    s.bass(stop, 2.6, prog[0].root - 12, 0.14f);
    std::vector<float> mixed = s.mix();          // peaks at 0.9; 0.8 of that keeps the AAC true peak under -1 dB
    for (float& v : mixed) v *= 0.8f;
    media::writeWav(path, mixed, Score::RATE);
}

struct Chapter { std::string title, note, code; };

// Caption card: what the chapter shows, and the call that makes it.
void caption(TNode* n, int len, const Chapter& ch, bool top = false) {
    TNode* card = n->add(12, len - 12);
    card->up = [len](int t, NodeProps& p) {
        const float in = (float)interpolate(t, { 0, 18 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        const float out = (float)interpolate(t, { len - 40.0, len - 22.0 }, { 1, 0 }, clamped());
        p.opacity = in * out;
        p.pos = { 0, 24 * (1 - in) };
    };
    card->fn = [ch, top](const Ctx& cx, int) {
        const float w = std::max({ MeasureTextEx(regular(), ch.code.c_str(), 26, 0).x, MeasureTextEx(bold(), ch.title.c_str(), 60, 0).x,
                                   MeasureTextEx(regular(), ch.note.c_str(), 28, 0).x }) + 80;
        const float y = top ? 64 : 1080 - 64 - 186;
        dr::roundRect(cx, 64, y, w, 186, 18, Color{ 12, 16, 22, 175 });
        dr::text(bold(), cx, ch.title.c_str(), { 104, y + 30 }, 60, 0, INK, 0, 0);
        dr::text(regular(), cx, ch.note.c_str(), { 104, y + 100 }, 28, 0, SUB, 0, 0);
        dr::text(regular(), cx, ch.code.c_str(), { 104, y + 142 }, 26, 0, mkColor(0xffd27a), 0, 0);
    };
}

// A valley of mixed woodland, flown over low: thousands of trees, each drawn as the full
// tree, a reduced one or an impostor by its distance, with grass near the camera. Returns
// how many trees.
size_t flyover(TNode* n, int len) {
    auto s = std::make_shared<g3d::Scene>();
    s->sky.enabled = true;
    s->sky.zenith = mkColor(0x4f7fbf);
    s->sky.horizon = mkColor(0xe6e2d2);
    s->sky.sunDirection = { -0.6f, 0.35f, -0.7f };
    s->sky.sunLight = 0;
    s->fogFromSky = true;
    s->fogNear = 120;
    s->fogFar = 520;
    s->ambient = 0.5f;
    s->shadowMapSize = 2048;
    g3d::Light sun = g3d::Light::directional({ -60, 40, -70 }, mkColor(0xfff1dc), 2.1f, true);
    sun.shadowExtent = 90;
    s->lights.push_back(sun);
    s->camera.fov = 55;
    s->camera.farPlane = 900;

    const float R = 320;
    flora::Area land = flora::rect({ -R, -R }, { R, R });
    // a valley along z: hills either side, the floor rising gently ahead
    auto height = [](float x, float z) {
        return 0.0016f * x * x * 0.6f + 22 * flora::noise(x, z, 140, 7) + 5 * flora::noise(x, z, 35, 8) + 0.02f * -z;
    };
    land.ground = height;
    g3d::Material ground;
    ground.color = mkColor(0x5b6a3a);
    ground.roughness = 1;
    s->add(flora::terrain(land, 200, 200), ground);
    flora::addGrass(*s, land, flora::meadow(), 3);

    std::vector<flora::TreePtr> trees;
    for (const char* name : { "oak", "birch", "pine", "spruce" }) trees.push_back(flora::makeTree(flora::tree(name)));
    flora::ScatterSpec sc;
    sc.spacing = 5.5f;
    sc.clumping = 0.55f;
    sc.clumpScale = 60;
    sc.variants = (int)trees.size();
    std::vector<flora::Placement> at = flora::scatter(land, sc, 11);
    // keep the flight line clear: a strip of meadow down the valley floor
    at.erase(std::remove_if(at.begin(), at.end(), [](const flora::Placement& p) { return std::fabs(p.position.x) < 14; }), at.end());
    flora::ForestSpec f;
    f.wind = 0.8f;
    f.fps = FPS;
    flora::addForest(*s, at, trees, f);

    s->camera.up = [height, len](int t, g3d::Camera& cam) {
        const float u = (float)frameTime(t) / len;
        const float z = 260 - 330 * u, x = 6 * std::sin(u * 3.0f);
        float floor = 0;                              // stay above the ground here and just ahead
        for (float ahead : { 0.f, 15.f, 30.f }) floor = std::max(floor, height(x, z - ahead));
        cam.position = { x, floor + 16, z };
        cam.target = { 0.6f * x, floor + 6, z - 60 };
    };
    n->add(0, len)->fn = g3d::draw(s);
    return at.size();
}

const Chapter CHAPTERS[] = {
    { "A meadow in the wind", "GPU grass blades and instanced trees; gust fronts cross both",
      "flora::addGrass(scene, land, flora::meadow());   flora::addForest(scene, at, trees);" },
    { "A woodland fly-over", "%TREES% trees: full, reduced or impostor by distance, cross-faded",
      "flora::scatter(land, {.spacing = 5.5f, .clumping = 0.55f});   ForestSpec{.impostorFrom = 80}" },
    { "A tree that grows", "trunk, then branches out from the stem, then leaves; any frame alone",
      "flora::addGrowingTree(scene, tree, {0, 0, 0}, 0, 1, grow);   field.arrival = arrivalFrom(...)" },
    { "In two dimensions", "grass, shrubs and trees in parallax layers, grown and swayed from t",
      "flora2d::tree(c, base, flora2d::oak(), t, u);   flora2d::grass(c, hill, flora2d::meadow(), t, u);" },
};

std::string fmtThousands(long long v) {
    std::string d = std::to_string(v), out;
    for (size_t i = 0; i < d.size(); i++) {
        if (i && (d.size() - i) % 3 == 0) out += ',';
        out += d[i];
    }
    return out;
}

}  // namespace

void registerFlora(Registry& out) {
    Comp c;
    c.name = "flora";
    c.w = 1920; c.h = 1080; c.fps = FPS;
    c.background = mkColor(0x101418);
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);

    TNode* meadow = ts.add(LEN);
    prefab::use(meadow, 0, LEN, "flora-meadow", Json::parse(R"({"box":[0,0,1920,1080],"trees":14,"wind":1.5,"orbit":0.1})"));
    caption(meadow, LEN, CHAPTERS[0]);

    ts.transition(Transition::fade(XF));
    TNode* fly = ts.add(LEN);
    Chapter wood = CHAPTERS[1];
    wood.note.replace(wood.note.find("%TREES%"), 7, fmtThousands((long long)flyover(fly, LEN)));
    caption(fly, LEN, wood);

    ts.transition(Transition::fade(XF));
    TNode* grow = ts.add(LEN);
    prefab::use(grow, 0, LEN, "flora-tree-grow", Json::parse(R"({"box":[0,0,1920,1080],"tree":"oak","delay":10,"duration":150})"));
    caption(grow, LEN, CHAPTERS[2]);

    ts.transition(Transition::wipe(XF, Direction::FromRight));
    TNode* flat = ts.add(LEN);
    prefab::use(flat, 0, LEN, "flora-2d-hill", Json::parse(R"({"box":[0,0,1920,1080],"trees":5,"delay":10,"duration":110})"));
    caption(flat, LEN, CHAPTERS[3], true);

    c.duration = ts.end();
    const int total = c.duration;
    c.assets = [total](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, total);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

}  // namespace rmtn
