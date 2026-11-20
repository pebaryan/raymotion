// flora_tests.cpp -- vegetation without a window: areas (rectangles, polygons with holes,
// exclusions, strips, edge fades, slope and altitude limits, mesh faces, geo regions), noise,
// and scattering (spacing, determinism, edits that only remove, density, clumps, speed).
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <vector>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define NEAR(a, b, eps) CHECK(std::fabs((double)(a) - (double)(b)) <= (eps))

static void areas() {
    flora::Area a = flora::rect({ 10, 10 }, { -10, -10 });                 // corners in any order
    CHECK(a.min.x == -10 && a.max.y == 10);
    CHECK(a.weight(0, 0) == 1 && a.weight(11, 0) == 0 && a.weight(0, -10.5f) == 0);

    // A square with a square hole: inside an odd number of rings.
    flora::Area p = flora::polygon({ { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } });
    p.polygons.push_back({ { 4, 4 }, { 6, 4 }, { 6, 6 }, { 4, 6 } });
    CHECK(p.min.x == 0 && p.max.x == 10);
    CHECK(p.weight(2, 2) == 1 && p.weight(5, 5) == 0 && p.weight(12, 5) == 0);

    // Exclusions, clearings and strips; the edge fade.
    flora::Area e = flora::rect({ -20, -20 }, { 20, 20 });
    e.exclude.push_back({ { 5, 5 }, { 8, 5 }, { 8, 8 }, { 5, 8 } });
    e.clearings.push_back({ { -10, -10 }, 3 });
    e.strips.push_back({ { { -20, 0 }, { 20, 0 } }, 4 });
    CHECK(e.weight(6, 6) == 0 && e.weight(-10, -8) == 0 && e.weight(3, 1.5f) == 0 && e.weight(3, 2.5f) == 1);
    NEAR(e.edgeDistance(3, -5), 3, 1e-4);                                 // the strip's edge is at z = -2
    e.edge = 2;
    CHECK(e.weight(3, 2.5f) > 0 && e.weight(3, 2.5f) < 0.5f && e.weight(3, 4.5f) > 0.99f);

    // Slope from the ground, and altitude.
    flora::Area s = flora::rect({ -10, -10 }, { 10, 10 });
    s.ground = [](float x, float) { return x * std::tan(30 * DEG2RAD); };
    NEAR(s.slope(0, 0), 30, 0.05);
    NEAR(s.height(3, 0), 3 * std::tan(30 * DEG2RAD), 1e-4);
    s.maxSlope = 20;
    CHECK(s.weight(0, 0) == 0);
    s.maxSlope = 45;
    CHECK(s.weight(0, 0) == 1);
    s.maxAltitude = 1;
    CHECK(s.weight(0, 0) == 1 && s.weight(5, 0) == 0);

    // Density, clamped.
    flora::Area d = flora::rect({ -1, -1 }, { 1, 1 });
    d.density = [](float x, float) { return x * 4; };
    CHECK(d.weight(-0.5f, 0) == 0 && d.weight(0.1f, 0) > 0.39f && d.weight(0.1f, 0) < 0.41f && d.weight(0.9f, 0) == 1);
}

static void noise() {
    float lo = 1, hi = 0, jump = 0;
    for (int i = 0; i < 4000; i++) {
        const float x = i * 0.37f, z = i * 0.11f - 50;
        const float n = flora::noise(x, z, 10, 4);
        lo = std::min(lo, n); hi = std::max(hi, n);
        jump = std::max(jump, std::fabs(n - flora::noise(x + 0.01f, z, 10, 4)));
    }
    std::printf("[flora] noise range %.3f..%.3f, largest step over 1 cm %.4f\n", lo, hi, jump);
    CHECK(lo >= 0 && hi <= 1 && hi - lo > 0.5f && jump < 0.01f);
    CHECK(flora::noise(3, 4, 10, 4) == flora::noise(3, 4, 10, 4) && flora::noise(3, 4, 10, 4) != flora::noise(3, 4, 10, 5));
}

