// flora.cpp -- "flora": an engine showcase (not a film) of the 0.12 vegetation:
//   flora::addGrass / g3d::Field   a meadow in gusting wind
//   flora::addForest               a landscape fly-over: thousands of trees, levels and impostors
//   flora::addGrowingTree          a tree from a sapling, grass coming up round it
//   flora2d::                      a 2D hillside in layers
// The meadow, the tree and the hillside are the built-in prefabs (flora-meadow,
// flora-tree-grow, flora-2d-hill); the fly-over is built here.
//   raymotion render flora
#include "rmtn/raymotion.hpp"

namespace rmtn {
namespace {

constexpr int FPS = 30, LEN = 210, XF = 20;
const Color INK = mkColor(0xf2f4ff), SUB = mkColor(0xb4bfcc);

Font bold() { return fonts::ui(true); }
Font regular() { return fonts::ui(false); }

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
    out.push_back(std::move(c));
}

}  // namespace rmtn
