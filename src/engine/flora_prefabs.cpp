// Built-in flora prefabs: "flora-meadow" (a 3D meadow with trees in the wind, the camera
// slowly round it), "flora-tree-grow" (one tree growing from a sapling, grass coming up
// round it) and "flora-2d-hill" (a 2D hillside in layers that grows in). Each fills `box`
// (node px). Props and defaults are in docs/flora.md; frames are functions of t.
#include "rmtn/flora.hpp"
#include "rmtn/flora2d.hpp"
#include "rmtn/prefab.hpp"
#include "rmtn/scene3d.hpp"
#include "rmtn/draw.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rmtn {
namespace flora {
namespace {

Rectangle boxOf(const Json& p, const char* name) {
    const Json& b = p["box"];
    if (!b.isArray() || b.size() != 4) throw std::runtime_error(std::string(name) + ": box must be [x, y, width, height]");
    const Rectangle r{ (float)b[0].asNumber(), (float)b[1].asNumber(), (float)b[2].asNumber(), (float)b[3].asNumber() };
    if (!(r.width > 0 && r.height > 0)) throw std::runtime_error(std::string(name) + ": box needs a positive size");
    return r;
}

float smooth(float u) {
    u = std::clamp(u, 0.f, 1.f);
    return u * u * (3 - 2 * u);
}

// The sky, light and look the 3D prefabs share.
void dress(g3d::Scene& s, float fps) {
    s.sky.enabled = true;
    s.sky.zenith = { 82, 128, 190, 255 };
    s.sky.horizon = { 222, 226, 214, 255 };
    s.sky.sunDirection = { -0.5f, 0.55f, -0.7f };
    s.sky.sunLight = 0;
    s.fogFromSky = true;
    s.ambient = 0.5f;
    s.shadowMapSize = 2048;
    g3d::Light sun = g3d::Light::directional({ -14, 18, -20 }, { 255, 244, 224, 255 }, 2.0f, true);
    sun.shadowExtent = 30;
    s.lights.push_back(sun);
    (void)fps;
}

g3d::Material groundMaterial(Color c) {
    g3d::Material m;
    m.color = c;
    m.roughness = 1;
    return m;
}

TreePtr build(const std::string& name, const char* prefab) {
    TreeBuild b;
    b.views = 6;
    b.viewSize = 128;
    try { return makeTree(tree(name), b); }
    catch (const std::invalid_argument&) { throw std::runtime_error(std::string(prefab) + ": no tree \"" + name + "\""); }
}

g3d::Field field(const std::string& name, const char* prefab) {
    try { return grass(name); }
    catch (const std::invalid_argument&) { throw std::runtime_error(std::string(prefab) + ": no grass \"" + name + "\""); }
}

}  // namespace

void registerPrefabs() {
    // A meadow with trees, the wind across it, the camera slowly round it.
    prefab::add({ "flora-meadow",
        Json::parse(R"({"box":[0,0,1920,1080],"grass":"meadow","size":40,"hills":1.5,"trees":10,
                        "kinds":["oak","birch"],"wind":1,"orbit":0.08,"distance":15,"height":2.2,
                        "ground":"#5d6b3c","fps":30,"seed":1})"),
        [](TNode& n, const Json& p) {
            const char* name = "flora-meadow";
            const Rectangle box = boxOf(p, name);
            const float size = std::clamp((float)p.num("size", 40), 4.f, 400.f), hills = (float)p.num("hills", 1.5);
            const float wind = std::max(0.f, (float)p.num("wind", 1)), fps = std::max(1.f, (float)p.num("fps", 30));
            const uint32_t seed = (uint32_t)p.integer("seed", 1);
            auto s = std::make_shared<g3d::Scene>();
            dress(*s, fps);
            s->fogNear = size;
            s->fogFar = size * 5;
            Area land = rect({ -size, -size }, { size, size });
            land.ground = [hills, seed](float x, float z) { return hills * noise(x, z, 30, seed); };
            s->add(terrain(land, 96, 96), groundMaterial(p.color("ground", Color{ 93, 107, 60, 255 })));
            g3d::Field g = field(p.str("grass", "meadow"), name);
            g.fps = fps;
            g.gust *= wind;
            g.sway *= wind;
            addGrass(*s, land, g, seed);
            std::vector<TreePtr> trees;
            const Json& kinds = p["kinds"];
            if (kinds.isArray())
                for (const Json& k : kinds.asArray()) trees.push_back(build(k.asString(), name));
            const int count = std::clamp(p.integer("trees", 10), 0, 2000);
            if (count > 0 && !trees.empty()) {
                ScatterSpec sc;
                sc.spacing = std::max(3.f, size * 1.6f / std::sqrt((float)count));
                sc.clumping = 0.4f;
                sc.variants = (int)trees.size();
                std::vector<Placement> at = scatter(land, sc, seed + 17);
                // keep the camera's circle clear and hold to `count`
                const float dist = (float)p.num("distance", 15);
                at.erase(std::remove_if(at.begin(), at.end(), [&](const Placement& q) {
                             const float r = std::sqrt(q.position.x * q.position.x + q.position.z * q.position.z);
                             return std::fabs(r - dist) < 6;
                         }), at.end());
                if ((int)at.size() > count) at.resize(count);
                ForestSpec f;
                f.wind = wind;
                f.fps = fps;
                addForest(*s, at, trees, f);
            }
            const float orbit = (float)p.num("orbit", 0.08), dist = (float)p.num("distance", 15), high = (float)p.num("height", 2.2);
            s->camera.fov = 50;
            s->camera.up = [orbit, dist, high, hills, seed, fps](int t, g3d::Camera& cam) {
                const float a = 0.6f + orbit * DEG2RAD * (float)frameTime(t) * 30 / fps;
                const float x = dist * std::sin(a), z = dist * std::cos(a);
                cam.position = { x, high + hills * noise(x, z, 30, seed), z };
                cam.target = { 0, high * 0.6f, 0 };
            };
            n.fn = g3d::draw(s, box);
        } });