static float closest(const std::vector<flora::Placement>& ps) {
    std::map<std::pair<int, int>, std::vector<Vector2>> bins;          // 5 m bins
    for (const auto& p : ps) bins[{ (int)std::floor(p.position.x / 5), (int)std::floor(p.position.z / 5) }].push_back({ p.position.x, p.position.z });
    float best = 1e30f;
    for (const auto& [key, list] : bins)
        for (int dj = -1; dj <= 1; dj++)
            for (int di = -1; di <= 1; di++) {
                auto other = bins.find({ key.first + di, key.second + dj });
                if (other == bins.end()) continue;
                for (Vector2 a : list)
                    for (Vector2 b : other->second)
                        if (a.x != b.x || a.y != b.y) best = std::min(best, Vector2Distance(a, b));
            }
    return best;
}

static void scattering() {
    flora::Area a = flora::rect({ -50, -50 }, { 50, 50 });
    a.ground = [](float x, float z) { return 0.1f * x + 0.05f * z; };
    flora::ScatterSpec spec;
    spec.spacing = 2;
    spec.variants = 3;
    const auto ps = flora::scatter(a, spec, 9);
    const double perR2 = ps.size() * 4.0 / (100.0 * 100.0);              // placements per spacing squared
    std::printf("[flora] scatter: %zu placements over 100 m at 2 m spacing (%.2f per spacing^2), closest %.3f m\n",
                ps.size(), perR2, closest(ps));
    CHECK(perR2 > 0.5 && perR2 < 0.9);
    CHECK(closest(ps) >= 2 - 1e-3f);
    bool onGround = true, ranges = true;
    int variantCount[3] = {};
    for (const auto& p : ps) {
        onGround = onGround && std::fabs(p.position.y - a.height(p.position.x, p.position.z)) < 1e-4f;
        ranges = ranges && p.yaw >= 0 && p.yaw < 360 && p.scale >= spec.scaleMin && p.scale <= spec.scaleMax && p.variant >= 0 && p.variant < 3;
        if (p.variant >= 0 && p.variant < 3) variantCount[p.variant]++;
    }
    CHECK(onGround && ranges && variantCount[0] > 0 && variantCount[1] > 0 && variantCount[2] > 0);

    // Determinism, and seeds.
    const auto again = flora::scatter(a, spec, 9), other = flora::scatter(a, spec, 10);
    bool same = again.size() == ps.size();
    for (size_t i = 0; same && i < ps.size(); i++)
        same = ps[i].id == again[i].id && ps[i].position.x == again[i].position.x && ps[i].yaw == again[i].yaw;
    CHECK(same);
    CHECK(other.size() != ps.size() || other[0].position.x != ps[0].position.x);

    // A road only removes: every placement kept is where it was, and none is on the road.
    flora::Area road = a;
    road.strips.push_back({ { { -50, -10 }, { 50, 15 } }, 6 });
    const auto kept = flora::scatter(road, spec, 9);
    std::map<uint32_t, Vector3> before;
    for (const auto& p : ps) before[p.id] = p.position;
    bool subset = kept.size() < ps.size(), offRoad = true;
    for (const auto& p : kept) {
        auto found = before.find(p.id);
        subset = subset && found != before.end() && Vector3Distance(found->second, p.position) < 1e-6f;
        offRoad = offRoad && road.weight(p.position.x, p.position.z) > 0;
    }
    CHECK(subset && offRoad);

    // Density thins; clumping gathers.
    flora::Area half = a;
    half.density = [](float, float) { return 0.5f; };
    const double ratio = (double)flora::scatter(half, spec, 9).size() / ps.size();
    CHECK(ratio > 0.42 && ratio < 0.58);
    flora::ScatterSpec clumped = spec;
    clumped.clumping = 0.7f;
    clumped.clumpScale = 20;
    auto spread = [](const std::vector<flora::Placement>& list) {           // coefficient of variation over 10 m cells
        std::vector<int> cells(100, 0);
        for (const auto& p : list) cells[(size_t)std::clamp((int)((p.position.x + 50) / 10), 0, 9) * 10 + std::clamp((int)((p.position.z + 50) / 10), 0, 9)]++;
        double m = 0, v = 0;
        for (int c : cells) m += c / 100.0;
        for (int c : cells) v += (c - m) * (c - m) / 100.0;
        return std::sqrt(v) / std::max(m, 1e-9);
    };
    const auto groves = flora::scatter(a, clumped, 9);
    std::printf("[flora] clumping 0.7: %zu of %zu kept, spread %.2f vs %.2f even\n", groves.size(), ps.size(), spread(groves), spread(ps));
    CHECK(groves.size() < ps.size() * 0.8 && spread(groves) > 2 * spread(ps));

    CHECK([] { try { flora::scatter(flora::rect({ 0, 0 }, { 10000, 10000 }), {}, 1); } catch (const std::invalid_argument&) { return true; } return false; }());
    CHECK(flora::scatter(flora::rect({ 0, 0 }, { 0, 5 }), {}, 1).empty());

    // A landscape's worth of trees.
    flora::Area land = flora::rect({ -500, -500 }, { 500, 500 });
    land.strips.push_back({ { { -500, 0 }, { 500, 40 } }, 8 });
    flora::ScatterSpec trees;
    trees.spacing = 4;
    trees.clumping = 0.3f;
    const auto t0 = std::chrono::steady_clock::now();
    const auto forest = flora::scatter(land, trees, 2);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("[flora] scatter: %zu trees over a square kilometre in %.0f ms\n", forest.size(), ms);
    CHECK(forest.size() > 20000);
}

