// maps.cpp -- "maps": geo:: data and projections. A globe spins with its far side clipped
// away, the same world morphs through seven projections, a conic frames one continent with
// a country picked out by its ISO code, and the bundled cities sit on top.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace rmtn {
namespace {

const Color BG = mkColor(0x0b1020), OCEAN = mkColor(0x12203d), LAND = mkColor(0x2f4a6e), BORDER = mkColor(0x0b1020),
            GRID = mkColor(0x23355a), EDGE = mkColor(0x5d7bb0), INK = mkColor(0xeef0ff), DIM = mkColor(0x8a91b4),
            AQUA = mkColor(0x47dad8), GOLDEN = mkColor(0xffc857);

void caption(TNode* scene, const char* kicker, const char* title) {
    scene->add(0, scene->dur)->fn = [kicker, title](const Ctx& cx, int t) {
        const float k = (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        Ctx c = cx;
        c.a *= k;
        dr::text(fonts::ui(false), c, kicker, { 120, 96 + 12 * (1 - k) }, 28, 4, AQUA);
        dr::text(fonts::ui(true), c, title, { 120, 136 + 12 * (1 - k) }, 64, 0, INK);
    };
}

// Ocean, graticule, land, borders and the map's edge.
void world(const Ctx& cx, const geo::Projection& p, float fade = 1) {
    Ctx c = cx;
    c.a *= fade;
    const vg::Path edge = p.outline();
    vg::fill(c, edge, OCEAN);
    vg::stroke(c, p.graticule(15), vg::Stroke(GRID, 1.2f));
    vg::fill(c, p.path(geo::land().merged()), LAND, vg::FillRule::EvenOdd);
    vg::stroke(c, p.lines(geo::countries().merged()), vg::Stroke(BORDER, 1.1f));
    vg::stroke(c, edge, vg::Stroke(EDGE, 2));
}

const std::string SCORE = "out/maps-assets-v1/score.wav";

// Scene starts (frames) and the morph timing of scene 2, shared by the picture and the score.
struct Cues {
    int s2 = 0, s3 = 0, s4 = 0, end = 0;
};
constexpr int MORPH_FROM = 10, HOLD = 30, MOVE = 16, MORPHS = 6;

// The score, 88 bpm on Dm-Bb-F-C: a pad and soft bass from the start, plucked arpeggios
// once the projections begin, a kick under them, hats for the last two scenes. Effects: a
// swell into every projection change and a bell as it lands, a chime for Kenya, a strum
// for the biggest cities and ticks as the rest appear, a closing chord.
void genScore(const std::string& path, const Cues& q) {
    using procgen::Score;
    const double fps = 30, end = q.end / fps;
    Score s(end + 0.5);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord dm{ 38, { 62, 65, 69 }, { 74, 77, 81, 77 } }, bb{ 34, { 62, 65, 70 }, { 74, 77, 82, 77 } },
                f{ 41, { 60, 65, 69 }, { 72, 77, 81, 77 } }, c{ 36, { 60, 64, 67 }, { 72, 76, 79, 76 } };
    const Chord prog[4] = { dm, bb, f, c };
    const double bar = 4 * 60.0 / 88, beat = bar / 4, stop = end - 2.2;
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& ch = prog[i % 4];
        const bool arps = t >= q.s2 / fps - 0.2, drums = t >= q.s2 / fps + bar, hats = t >= q.s3 / fps;
        for (int p : ch.pad) s.pad(t, bar * 0.96, p, 0.07f, arps ? 1500 : 900);
        for (int e = 0; e < 2; e++) s.bass(t + e * 2 * beat, 2 * beat * 0.9, ch.root, 0.16f);
        if (arps)
            for (int e = 0; e < 8; e++) s.pluck(t + e * beat / 2, ch.arp[e % 4], e % 4 == 0 ? 0.045f : 0.028f, e % 2 ? 0.3f : 0.7f);
        if (drums) { s.kick(t, 0.32f); s.kick(t + 2.5 * beat, 0.22f); }
        if (hats) for (int h = 0; h < 8; h++) s.hat(t + h * beat / 2, h % 2 ? 0.05f : 0.03f, h % 2 ? 0.4f : 0.6f);
    }
    // Scene changes: a short swell into each, a soft bell on arrival.
    for (int at : { q.s2, q.s3, q.s4 }) {
        s.riser(at / fps - 0.6, at / fps, 0.04f);
        s.bell(at / fps + 0.05, 81, 0.06f, 0.5f);
    }
    // Projection morphs.
    const int landing[MORPHS] = { 74, 77, 81, 79, 84, 86 };
    for (int k = 0; k < MORPHS; k++) {
        const double from = (q.s2 + MORPH_FROM + k * (HOLD + MOVE) + HOLD) / fps, to = from + MOVE / fps;
        s.riser(from - 0.1, to, 0.05f);
        s.bell(to, landing[k], 0.08f, k % 2 ? 0.35f : 0.65f);
    }
    // Kenya lights up.
    const double kenya = (q.s3 + 30) / fps;
    s.bell(kenya, 81, 0.08f, 0.6f);
    s.bell(kenya + 0.22, 86, 0.07f, 0.65f);
    s.pluck(kenya + 0.44, 93, 0.05f, 0.7f, 3);
    // Cities: a strum for the ten biggest, ticks as the rest pop in.
    const auto& cities = geo::cities().features;
    const int strum[10] = { 62, 65, 69, 74, 77, 81, 86, 89, 93, 98 };
    for (size_t k = 0; k < cities.size(); k++) {
        const double at = (q.s4 + 14 + k * 0.4) / fps;
        const float pan = (float)((cities[k].geometry.points[0].lon + 180) / 360);
        if (k < 10) s.pluck(at + k * 0.05, strum[k], 0.05f, pan, 4);
        else if (k % 5 == 0) s.tick(at, 0.025f);
    }
    // The end: a held Dm with bells.
    for (int p : dm.pad) { s.pad(stop, 2.2, p, 0.1f, 1800, 0.35f); s.pad(stop, 2.2, p - 12, 0.07f, 1100, 0.65f); }
    s.bass(stop, 2.2, dm.root - 12, 0.2f);
    for (int k = 0; k < 4; k++) s.bell(stop + k * 0.35, dm.arp[k] + 12, 0.06f, 0.2f + 0.2f * k);
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerMaps(Registry& out) {
    Comp c;
    c.name = "maps";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    auto scene = [&](int dur) {
        TNode* s = ts.add(dur);
        s->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, BG); };
        return s;
    };