    // One tree growing from a sapling, grass coming up round it, the camera slowly round.
    prefab::add({ "flora-tree-grow",
        Json::parse(R"({"box":[0,0,1920,1080],"tree":"oak","grass":"lawn","delay":0,"duration":150,
                        "wind":0.6,"orbit":0.1,"ground":"#55663a","fps":30,"seed":1})"),
        [](TNode& n, const Json& p) {
            const char* name = "flora-tree-grow";
            const Rectangle box = boxOf(p, name);
            const float fps = std::max(1.f, (float)p.num("fps", 30)), wind = std::max(0.f, (float)p.num("wind", 0.6));
            const double delay = p.num("delay", 0), duration = std::max(1.0, p.num("duration", 150));
            const uint32_t seed = (uint32_t)p.integer("seed", 1);
            const TreePtr tr = build(p.str("tree", "oak"), name);
            const float top = std::max(tr->top, 1.f), reach = std::max(tr->radius, top * 0.3f);
            auto s = std::make_shared<g3d::Scene>();
            dress(*s, fps);
            s->lights[0].shadowExtent = top * 1.5f;
            const float size = top * 4, land = top * 40;    // a lawn round the tree on ground to the haze
            s->fogNear = size;
            s->fogFar = land * 0.8f;
            s->camera.farPlane = land * 2;
            s->add(terrain(rect({ -land, -land }, { land, land }), 8, 8), groundMaterial(p.color("ground", Color{ 85, 102, 58, 255 })));
            g3d::Field g = field(p.str("grass", "lawn"), name);
            g.fps = fps;
            g.arrival = arrivalFrom({ 0, 0 }, size);
            g.grow = 0;
            g3d::Object3D* lawn = addGrass(*s, rect({ -size, -size }, { size, size }), g, seed);
            // the grass leads the tree a little, and is all up by the time the leaves open
            lawn->up = [delay, duration](int t, g3d::Object3D& o) {
                o.field->grow = smooth((float)((frameTime(t) - delay + duration * 0.15) / (duration * 0.8)));
            };
            ForestSpec f;
            f.wind = wind;
            f.fps = fps;
            addGrowingTree(*s, tr, { 0, 0, 0 }, 0, 1,
                           [delay, duration](int t) { return smooth((float)((frameTime(t) - delay) / duration)); }, f);
            const float orbit = (float)p.num("orbit", 0.1), dist = top * 1.5f + reach;
            s->camera.fov = 45;
            s->camera.up = [orbit, dist, top, fps](int t, g3d::Camera& cam) {
                const float a = 0.5f + orbit * DEG2RAD * (float)frameTime(t) * 30 / fps;
                cam.position = { dist * std::sin(a), top * 0.35f, dist * std::cos(a) };
                cam.target = { 0, top * 0.45f, 0 };
            };
            n.fn = g3d::draw(s, box);
        } });

    // A 2D hillside: sky, a far ridge of silhouette trees, a near hill of grass with trees
    // and shrubs, all growing in from `delay` over `duration`.
    prefab::add({ "flora-2d-hill",
        Json::parse(R"({"box":[0,0,1920,1080],"top":"#8fb8d8","horizon":"#efe4cc","far":"#8aa58f",
                        "near":"#5b7d3c","grass":"meadow","trees":4,"delay":0,"duration":90,
                        "wind":1,"fps":30,"seed":1})"),
        [](TNode& n, const Json& p) {
            const char* name = "flora-2d-hill";
            const Rectangle box = boxOf(p, name);
            const Color top = p.color("top", Color{ 143, 184, 216, 255 }), horizon = p.color("horizon", Color{ 239, 228, 204, 255 });
            const Color far = p.color("far", Color{ 138, 165, 143, 255 }), near = p.color("near", Color{ 91, 125, 60, 255 });
            const int trees = std::clamp(p.integer("trees", 4), 0, 40);
            const double delay = p.num("delay", 0), duration = std::max(1.0, p.num("duration", 90));
            const uint32_t seed = (uint32_t)p.integer("seed", 1);
            flora2d::Grass g;
            try { g = flora2d::grass(p.str("grass", "meadow").c_str()); }
            catch (const std::invalid_argument&) { throw std::runtime_error(std::string(name) + ": no grass \"" + p.str("grass", "") + "\""); }
            flora2d::Wind w;
            w.strength = std::max(0.f, (float)p.num("wind", 1));
            w.fps = std::max(1.f, (float)p.num("fps", 30));
            const float W = box.width, H = box.height, k = H / 1080;
            g.height *= k;
            g.width *= k;
            const auto ridge = flora2d::groundLine(box.x, box.x + W, box.y + H * 0.62f, 40 * k, 700 * k, seed, 64);
            const auto hill = flora2d::groundLine(box.x, box.x + W, box.y + H * 0.78f, 60 * k, 1100 * k, seed + 1, 64);
            n.fn = [=](const Ctx& c, int t) {
                const float u = smooth((float)((frameTime(t) - delay) / duration));
                dr::rectGrad4(c, box.x, box.y, W, H, top, top, horizon, horizon);
                // the far ridge, hazed, its trees in silhouette
                flora2d::ground(c, ridge, box.y + H, flora2d::hazed(far, 0.5f, horizon));
                for (int i = 0; i < 9; i++) {
                    flora2d::Tree tr = i % 3 == 1 ? flora2d::pine() : flora2d::oak();
                    tr.height = (130 + 40 * (float)((i * 37) % 5) / 4) * k;
                    tr.trunkWidth *= k * 0.5f;
                    tr.leafSize *= k * 0.6f;
                    tr.silhouette = true;
                    tr.dark = flora2d::hazed(far, 0.35f, horizon);
                    const float x = box.x + W * (0.05f + 0.11f * i);
                    flora2d::tree(c, { x, flora2d::heightAt(ridge, x) + 4 }, tr, t, std::clamp(u * 1.4f, 0.f, 1.f), seed + 50 + i, w);
                }
                // the near hill: ground, trees and shrubs, then the grass in front of their roots
                flora2d::ground(c, hill, box.y + H, near);
                for (int i = 0; i < trees; i++) {
                    flora2d::Tree tr = i % 2 ? flora2d::birch() : flora2d::oak();
                    tr.height *= k * (0.9f + 0.25f * (float)((i * 53) % 7) / 6);
                    tr.trunkWidth *= k;
                    tr.leafSize *= k;
                    const float x = box.x + W * (0.12f + 0.76f * (trees > 1 ? (float)i / (trees - 1) : 0.5f));
                    const float late = 0.15f * (float)i / std::max(trees, 1);
                    flora2d::tree(c, { x, flora2d::heightAt(hill, x) + 6 }, tr, t, std::clamp((u - late) / (1 - late), 0.f, 1.f), seed + 10 + i, w);
                    flora2d::Shrub sh;
                    sh.size *= k;
                    sh.height *= k;
                    const float xs = x + W * 0.05f;
                    flora2d::shrub(c, { xs, flora2d::heightAt(hill, xs) + 4 }, sh, t, std::clamp(u * 1.2f - 0.1f, 0.f, 1.f), seed + 30 + i, w);
                }
                flora2d::grass(c, hill, g, t, std::clamp(u * 1.3f, 0.f, 1.f), seed + 2, w);
            };
        } });
}

}  // namespace flora
}  // namespace rmtn