static void meshAreas() {
    // A table top at y = 2 over x, z in 0..2, a ramp too steep to keep, and a lower floor.
    std::vector<Vector3> tris = {
        { 0, 2, 0 }, { 0, 2, 2 }, { 2, 2, 0 }, { 2, 2, 0 }, { 0, 2, 2 }, { 2, 2, 2 },          // top
        { 2, 2, 0 }, { 2, 2, 2 }, { 3, 0, 0 },                                                  // 63 degrees
        { -5, 0, -5 }, { -5, 0, 5 }, { 5, 0, -5 },                                              // floor (one triangle)
    };
    flora::Area a = flora::onMesh(tris, 35);
    CHECK(a.weight(1, 1) == 1 && a.height(1, 1) == 2);                    // the highest face over the point
    CHECK(a.weight(-3, -3) == 1 && a.height(-3, -3) == 0);
    CHECK(a.weight(4, 4) == 0);                                            // outside the floor triangle
    CHECK(a.min.x == -5 && a.max.x == 5);
    const auto onTop = flora::scatter(flora::onMesh({ tris.begin(), tris.begin() + 6 }, 35), { 0.3f }, 1);
    bool allOnTop = !onTop.empty();
    for (const auto& p : onTop) allOnTop = allOnTop && p.position.y == 2;
    CHECK(allOnTop);
    CHECK(flora::onMesh({ tris.begin() + 6, tris.begin() + 9 }, 35).weight(2.5f, 0.5f) == 0);
}

static void regions() {
    // France in an equirectangular plane, a metre a degree, north towards -z.
    const geo::Feature& france = geo::countries().get("FRA");
    flora::Area fr = flora::region(france.geometry, [](geo::LonLat p) { return Vector2{ (float)p.lon, (float)-p.lat }; });
    CHECK(fr.weight(2.35f, -48.85f) == 1);                                 // Paris
    CHECK(fr.weight(13.4f, -52.5f) == 0);                                  // Berlin
    CHECK(fr.min.x < -4 && fr.max.y > -43);
}

