// cities.cpp -- "city-models": CityJSON in g3d. The bundled sample town rises from the
// ground under an orbiting camera, recolours by construction year with a legend, shows
// its LoD 1.2 blocks beside its LoD 2.2 buildings, picks out the tower by its id, lights
// two streets and their lamps at dusk, and runs a bus along its route (a line geometry).
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>

namespace rmtn {
namespace {

const Color BG = mkColor(0x0b1020), INK = mkColor(0xeef0ff), DIM = mkColor(0x8a91b4), AQUA = mkColor(0x47dad8),
            GOLDEN = mkColor(0xffc857), OLD = mkColor(0x7a3b8f), NEW = mkColor(0xf2d45c);

void caption(TNode* scene, const char* kicker, const char* title) {
    scene->add(0, scene->dur)->fn = [kicker, title](const Ctx& cx, int t) {
        const float k = (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        Ctx c = cx;
        c.a *= k;
        dr::text(fonts::ui(false), c, kicker, { 120, 96 + 12 * (1 - k) }, 28, 4, AQUA);
        dr::text(fonts::ui(true), c, title, { 120, 136 + 12 * (1 - k) }, 64, 0, INK);
    };
}

bool ground(const city::Object& o) {
    return o.type == "LandUse" || o.type == "Road" || o.type == "WaterBody" || o.type == "PlantCover";
}

// A lit scene for the town: sun with shadows, soft sky, the ground objects laid flat (one
// mesh, or with `groundMesh` false none: add them as objects to animate them).
g3d::ScenePtr townScene(const city::Model& town, const city::Look& look, bool groundMesh = true) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->skyColor = mkColor(0xbcd0ff);
    scene->groundColor = mkColor(0x3a3530);
    scene->ambient = 0.55f;
    scene->shadowMapSize = 2048;
    g3d::Light sun = g3d::Light::directional({ 230, 190, 140 }, mkColor(0xfff1dc), 1.3f, true);
    sun.shadowExtent = 220;
    sun.shadowBias = 0.15f;
    scene->lights.push_back(sun);
    g3d::Material flat;
    flat.roughness = 0.95f;
    flat.castShadow = false;
    if (groundMesh) scene->add(city::mesh(town.filter(ground), look), flat);
    return scene;
}

// The model with only the faces `keep` accepts (objects left with none are dropped).
city::Model keepFaces(const city::Model& m, const std::function<bool(const city::Object&, const city::Face&)>& keep) {
    city::Model out = m;
    out.objects.clear();
    for (city::Object o : m.objects) {
        bool any = false;
        for (city::Geometry& g : o.geometry) {
            std::vector<city::Face> faces;
            for (const city::Face& f : g.faces)
                if (keep(o, f)) faces.push_back(f);
            g.faces = std::move(faces);
            any = any || !g.faces.empty();
        }
        if (any) out.objects.push_back(std::move(o));
    }
    return out;
}

bool lampHead(const city::Object& o, const city::Face& f) { return o.type == "CityFurniture" && f.kind == "+Lamp"; }

// Where the bus is at a frame (a fraction of the route's length): it leaves at `depart`,
// eases into each stop, waits there, and eases out again.
struct Timetable {
    std::vector<float> stops;               // route fraction of each stop
    std::vector<int> arrive;                // frame it reaches each stop
    std::vector<int> legStart;              // frame each leg starts (the last runs to the end)
    std::vector<float> legFrom, legTo;
    int finish = 0;
    float at(int t) const {
        for (size_t i = legStart.size(); i-- > 0;) {
            if (t < legStart[i]) continue;
            const int end = i < arrive.size() ? arrive[i] : finish;
            const float k = (float)interpolate(t, { (double)legStart[i], (double)end }, { 0, 1 }, clamped(ease::inOut(ease::quad)));
            return legFrom[i] + (legTo[i] - legFrom[i]) * k;
        }
        return 0;
    }
};

Timetable timetable(const std::vector<Vector3>& route, const std::vector<Vector3>& stopPoints, int depart, int travel, int dwell) {
    Timetable tt;
    for (const Vector3& s : stopPoints) {          // the nearest point of the route
        float best = 0, bestD = 1e30f;
        for (int i = 0; i <= 2000; i++) {
            const float d = Vector3Distance(city::along(route, i / 2000.f), s);
            if (d < bestD) { bestD = d; best = i / 2000.f; }
        }
        tt.stops.push_back(best);
    }
    std::sort(tt.stops.begin(), tt.stops.end());
    float from = 0;
    int t = depart;
    for (size_t i = 0; i <= tt.stops.size(); i++) {
        const float to = i < tt.stops.size() ? tt.stops[i] : 1.f;
        tt.legStart.push_back(t);
        tt.legFrom.push_back(from);
        tt.legTo.push_back(to);
        t += std::max(12, (int)std::lround(travel * (to - from)));
        if (i < tt.stops.size()) tt.arrive.push_back(t);
        else tt.finish = t;
        t += dwell;
        from = to;
    }
    return tt;
}

// A soft halo: rings of falling alpha.
void halo(const Ctx& c, Vector2 at, float r, Color col, float a) {
    for (int i = 6; i >= 1; i--) dr::circle(c, at, r * i / 6, withAlpha(col, a * 0.16f));
}

city::Look townLook() {
    city::Look look;
    look.terrain = mkColor(0x56604a);
    look.road = mkColor(0x3c4150);
    look.water = mkColor(0x2f5f8f);
    look.vegetation = mkColor(0x5d8a4e);
    return look;
}

const std::string SCORE = "out/city-models-assets-v1/score.wav";

// Timing shared by the picture and the score (frames).
constexpr int RISE_DELAY = 10, RISE_LENGTH = 26;
constexpr float RISE_STAGGER = 0.55f;
constexpr int GLOW_FROM = 20, GLOW_TO = 50, CARD_FROM = 58, CARD_TO = 76;

constexpr int DUSK_TO = 60, ROAD_FROM = 26, ROAD_TO = 50, LAMP_FROM = 64, LAMP_STAGGER = 5;
constexpr int DEPART = 26, TRAVEL = 200, DWELL = 16;

struct Cues {
    int s2 = 0, s3 = 0, s4 = 0, s5 = 0, s6 = 0, end = 0, risers = 0;
    std::vector<int> lamps;                 // frame each lamp lights (in scene 5)
    std::vector<int> stops;                 // frame the bus reaches each stop (in scene 6)
};

// The score, 100 bpm on C-Am-F-G: a pad and bass under the whole piece, drums once the town
// stands. Effects: a rising pentatonic pluck as the buildings go up (pitch climbing with
// the count), a chord when the last one lands, a shimmer as the colours change, a bell per
// level-of-detail panel, a swell and soft impact as the tower lights, and a tick and bell
// when its card opens.
void genScore(const std::string& path, const Cues& q) {
    using procgen::Score;
    const double fps = 30, end = q.end / fps;
    Score s(end + 0.5);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord cM{ 36, { 60, 64, 67 }, { 72, 76, 79, 76 } }, am{ 45, { 60, 64, 69 }, { 72, 76, 81, 76 } },
                f{ 41, { 60, 65, 69 }, { 72, 77, 81, 77 } }, g{ 43, { 59, 62, 67 }, { 71, 74, 79, 74 } };
    const Chord prog[4] = { cM, am, f, g };
    const double bar = 4 * 60.0 / 100, beat = bar / 4, stop = end - 2.2;
    const double risen = (RISE_DELAY + RISE_STAGGER * (q.risers - 1) + RISE_LENGTH) / fps;
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& ch = prog[i % 4];
        const bool drums = t >= risen - 0.1, full = t >= q.s3 / fps - 0.1;
        for (int p : ch.pad) s.pad(t, bar * 0.96, p, 0.07f, drums ? 1600 : 950);
        for (int e = 0; e < 4; e++) s.bass(t + e * beat, beat * 0.85, ch.root, drums ? 0.2f : 0.12f);
        if (drums) { s.kick(t, 0.38f); s.kick(t + 2 * beat, 0.32f); }
        if (full) { s.clap(t + beat, 0.1f); s.clap(t + 3 * beat, 0.1f); }
        if (drums) for (int h = 0; h < 8; h++) s.hat(t + h * beat / 2, h % 2 ? 0.05f : 0.03f, h % 2 ? 0.4f : 0.6f);
        if (full)
            for (int e = 0; e < 8; e++) s.pluck(t + e * beat / 2, ch.arp[e % 4], 0.025f, e % 2 ? 0.3f : 0.7f);
    }
    // The town going up: every third building plucks, climbing a C pentatonic.
    const int penta[5] = { 0, 2, 4, 7, 9 };
    for (int k = 0; k < q.risers; k += 3) {
        const int step = k / 3, note = 60 + 12 * (step / 5 % 3) + penta[step % 5];
        s.pluck((RISE_DELAY + RISE_STAGGER * k) / fps, note, 0.035f, 0.3f + 0.4f * (step % 2), 4);
    }
    s.impact(RISE_DELAY / fps, 0.06f);
    for (int k = 0; k < 3; k++) s.bell(risen + k * 0.12, cM.arp[k] + 12, 0.07f, 0.3f + 0.2f * k);
    // Scene changes.
    for (int at : { q.s2, q.s3, q.s4 }) s.riser(at / fps - 0.6, at / fps, 0.04f);
    for (int k = 0; k < 4; k++) s.bell(q.s2 / fps + 0.1 + k * 0.09, 84 + penta[k], 0.05f, 0.2f + 0.2f * k);   // recolour
    s.tick((q.s2 + 10) / fps, 0.04f);                                                                         // legend
    s.bell((q.s3 + 8) / fps, 76, 0.07f, 0.25f);                                                               // LoD 1.2
    s.bell((q.s3 + 20) / fps, 83, 0.07f, 0.75f);                                                              // LoD 2.2
    // The tower lights up, then its card opens.
    s.riser((q.s4 + GLOW_FROM) / fps, (q.s4 + GLOW_TO) / fps, 0.06f);
    s.impact((q.s4 + GLOW_TO) / fps, 0.1f);
    s.tick((q.s4 + CARD_FROM) / fps, 0.06f);
    s.bell((q.s4 + CARD_FROM + 4) / fps, 88, 0.08f, 0.6f);
    s.pluck((q.s4 + CARD_FROM + 10) / fps, 91, 0.04f, 0.65f, 3);
    // Dusk: each street swells and chimes as it lights, then a soft bell per lamp, climbing.
    for (int k = 0; k < 2; k++) {
        s.riser((q.s5 + ROAD_FROM + 12 * k) / fps - 0.3, (q.s5 + ROAD_TO - 12 + 12 * k) / fps, 0.035f);
        s.bell((q.s5 + ROAD_TO - 12 + 12 * k) / fps, k ? 79 : 72, 0.07f, k ? 0.7f : 0.3f);
    }
    for (size_t k = 0; k < q.lamps.size(); k++) {
        const int step = (int)k / 2, note = 72 + 12 * (step / 5 % 2) + penta[step % 5];
        s.bell((q.s5 + q.lamps[k]) / fps, note, 0.03f, k % 2 ? 0.75f : 0.25f);
        if (k % 2 == 0) s.tick((q.s5 + q.lamps[k]) / fps, 0.015f);
    }
    // The bus: a swell as it pulls away and a two-note chime at each stop.
    s.riser((q.s6 + DEPART) / fps - 0.5, (q.s6 + DEPART) / fps + 0.3, 0.04f);
    s.tick((q.s6 + DEPART) / fps, 0.05f);
    for (int at : q.stops) {
        s.bell((q.s6 + at) / fps, 88, 0.08f, 0.45f);
        s.bell((q.s6 + at) / fps + 0.18, 84, 0.07f, 0.55f);
    }
    // The end: a held C with bells.
    for (int p : cM.pad) { s.pad(stop, 2.2, p, 0.1f, 1800, 0.35f); s.pad(stop, 2.2, p - 12, 0.07f, 1100, 0.65f); }
    s.bass(stop, 2.2, cM.root - 12, 0.2f);
    for (int k = 0; k < 4; k++) s.bell(stop + k * 0.35, cM.arp[k] + 12, 0.06f, 0.2f + 0.2f * k);
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerCities(Registry& out) {
    Comp c;
    c.name = "city-models";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    auto scene = [&](int dur) {
        TNode* s = ts.add(dur);
        s->fn = [](const Ctx& cx, int) { dr::rectGrad4(cx, 0, 0, 1920, 1080, mkColor(0x1a2340), mkColor(0x1a2340), BG, BG); };
        return s;
    };

    const city::Model town = city::load("data/city/town.city.json");
    const city::Model standing = town.filter([](const city::Object& o) { return !ground(o); });
    const BoundingBox box = town.bounds();

    // 1. The town rises, buildings nearest the middle first, as the camera circles.
    int risers = 0;
    TNode* s1 = scene(240);
    caption(s1, "CITY::LOAD, CITY::RISE", "a CityJSON town, LoD 2.2");
    {
        const city::Look look = townLook();
        auto g = townScene(town, look);
        g3d::Material m;
        m.roughness = 0.8f;
        risers = (int)city::add(*g, standing, look, m)->children.size();
        city::rise(g->find("city"), RISE_DELAY, RISE_STAGGER, RISE_LENGTH);
        g->camera.fov = 38;
        g->camera.up = [box](int t, g3d::Camera& cam) {
            city::frame(cam, box, 200 + t * 0.18f, 36 - t * 0.03f, -0.12f);
        };
        s1->add(0, 240)->fn = g3d::draw(g);
    }

    // 2. Coloured by an attribute: the year each building went up.
    ts.transition(Transition::fade(16));
    TNode* s2 = scene(180);
    caption(s2, "LOOK::COLOR", "by yearOfConstruction");
    {
        city::Look look = townLook();
        look.color = [](const city::Object& o, const city::Face& f) {
            if ((o.type != "Building" && o.type != "BuildingPart") || f.kind == "GroundSurface") return BLANK;
            const Json& y = o.attributes["yearOfConstruction"];
            if (!y.isNumber()) return mkColor(0x9aa0b4);
            const float u = std::clamp((float)((y.asNumber() - 1880) / 140), 0.f, 1.f);
            Color col = mixC(OLD, NEW, u);
            if (f.kind == "RoofSurface") col = mixC(col, WHITE, 0.15f);
            return col;
        };
        auto g = townScene(town, look);
        g3d::Material m;
        m.roughness = 0.8f;
        city::add(*g, standing, look, m);
        g->camera.fov = 38;
        g->camera.up = [box](int t, g3d::Camera& cam) { city::frame(cam, box, 243 + t * 0.18f, 29, -0.12f); };
        s2->add(0, 180)->fn = g3d::draw(g);
        s2->add(10, 170)->fn = [](const Ctx& cx, int t) {
            Ctx k = cx;
            k.a *= (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped());
            const float x = 1380, y = 960, w = 400;
            for (int i = 0; i < 100; i++)
                dr::rect(k, x + w * i / 100.f, y, w / 100.f + 1, 18, mixC(OLD, NEW, i / 99.f));
            dr::text(code::font(false), k, "1880", { x, y + 30 }, 24, 0, DIM, 0, 0);
            dr::text(code::font(false), k, "2020", { x + w, y + 30 }, 24, 0, DIM, 1, 0);
            dr::text(fonts::ui(false), k, "year built", { x, y - 36 }, 26, 0, INK, 0, 0);
        };
    }

    // 3. Levels of detail: the same block as LoD 1.2 prisms and as LoD 2.2 houses.
    ts.transition(Transition::fade(16));
    TNode* s3 = scene(180);
    caption(s3, "LEVELS OF DETAIL", "at(\"1.2\") and at(\"2.2\")");
    {
        const Vector3 near = city::base(town.get("house-12"));
        const city::Model block = town.filter([near](const city::Object& o) {
            if (!o.at() || ground(o) || o.type != "Building") return false;
            const Vector3 b = city::base(o);
            return std::hypot(b.x - near.x, b.y - near.y) < 60;
        });
        const BoundingBox bb = block.bounds();
        for (int side = 0; side < 2; side++) {
            city::Look look = townLook();
            look.lod = side == 0 ? "1.2" : "2.2";
            auto g = townScene(town, look);
            g->lights[0].shadowExtent = 120;
            g3d::Material m;
            m.roughness = 0.8f;
            city::add(*g, block, look, m);
            g->camera.fov = 34;
            g->camera.up = [bb](int t, g3d::Camera& cam) { city::frame(cam, bb, 215 + t * 0.12f, 32, -0.05f, 0.88f); };
            const Rectangle view{ side == 0 ? 60.f : 980.f, 250, 880, 780 };
            s3->add(0, 180)->fn = g3d::draw(g, view);
            s3->add(8, 172)->fn = [side, view](const Ctx& cx, int t) {
                Ctx k = cx;
                k.a *= (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped());
                dr::text(code::font(false), k, side == 0 ? "LoD 1.2  blocks" : "LoD 2.2  roofs, walls, ground",
                         { view.x + 20, view.y + view.height - 40 }, 28, 0, GOLDEN, 0, 0);
            };
        }
    }

    // 4. Finding one object: the tower, by id, picked out while the rest goes quiet.
    ts.transition(Transition::fade(16));
    TNode* s4 = scene(170);
    caption(s4, "MODEL::GET", "get(\"tower\"), its parts and attributes");
    {
        const city::Look look = townLook();
        auto g = townScene(town, look);
        g3d::Material m;
        m.roughness = 0.8f;
        g3d::Object3D* group = city::add(*g, standing, look, m);
        for (const auto& child : group->children) {
            const bool tower = child->name.rfind("tower", 0) == 0;
            child->up = [tower](int t, g3d::Object3D& self) {
                const float k = (float)interpolate(t, { GLOW_FROM, GLOW_TO }, { 0, 1 }, clamped(ease::inOut(ease::cubic)));
                if (tower) {
                    self.material.emissive = withAlpha(mixC(BLACK, mkColor(0x8a5a10), k), 1.f);
                    self.material.color = mixC(WHITE, GOLDEN, k);
                } else {
                    self.material.color = mixC(WHITE, mkColor(0x5a6078), k * 0.8f);
                }
            };
        }
        const city::Object& tower = town.get("tower");
        BoundingBox tb = town.get("tower-podium").bounds();
        const BoundingBox shaft = town.get("tower-shaft").bounds();
        tb.max = Vector3Max(tb.max, shaft.max);
        g->camera.fov = 38;
        g->camera.up = [box, tb](int t, g3d::Camera& cam) {
            const float k = (float)interpolate(t, { 10, 110 }, { 0, 1 }, clamped(ease::inOut(ease::cubic)));
            BoundingBox b;
            b.min = Vector3Lerp(box.min, tb.min, k);
            b.max = Vector3Lerp(box.max, tb.max, k);
            city::frame(cam, b, 286 + t * 0.1f, 30 - 8 * k, (float)(-0.12 + 0.9 * k));
        };
        s4->add(0, 170)->fn = g3d::draw(g);

        // A callout pinned to the top of the shaft: the 3D point is projected through the
        // scene's camera every frame, so the card follows the tower as the camera moves and
        // always faces the viewer.
        const Vector3 top = city::toScene(Vector3{ (shaft.min.x + shaft.max.x) / 2, (shaft.min.y + shaft.max.y) / 2, shaft.max.z });
        char rows[3][96];
        std::snprintf(rows[0], sizeof rows[0], "%.0f m  %s", tower.attributes["measuredHeight"].asNumber(),
                      tower.attributes["function"].asString().c_str());
        std::snprintf(rows[1], sizeof rows[1], "%d parts: podium, shaft", (int)tower.children.size());
        std::snprintf(rows[2], sizeof rows[2], "id \"%s\"", tower.id.c_str());
        struct Card {
            std::string name, rows[3];
        };
        const Card card{ tower.attributes["name"].asString(), { rows[0], rows[1], rows[2] } };
        s4->add(0, 170)->fn = [g, top, card](const Ctx& cx, int t) {
            const float k = (float)interpolate(t, { CARD_FROM, CARD_TO }, { 0, 1 }, clamped(ease::out(ease::cubic)));
            if (k <= 0) return;
            Vector2 at;
            if (!g3d::project(g3d::cameraAt(g->camera, t), top, { 0, 0, 1920, 1080 }, at)) return;
            const float w = 400, h = 186;
            const Vector2 panel{ std::min(at.x + 150, 1880 - w), std::max(at.y - 150 - 60 * (1 - k), 240.f) };
            Ctx cc = cx;
            cc.a *= k;
            dr::circle(cc, at, 9, GOLDEN);
            dr::circle(cc, at, 16, withAlpha(GOLDEN, 0.3f));
            vg::stroke(cc, vg::Path::line(at, { panel.x, panel.y + h / 2 }).trim(0, k), vg::Stroke(GOLDEN, 2.5f));
            dr::roundRect(cc, panel.x, panel.y, w, h, 14, withAlpha(mkColor(0x0b1020), 0.86f));
            dr::rect(cc, panel.x, panel.y, 6, h, GOLDEN);
            dr::text(fonts::ui(true), cc, card.name.c_str(), { panel.x + 30, panel.y + 24 }, 40, 0, INK, 0, 0);
            for (int i = 0; i < 3; i++)
                dr::text(code::font(false), cc, card.rows[i].c_str(), { panel.x + 30, panel.y + 82 + 32.f * i }, 24, 0,
                         i == 0 ? GOLDEN : DIM, 0, 0);
        };
    }

    // 5. Not just buildings: two roads found by id light up as dusk falls, then the street
    // lamps (CityFurniture placed from a template) come on one after another.
    ts.transition(Transition::fade(16));
    TNode* s5 = scene(230);
    caption(s5, "ROADS AND CITY FURNITURE", "get(\"avenue-1\"), 24 street lamps");
    std::vector<int> lampTimes;
    {
        const city::Look look = townLook();
        auto g = townScene(town, look, false);
        g->up = [](int t, g3d::Scene& self) {
            const float k = (float)interpolate(t, { 0, DUSK_TO }, { 0, 1 }, clamped(ease::inOut(ease::cubic)));
            self.ambient = 0.55f - 0.33f * k;
            self.skyColor = mixC(mkColor(0xbcd0ff), mkColor(0x34406e), k);
            self.lights[0].intensity = 1.3f - 0.95f * k;
            self.lights[0].color = mixC(mkColor(0xfff1dc), mkColor(0xff9a5c), k);
        };
        g3d::Material flat;
        flat.roughness = 0.95f;
        flat.castShadow = false;
        g3d::Object3D* streets = city::add(*g, town.filter(ground), look, flat);
        streets->name = "ground";
        for (const auto& child : streets->children) {
            const int order = child->name == "avenue-1" ? 0 : child->name == "street-ew-1" ? 1 : -1;
            if (order < 0) continue;
            child->up = [order](int t, g3d::Object3D& self) {
                const float k = (float)interpolate(t, { ROAD_FROM + 12. * order, ROAD_TO - 12. + 12 * order }, { 0, 1 },
                                                   clamped(ease::out(ease::cubic)));
                self.material.emissive = mixC(BLACK, mkColor(0x1b6f86), k);
                self.material.color = mixC(WHITE, AQUA, k * 0.6f);
            };
        }
        g3d::Material m;
        m.roughness = 0.8f;
        city::add(*g, keepFaces(standing, [](const city::Object& o, const city::Face& f) { return !lampHead(o, f); }), look, m);

        // The lamp heads on their own and unlit, so each glows when it comes on; the lamps of
        // each street light from its west or south end, both streets together.
        const city::Model heads = keepFaces(town, lampHead);
        g3d::Material bulb;
        bulb.unlit = true;
        bulb.castShadow = false;
        g3d::Object3D* lit = city::add(*g, heads, look, bulb);
        std::vector<Vector3> glows;
        std::map<std::string, int> perStreet;
        for (size_t i = 0; i < heads.objects.size(); i++) {
            const city::Object& lamp = heads.objects[i];
            const int on = LAMP_FROM + LAMP_STAGGER * perStreet[lamp.attributes["street"].asString()]++;
            lampTimes.push_back(on);
            const BoundingBox hb = lamp.bounds();
            glows.push_back(city::toScene(Vector3Scale(Vector3Add(hb.min, hb.max), 0.5f)));
            lit->children[i]->up = [on](int t, g3d::Object3D& self) {
                const float k = (float)interpolate(t, { (double)on, on + 8. }, { 0, 1 }, clamped());
                const float flash = (float)interpolate(t, { on + 2., on + 16. }, { 1, 0 }, clamped());
                self.material.color = mixC(mkColor(0x2a2c34), mixC(mkColor(0xffd88a), WHITE, flash), k);
            };
        }
        BoundingBox rb = town.get("avenue-1").bounds();
        const BoundingBox sb = town.get("street-ew-1").bounds();
        rb.min = Vector3Min(rb.min, sb.min);
        rb.max = Vector3Max(rb.max, sb.max);
        rb.max.z = 30;
        g->camera.fov = 38;
        g->camera.up = [rb](int t, g3d::Camera& cam) { city::frame(cam, rb, 205 + t * 0.12f, 33 - t * 0.02f, -0.2f); };
        s5->add(0, 230)->fn = g3d::draw(g);

        // Halos over the lit lamps and a tag on each street, projected from 3D every frame.
        struct Tag { std::string name; Vector3 at; int order; };
        std::vector<Tag> tags;
        for (const char* id : { "avenue-1", "street-ew-1" }) {
            const city::Object& road = town.get(id);
            const BoundingBox b = road.bounds();
            const bool ns = b.max.y - b.min.y > b.max.x - b.min.x;    // tagged part-way along, off the crossing
            const Vector3 mid{ ns ? (b.min.x + b.max.x) / 2 : b.min.x + (b.max.x - b.min.x) * 0.1f,
                               ns ? b.min.y + (b.max.y - b.min.y) * 0.12f : (b.min.y + b.max.y) / 2, b.max.z };
            tags.push_back({ std::string(id) + "  " + road.attributes["name"].asString(), city::toScene(mid), (int)tags.size() });
        }
        s5->add(0, 230)->fn = [g, glows, lampTimes, tags](const Ctx& cx, int t) {
            const g3d::Camera cam = g3d::cameraAt(g->camera, t);
            Vector2 at;
            for (size_t i = 0; i < glows.size(); i++) {
                const float k = (float)interpolate(t, { (double)lampTimes[i], lampTimes[i] + 10. }, { 0, 1 }, clamped());
                if (k > 0 && g3d::project(cam, glows[i], { 0, 0, 1920, 1080 }, at)) halo(cx, at, 22, mkColor(0xffd88a), k);
            }
            for (const Tag& tag : tags) {
                const float k = (float)interpolate(t, { ROAD_TO - 4. + 12 * tag.order, ROAD_TO + 10. + 12 * tag.order }, { 0, 1 },
                                                   clamped(ease::out(ease::cubic)));
                if (k <= 0 || !g3d::project(cam, tag.at, { 0, 0, 1920, 1080 }, at)) continue;
                Ctx cc = cx;
                cc.a *= k;
                const float w = 30 + 15.5f * tag.name.size(), lift = 10 * (1 - k);
                dr::roundRect(cc, at.x - w / 2, at.y - 70 - lift, w, 46, 23, withAlpha(BG, 0.85f));
                dr::text(code::font(false), cc, tag.name.c_str(), { at.x, at.y - 47 - lift }, 26, 0, AQUA, 0.5f, 0.5f);
                dr::circle(cc, at, 6, AQUA);
            }
        };
    }

    // 6. A trajectory: the bus route is a MultiLineString. city::trail draws it on while a
    // bus follows with city::along, pulling in at the stops (a MultiPoint).
    ts.transition(Transition::fade(16));
    const std::vector<Vector3> route = town.get("bus-route").at()->lines[0];
    const std::vector<Vector3> stopPoints = town.get("bus-stops").at()->points;
    const Timetable tt = timetable(route, stopPoints, DEPART, TRAVEL, DWELL);
    const int s6dur = tt.finish + 50;
    TNode* s6 = scene(s6dur);
    caption(s6, "CITY::TRAIL, CITY::ALONG", "Line 7, a MultiLineString");
    {
        const city::Look look = townLook();
        auto g = townScene(town, look);
        g3d::Material m;
        m.roughness = 0.8f;
        city::add(*g, standing, look, m);
        city::TrailStyle line;
        line.width = 5.5f;
        line.color = GOLDEN;
        city::trail(*g, route, [tt](int t) { return tt.at(t); }, line);
        // Stops: a post and a sign beside each, and a pad on the road that pops as the bus pulls in.
        g3d::Material post;
        post.color = mkColor(0xdde2f0);
        g3d::Material sign;
        sign.color = GOLDEN;
        sign.emissive = mkColor(0x3a2a08);
        g3d::Material pad;
        pad.color = GOLDEN;
        pad.unlit = true;
        pad.castShadow = false;
        for (size_t i = 0; i < stopPoints.size(); i++) {
            const Vector3 at = city::toScene(stopPoints[i]);
            g3d::Object3D* disc = g->add(g3d::Geometry::cylinder(5, 0.25f, 40), pad);
            disc->position = { at.x, at.y + 0.1f, at.z };
            const int arrive = i < tt.arrive.size() ? tt.arrive[i] : 0;
            disc->up = [arrive](int t, g3d::Object3D& self) {
                const float k = (float)interpolate(t, { arrive - 10., arrive + 4. }, { 0, 1 }, clamped(ease::out(ease::back(2.5))));
                self.visible = k > 0;
                self.scale = { k, 1, k };
            };
            g->add(g3d::Geometry::cylinder(0.12f, 3.2f, 12), post)->position = { at.x + 3, at.y + 1.6f, at.z + 3 };
            g->add(g3d::Geometry::box(1.2f, 0.8f, 0.12f), sign)->position = { at.x + 3, at.y + 3.2f, at.z + 3 };
        }
        // The bus, its front along +z, turned to the direction of travel (smoothed round corners).
        const float len = city::length(route);
        auto heading = [route, len](float u) {
            const float du = 7 / len;
            const Vector3 a = city::along(route, u - du), b = city::along(route, u + du);
            return std::atan2(b.x - a.x, b.y - a.y) * RAD2DEG;
        };
        g3d::Object3D* bus = g->add();
        bus->name = "bus";
        g3d::Material paint;
        paint.color = mkColor(0xe8505b);
        paint.roughness = 0.4f;
        g3d::Material glass;
        glass.color = mkColor(0x1d2436);
        glass.roughness = 0.2f;
        bus->add(g3d::Geometry::box(2.6f, 2.6f, 11), paint)->position = { 0, 1.6f, 0 };
        bus->add(g3d::Geometry::box(2.7f, 0.9f, 10.4f), glass)->position = { 0, 2.1f, 0.1f };
        bus->scale = { 1.5f, 1.5f, 1.5f };                       // larger than life, to read at this distance
        bus->up = [tt, route, heading](int t, g3d::Object3D& self) {
            const float u = tt.at(t);
            self.position = city::toScene(city::along(route, u));
            self.rotation = { 0, 180 - heading(u), 0 };
        };
        // The camera frames the route, closer in, and drifts after the bus.
        const BoundingBox rbox = town.get("bus-route").bounds();
        const Vector3 centre = city::toScene(Vector3Scale(Vector3Add(rbox.min, rbox.max), 0.5f));
        g->camera.fov = 38;
        g->camera.up = [rbox, centre, tt, route](int t, g3d::Camera& cam) {
            city::frame(cam, rbox, 168 + t * 0.08f, 44 - t * 0.03f, -0.3f);
            Vector3 drift = Vector3Scale(Vector3Subtract(city::toScene(city::along(route, tt.at(t - 20))), centre), 0.45f);
            drift.y = 0;
            cam.position = Vector3Add(cam.position, drift);
            cam.target = Vector3Add(cam.target, drift);
        };
        s6->add(0, s6dur)->fn = g3d::draw(g);

        // 2D: a "Line 7" tag riding on the bus, and each stop's name under its pad as the bus pulls in.
        s6->add(0, s6dur)->fn = [g, tt, route, stopPoints](const Ctx& cx, int t) {
            const g3d::Camera cam = g3d::cameraAt(g->camera, t);
            Vector2 at;
            Vector3 roof = city::toScene(city::along(route, tt.at(t)));
            roof.y += 4;
            const float k = (float)interpolate(t, { 6, 22 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
            if (k > 0 && g3d::project(cam, roof, { 0, 0, 1920, 1080 }, at)) {
                Ctx cc = cx;
                cc.a *= k;
                dr::roundRect(cc, at.x - 62, at.y - 64, 124, 44, 22, mkColor(0xe8505b));
                dr::text(fonts::ui(true), cc, "Line 7", { at.x, at.y - 42 }, 26, 0, WHITE, 0.5f, 0.5f);
            }
            for (size_t i = 0; i < tt.arrive.size() && i < stopPoints.size(); i++) {
                const float a = (float)interpolate(t, { tt.arrive[i] - 6., tt.arrive[i] + 8. }, { 0, 1 }, clamped(ease::out(ease::back())));
                if (a <= 0) continue;
                if (!g3d::project(cam, city::toScene(stopPoints[i]), { 0, 0, 1920, 1080 }, at)) continue;   // below its pad
                Ctx cc = cx;
                cc.a *= std::min(a, 1.f);
                char name[32];
                std::snprintf(name, sizeof name, "STOP %d", (int)i + 1);
                const float h = 40 * a;
                dr::roundRect(cc, at.x - 64, at.y + 26, 128, h, 10, withAlpha(BG, 0.9f));
                dr::text(code::font(false), cc, name, { at.x, at.y + 26 + h / 2 }, 24, 0, GOLDEN, 0.5f, 0.5f);
            }
        };
    }

    c.duration = ts.end();
    const Cues cues{ s2->start, s3->start, s4->start, s5->start, s6->start, c.duration, risers, lampTimes, tt.arrive };
    c.assets = [cues](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, cues);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

} // namespace rmtn