    // 1. A globe turning: the far side is clipped at the horizon and the land closed along it.
    TNode* s1 = scene(150);
    caption(s1, "GEO::PROJECTION", "orthographic, rotating");
    s1->add(0, 150)->fn = [](const Ctx& cx, int t) {
        const double lon = -20 + t * 1.6;
        const auto globe = geo::Projection::orthographic().rotate({ lon, -18 - 10 * std::sin(t / 40.0) })
                               .fit({ 760, 190, 860, 860 }, geo::Sphere);
        world(cx, globe, (float)interpolate(t, { 0, 16 }, { 0, 1 }, clamped()));
    };

    // 2. One world, seven projections, each morphing into the next.
    ts.transition(Transition::fade(16));
    TNode* s2 = scene(330);
    caption(s2, "PROJECTION::MIX", "every map is a projection");
    struct Stop {
        const char* name;
        geo::Projection p;
    };
    const Rectangle box{ 160, 270, 1600, 760 };
    std::vector<Stop> stops = {
        { "Equirectangular", geo::Projection::equirectangular() },
        { "Mercator", geo::Projection::mercator() },
        { "Equal Earth", geo::Projection::equalEarth() },
        { "Robinson", geo::Projection::robinson() },
        { "Mollweide", geo::Projection::mollweide() },
        { "Azimuthal equal-area", geo::Projection::azimuthalEqualArea().rotate({ 20, 30 }) },
        { "Orthographic", geo::Projection::orthographic().rotate({ 20, 30 }) },
    };
    for (Stop& s : stops) s.p.fit(box, s.p.kind() == geo::Kind::Mercator ? geo::land().merged() : geo::Sphere);
    s2->add(MORPH_FROM, 320)->fn = [stops](const Ctx& cx, int t) {
        const int hold = HOLD, move = MOVE, each = hold + move;
        const int i = std::min((int)stops.size() - 1, t / each), local = t - i * each;
        const double u = i + 1 < (int)stops.size() && local > hold
                             ? interpolate(local, { (double)hold, (double)each }, { 0, 1 }, clamped(ease::inOut(ease::cubic)))
                             : 0;
        const geo::Projection p = u > 0 ? geo::Projection::mix(stops[i].p, stops[i + 1].p, u) : stops[i].p;
        world(cx, p, (float)interpolate(t, { 0, 12 }, { 0, 1 }, clamped()));
        Ctx k = cx;
        k.a *= (float)(1 - std::sin(u * PI));
        const char* name = stops[u > 0.5 ? i + 1 : i].name;
        dr::text(code::font(false), k, name, { 1800, 160 }, 34, 0, GOLDEN, 1, 0.5);
    };