static void fields() {
    flora::Area a = flora::rect({ -30, -20 }, { 30, 20 });
    a.strips.push_back({ { { -30, 0 }, { 30, 0 } }, 4 });
    a.ground = [](float x, float) { return x * 0.1f; };
    g3d::Field f = flora::meadow();
    const int revision = f.revision;
    flora::fit(f, a);
    CHECK(f.min.x == -30 && f.max.y == 20 && f.revision == revision + 1);
    CHECK(f.mask && f.mask(5, 0) == 0 && f.mask(5, 10) == 1 && f.ground && f.ground(10, 3) == 1);
    CHECK(f.mapResolution >= 120);
}

// Trees on the CPU: presets build within the 16-bit index limit, the same spec gives the same
// tree, the wind moves the crown but not the foot, seasons turn and drop deciduous leaves.
static void treesCpu() {
    const char* names[] = { "oak", "birch", "pine", "spruce", "palm", "tropical", "shrub", "hedge" };
    for (const char* n : names) {
        const flora::TreePtr t = flora::makeTree(flora::tree(n), { 8, 4, 4, 32 });
        CHECK(t->hero.wood->vertices > 0 && t->hero.wood->vertices < 65536);
        CHECK(t->hero.leaves->vertices > 0 && t->hero.leaves->vertices < 65536);
        CHECK(t->reducedTriangles < t->heroTriangles);
        CHECK(t->views.size() == 4);
        std::printf("[flora] tree %-8s top %.2f m (spec %.1f), reach %.2f m\n", n, t->top, flora::tree(n).height, t->radius);
        CHECK(t->top > 0.5f * flora::tree(n).height && t->top < 1.2f * flora::tree(n).height + flora::tree(n).leafSize);   // cards stand out
    }
    bool threw = false;
    try { flora::tree("baobab"); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    const flora::TreePtr a = flora::makeTree(flora::oak(), { 8, 4, 4, 32 }), b = flora::makeTree(flora::oak(), { 8, 4, 4, 32 });
    CHECK(a->hero.leaves->positions == b->hero.leaves->positions && a->hero.wood->positions == b->hero.wood->positions);
    flora::TreeSpec other = flora::oak();
    other.seed = 2;
    CHECK(flora::makeTree(other, { 8, 4, 4, 32 })->hero.wood->positions != a->hero.wood->positions);

    // Wind: frame 2 of 8 differs from frame 0 high in the crown, not at the foot.
    const auto& w = *a->hero.wood;
    float foot = 0, crown = 0;
    for (int v = 0; v < w.vertices; v++) {
        const float* p0 = &w.positions[(size_t)v * 3];
        const float* p4 = &w.positions[((size_t)2 * w.vertices + v) * 3];
        const float d = std::hypot(p4[0] - p0[0], p4[2] - p0[2]);
        if (p0[1] < 0.3f) foot = std::max(foot, d);
        if (p0[1] > 9) crown = std::max(crown, d);
    }
    std::printf("[flora] oak wind: foot moves %.4f m, crown up to %.3f m\n", foot, crown);
    CHECK(foot < 0.002f && crown > 0.05f);

    // Seasons: autumn turns leaves warmer; a bare deciduous tree has none; a pine keeps its.
    auto leafVerts = [](flora::TreeSpec s, float season) {
        s.season = season;
        return flora::makeTree(s, { 1, 4, 1, 16 })->hero.leaves->vertices;
    };
    CHECK(leafVerts(flora::oak(), 1) == 0);
    CHECK(leafVerts(flora::oak(), 0.9f) < leafVerts(flora::oak(), 0.35f));
    CHECK(leafVerts(flora::pine(), 1) == leafVerts(flora::pine(), 0.35f));
}

// Growth on the CPU: u = 1 is the tree at rest exactly, u = 0 is nothing (every triangle
// collapsed), the wood grows out from the stem, and leaves come last.
static void growthCpu() {
    const flora::TreePtr t = flora::makeTree(flora::oak(), { 8, 4, 4, 32 });
    const flora::TreePose full = flora::grownPositions(*t, 1);
    bool exact = full.wood.size() == (size_t)t->hero.wood->vertices && full.leaves.size() == (size_t)t->hero.leaves->vertices;
    for (size_t i = 0; exact && i < full.wood.size(); i++)
        exact = full.wood[i].x == t->hero.wood->positions[i * 3] && full.wood[i].y == t->hero.wood->positions[i * 3 + 1] &&
                full.wood[i].z == t->hero.wood->positions[i * 3 + 2];
    for (size_t i = 0; exact && i < full.leaves.size(); i++)
        exact = full.leaves[i].x == t->hero.leaves->positions[i * 3] && full.leaves[i].y == t->hero.leaves->positions[i * 3 + 1];
    CHECK(exact);
    // u = 0: every vertex of a ring or card sits on its pivot, so nothing has area.
    const flora::TreePose none = flora::grownPositions(*t, 0);
    float spread = 0;
    for (size_t i = 1; i < none.leaves.size(); i++)
        if (i % 4) spread = std::max(spread, Vector3Distance(none.leaves[i], none.leaves[i - 1]));
    CHECK(spread < 1e-5f);
    float top0 = 0;
    for (const Vector3& p : none.wood) top0 = std::max(top0, p.y);
    // Part way: the stem is up before the crown's tips, and leaves wait for 0.6.
    auto highest = [](const std::vector<Vector3>& v) { float h = 0; for (const Vector3& p : v) h = std::max(h, p.y); return h; };
    auto leafArea = [](const std::vector<Vector3>& v) {
        double a = 0;
        for (size_t i = 0; i + 3 < v.size(); i += 4) a += Vector3Length(Vector3CrossProduct(Vector3Subtract(v[i + 1], v[i]), Vector3Subtract(v[i + 3], v[i])));
        return a;
    };
    const flora::TreePose third = flora::grownPositions(*t, 0.3f), late = flora::grownPositions(*t, 0.8f);
    std::printf("[flora] oak growing: wood top %.2f m at u 0, %.2f at 0.3, %.2f at 0.8, %.2f at 1; leaf area %.0f, %.0f, %.0f m^2\n",
                top0, highest(third.wood), highest(late.wood), highest(full.wood), leafArea(third.leaves), leafArea(late.leaves), leafArea(full.leaves));
    CHECK(top0 < 0.5f);
    CHECK(highest(third.wood) > 2 && highest(third.wood) < highest(late.wood) && highest(late.wood) <= highest(full.wood) + 1e-3f);
    CHECK(leafArea(third.leaves) < 1e-3 && leafArea(late.leaves) > 0.1 * leafArea(full.leaves) && leafArea(late.leaves) < leafArea(full.leaves));
    // Grass fronts: 0 at the start, 1 at the reach and beyond.
    const auto from = flora::arrivalFrom({ 2, 0 }, 10);
    NEAR(from(2, 0), 0, 1e-6); NEAR(from(7, 0), 0.5, 1e-6); NEAR(from(40, 0), 1, 1e-6);
    const auto along = flora::arrivalAlong({ { 0, 0 }, { 10, 0 } }, 4);
    NEAR(along(5, 2), 0.5, 1e-6);
    const auto into = flora::arrivalInto(flora::rect({ 0, 0 }, { 10, 10 }), 5);
    NEAR(into(5, 5), 1, 1e-6); NEAR(into(1, 5), 0.2, 1e-6);
}

static void stylesCpu() {
    // Low-poly: faceted crowns, a fraction of the realistic triangles, and growth still exact at u = 1.
    flora::TreeBuild realistic{ 8, 4, 4, 32 }, low = realistic, paper = realistic;
    low.style = flora::TreeStyle::LowPoly;
    paper.style = flora::TreeStyle::PaperCut;
    const char* names[] = { "oak", "spruce", "palm", "shrub" };
    for (const char* n : names) {
        const flora::TreePtr r = flora::makeTree(flora::tree(n), realistic), l = flora::makeTree(flora::tree(n), low),
                             p = flora::makeTree(flora::tree(n), paper);
        std::printf("[flora] %-6s triangles: realistic %lld, low-poly %lld, paper %lld; heights %.1f %.1f %.1f m\n", n, r->heroTriangles,
                    l->heroTriangles, p->heroTriangles, r->top, l->top, p->top);
        CHECK(l->heroTriangles > 20 && (std::string(n) == "palm" || l->heroTriangles * 3 < r->heroTriangles));
        CHECK(l->top > 0.75f * r->top && l->top < 1.25f * r->top);
        CHECK((int)l->views.size() == 4 && l->leaves.alphaCutoff == 0 && !l->leaves.dynamicMap);
        CHECK(p->heroTriangles == 2 && p->views.empty() && p->wood.dynamicMap && p->wood.alphaCutoff > 0);
        CHECK(p->top >= r->top * 0.99f);                                // the card holds the whole tree
        const flora::TreePose full = flora::grownPositions(*l, 1);
        bool exact = full.leaves.size() == (size_t)l->hero.leaves->vertices;
        for (size_t i = 0; exact && i < full.leaves.size(); i++)
            exact = full.leaves[i].x == l->hero.leaves->positions[i * 3] && full.leaves[i].y == l->hero.leaves->positions[i * 3 + 1] &&
                    full.leaves[i].z == l->hero.leaves->positions[i * 3 + 2];
        CHECK(exact);
    }
    // Splat canopies: two splats a leaf card in place of the cards, wood kept, no impostors.
    flora::TreeBuild splatBuild = realistic;
    splatBuild.style = flora::TreeStyle::Splat;
    const flora::TreePtr soft = flora::makeTree(flora::oak(), splatBuild), sharp = flora::makeTree(flora::oak(), realistic);
    std::printf("[flora] splat oak: %zu splats, %d wood vertices, top %.1f m (realistic %.1f)\n", soft->canopy ? soft->canopy->size() : 0,
                soft->hero.wood->vertices, soft->top, sharp->top);
    CHECK(soft->canopy && soft->canopy->size() > 500 && soft->hero.leaves->frames == 0 && soft->views.empty());
    CHECK(soft->top > 0.9f * sharp->top && soft->top < 1.1f * sharp->top);
    CHECK(!sharp->canopy);
    // Shading reaches every material; toon grass is drawn in flat bands.
    flora::TreeBuild toon = low;
    toon.shading = g3d::Shading::Toon;
    const flora::TreePtr t = flora::makeTree(flora::oak(), toon);
    CHECK(t->wood.shading == g3d::Shading::Toon && t->leaves.shading == g3d::Shading::Toon && t->impostor.shading == g3d::Shading::Toon);
    CHECK(flora::grass("toon").colorBands == 3 && flora::toonGrass().width > flora::meadow().width);
}

static void flat() {
    // 2D: ground lines are deterministic and follow their amplitude; presets resolve by name.
    const auto a = flora2d::groundLine(0, 1000, 500, 40, 400, 3), b = flora2d::groundLine(0, 1000, 500, 40, 400, 3);
    float lo = 1e9f, hi = -1e9f;
    for (const Vector2& p : a) { lo = std::min(lo, p.y); hi = std::max(hi, p.y); }
    CHECK(a.size() == 64 && a.front().x == 0 && a.back().x == 1000 && hi <= 540.01f && lo >= 459.99f && hi - lo > 30);
    bool same = a.size() == b.size();
    for (size_t i = 0; same && i < a.size(); i++) same = a[i].x == b[i].x && a[i].y == b[i].y;
    CHECK(same);
    NEAR(flora2d::heightAt({ { 0, 10 }, { 10, 20 } }, 5), 15, 1e-5);
    NEAR(flora2d::heightAt({ { 0, 10 }, { 10, 20 } }, -4), 10, 1e-5);
    CHECK(flora2d::grass("tall").height > flora2d::grass("lawn").height && flora2d::tree("pine").form == flora2d::TreeForm::Conifer);
    bool threw = false;
    try { flora2d::tree("baobab"); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
    // The wind: still is zero; a direction flips the lean.
    flora2d::Wind w;
    CHECK(flora2d::bendAt(flora2d::Wind{ 0 }, 100, 40) == 0);
    w.sway = 0; w.gust = 0;
    const float right = flora2d::bendAt(w, 100, 40);
    w.direction = -1;
    NEAR(flora2d::bendAt(w, 100, 40), -right, 1e-6);

    // Outside motion blur the wind reads t as it is: bendAt equals the formula on integer frames.
    CHECK(subframe() == 0);
    flora2d::Wind g;
    double worst = 0;
    for (int t : { 0, 1, 17, 90, 301 })
        for (float x : { 0.f, 150.f, 777.f }) {
            const float s = t / g.fps, xp = x - g.gustSpeed * s, period = g.gustWidth * 3, phase = 0.3f;
            const float gust = std::pow(0.5f + 0.5f * std::cos(6.28318531f * xp / period), 8.f) * g.gust;
            const float want = g.strength * (0.12f + 0.55f * gust + 0.14f * std::sin(6.28318531f * (g.sway * s + phase) + x * 0.004f) +
                                             0.05f * gust * std::sin(6.28318531f * (3 * s + phase)));
            worst = std::max(worst, std::fabs((double)want - flora2d::bendAt(g, x, t, phase)));
        }
    std::printf("[flora2d] bendAt on integer frames vs the formula: worst difference %.3g\n", worst);
    CHECK(worst < 1e-6);
}

// ---- meadows from vector paths

// Distance from p to a ring's edges, and whether p is inside the rings by even-odd.
static float distToRings(const std::vector<vg::Polyline>& rings, Vector2 p, bool& inside) {
    float best = 1e30f;
    inside = false;
    for (const vg::Polyline& r : rings)
        for (size_t i = 0; i < r.pts.size(); i++) {
            const Vector2 a = r.pts[i], b = r.pts[(i + 1) % r.pts.size()];
            if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
            const Vector2 ab{ b.x - a.x, b.y - a.y }, ap{ p.x - a.x, p.y - a.y };
            const float k = std::clamp((ap.x * ab.x + ap.y * ab.y) / std::max(ab.x * ab.x + ab.y * ab.y, 1e-9f), 0.f, 1.f);
            best = std::min(best, std::hypot(ap.x - k * ab.x, ap.y - k * ab.y));
        }
    return best;
}

static void pathMeadows() {
    // A shape with a cubic crest and a round hole.
    const vg::Path outer = vg::Path::parse("M 20 150 C 20 20 280 20 280 150 L 280 190 L 20 190 Z");
    const vg::Path hole = vg::Path::parse("M 100 130 C 100 100 200 100 200 130 C 200 160 100 160 100 130 Z");
    vg::Path shape = outer;
    shape.append(hole);                                                   // same winding: a hole by even-odd
    vg::Path nz = outer;
    nz.append(hole.reversed());                                           // opposite winding: a hole by non-zero
    const flora2d::Grass lawn = [] { flora2d::Grass g = flora2d::lawn(); g.density = 2; return g; }();

    const auto roots = flora2d::meadowRoots(shape, lawn, 5, 0.5f, vg::FillRule::EvenOdd);
    const auto again = flora2d::meadowRoots(shape, lawn, 5, 0.5f, vg::FillRule::EvenOdd);
    const auto other = flora2d::meadowRoots(shape, lawn, 6, 0.5f, vg::FillRule::EvenOdd);
    const auto viaNonZero = flora2d::meadowRoots(nz, lawn, 5, 0.5f, vg::FillRule::NonZero);
    const auto fine = shape.flatten(0.005f);
    const auto holeFine = hole.flatten(0.005f);
    float worstOutside = 0;
    int inHole = 0, nearCurve = 0;
    float deepestInHole = 0;
    for (const Vector2& r : roots) {
        bool in;
        const float d = distToRings(fine, r, in);
        if (!in) worstOutside = std::max(worstOutside, d);
        bool inH;
        const float dh = distToRings(holeFine, r, inH);
        if (inH) deepestInHole = std::max(deepestInHole, dh);
        inHole += inH && dh > 0.5f;
        nearCurve += in && d < 1.5f && r.y < 150;                         // roots hugging the curved crest
    }
    std::printf("[flora2d] path meadow: %zu roots, worst %.4f px outside the shape (limit 0.5), %d deeper than 0.5 px in the hole (deepest %.4f px), %d within 1.5 px of the crest\n",
                roots.size(), worstOutside, inHole, deepestInHole, nearCurve);
    CHECK(roots.size() > 5000 && worstOutside <= 0.5f && inHole == 0 && nearCurve > 20);
    CHECK(roots.size() == again.size() && std::equal(roots.begin(), roots.end(), again.begin(), [](Vector2 a, Vector2 b) { return a.x == b.x && a.y == b.y; }));
    CHECK(other.size() != roots.size() || other[0].x != roots[0].x);
    CHECK(viaNonZero.size() == roots.size() && std::equal(roots.begin(), roots.end(), viaNonZero.begin(), [](Vector2 a, Vector2 b) { return a.x == b.x && a.y == b.y; }));
    // Without the hole there are roots in that place: the hole is what empties it.
    const auto whole = flora2d::meadowRoots(outer, lawn, 5, 0.5f);
    int filled = 0;
    for (const Vector2& r : whole) { bool in; distToRings(holeFine, r, in); filled += in; }
    std::printf("[flora2d] the same shape without the hole has %zu roots, %d of them where the hole is\n", whole.size(), filled);
    CHECK(filled > 500 && whole.size() > roots.size());
    CHECK(flora2d::meadowRoots(vg::Path{}, lawn).empty() && flora2d::meadowRoots(vg::Path::line({ 0, 0 }, { 10, 10 }), lawn).empty());

    // The polygon overload keeps its roots: golden values recorded from the code before the
    // path overload existed (the same loop, built on its own).
    const std::vector<Vector2> quad{ { 60, 90 }, { 260, 90 }, { 300, 190 }, { 20, 190 } };
    struct Gold { float density, height; uint32_t seed; float persp; size_t n; double sx, sy; float fx, fy, lx, ly; };
    const Gold golds[] = { { 1.6f, 18, 1, 0.5f, 15345, 2455712.9389, 2192113.2913, 61.4347f, 90.0888f, 298.8576f, 189.2592f },
                           { 0.9f, 46, 7, 0.3f, 3354, 536489.0985, 476755.2815, 60.3387f, 90.9502f, 297.1361f, 186.0949f } };
    for (const Gold& k : golds) {
        flora2d::Grass g; g.density = k.density; g.height = k.height;
        const auto r = flora2d::meadowRoots(quad, g, k.seed, k.persp);
        double sx = 0, sy = 0;
        for (const Vector2& p : r) { sx += p.x; sy += p.y; }
        std::printf("[flora2d] polygon meadow seed %u: %zu roots (was %zu), sums %.4f %.4f (were %.4f %.4f)\n", k.seed, r.size(), k.n, sx, sy, k.sx, k.sy);
        CHECK(r.size() == k.n && std::fabs(sx - k.sx) < 0.01 && std::fabs(sy - k.sy) < 0.01);
        CHECK(r.size() == k.n && std::fabs(r.front().x - k.fx) < 1e-3f && std::fabs(r.front().y - k.fy) < 1e-3f &&
              std::fabs(r.back().x - k.lx) < 1e-3f && std::fabs(r.back().y - k.ly) < 1e-3f);
    }
}

int main() try {
    areas();
    noise();
    scattering();
    meshAreas();
    regions();
    fields();
    treesCpu();
    growthCpu();
    stylesCpu();
    flat();
    pathMeadows();
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::puts("flora tests passed");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "uncaught: %s\n", e.what());
    return 2;
}