    // 3. A conic framed on one continent, with a country picked out by its ISO code.
    ts.transition(Transition::fade(16));
    TNode* s3 = scene(150);
    caption(s3, "FIT AND LOOK UP", "conicEqualArea, get(\"KEN\")");
    const geo::Layer africa = geo::countries().filter([](const geo::Feature& f) { return f.properties.str("continent", "") == "Africa"; });
    const auto conic = geo::Projection::conicEqualArea().parallels(-20, 20).rotate({ -18, 0 })
                           .fit({ 900, 140, 820, 900 }, africa, 10);
    const vg::Path continent = conic.path(africa.merged()), borders = conic.lines(africa.merged());
    char area[64];
    std::snprintf(area, sizeof area, "%.0f,000 km\xc2\xb2", geo::area(geo::countries().get("KEN").geometry) / 1000);
    s3->add(0, 150)->fn = [conic, continent, borders, label = std::string(area)](const Ctx& cx, int t) {
        Ctx m = cx;
        m.a *= (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped());
        vg::fill(m, continent, LAND, vg::FillRule::EvenOdd);
        vg::stroke(m, borders, vg::Stroke(BORDER, 1.4f));
        const geo::Feature& kenya = geo::countries().get("KEN");
        const float k = (float)interpolate(t, { 30, 50 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        vg::fill(m, conic.path(kenya.geometry), withAlpha(GOLDEN, k), vg::FillRule::EvenOdd);
        Ctx l = m;
        l.a *= k;
        const Vector2 at = conic(kenya.labelPoint());
        dr::text(fonts::ui(true), l, kenya.name().c_str(), { at.x + 150, at.y - 40 }, 40, 0, INK, 0, 0.5);
        dr::text(code::font(false), l, label.c_str(), { at.x + 150, at.y + 6 }, 26, 0, DIM, 0, 0.5);
        vg::stroke(l, vg::Path::line({ at.x + 20, at.y - 12 }, { at.x + 138, at.y - 38 }), vg::Stroke(INK, 2));
    };

    // 4. The bundled cities on an Equal Earth map: the largest first.
    ts.transition(Transition::fade(16));
    TNode* s4 = scene(150);
    caption(s4, "GEO::CITIES()", "243 places, largest first");
    const auto flat = geo::Projection::equalEarth().rotate({ -10, 0 }).fit({ 160, 250, 1600, 780 }, geo::Sphere);
    s4->add(0, 150)->fn = [flat](const Ctx& cx, int t) {
        world(cx, flat, (float)interpolate(t, { 0, 12 }, { 0, 1 }, clamped()));
        const auto& cities = geo::cities().features;
        std::vector<Vector2> labelled;
        for (size_t i = 0; i < cities.size(); i++) {
            const float k = (float)interpolate(t - 14 - (double)i * 0.4, { 0, 10 }, { 0, 1 }, clamped(ease::out(ease::back())));
            if (k <= 0) continue;
            const Vector2 at = flat(cities[i].geometry.points[0]);
            const bool big = i < 10;
            dr::circle(cx, at, (big ? 7.f : 3.5f) * k, big ? GOLDEN : AQUA);
            bool crowded = false;
            for (Vector2 o : labelled) crowded = crowded || (std::fabs(o.x - at.x) < 150 && std::fabs(o.y - at.y) < 30);
            if (big && !crowded) {
                labelled.push_back(at);
                Ctx l = cx;
                l.a *= std::min(1.f, k);
                dr::text(fonts::ui(false), l, cities[i].name().c_str(), { at.x + 12, at.y }, 24, 0, INK, 0, 0.5);
            }
        }
    };

    c.duration = ts.end();
    const Cues cues{ s2->start, s3->start, s4->start, c.duration };
    c.assets = [cues](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, cues);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

} // namespace rmtn
