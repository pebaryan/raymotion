#include "rmtn/flags.hpp"
#include "rmtn/anim.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/fonts.hpp"
#include "rmtn/prefab.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rmtn::flags {
namespace {

struct Country { const char* code; const char* name; };
constexpr Country data[] = {
    {"at", "Austria"}, {"be", "Belgium"}, {"br", "Brazil"}, {"ca", "Canada"},
    {"ch", "Switzerland"}, {"cn", "China"}, {"cz", "Czechia"}, {"de", "Germany"},
    {"dk", "Denmark"}, {"ee", "Estonia"}, {"fi", "Finland"}, {"fr", "France"},
    {"gb", "United Kingdom"}, {"hu", "Hungary"}, {"id", "Indonesia"}, {"ie", "Ireland"},
    {"in", "India"}, {"it", "Italy"}, {"jp", "Japan"}, {"lu", "Luxembourg"},
    {"mc", "Monaco"}, {"nl", "Netherlands"}, {"no", "Norway"}, {"pl", "Poland"},
    {"ro", "Romania"}, {"ru", "Russia"}, {"se", "Sweden"}, {"ua", "Ukraine"},
    {"us", "United States"},
};

const Country& country(const std::string& code) {
    for (const Country& item : data) if (code == item.code) return item;
    throw std::runtime_error("flags: unknown country '" + code + "'");
}

Color rgb(unsigned hex) { return mkColor(hex); }
void box(const Ctx& c, float x, float y, float w, float h, unsigned col) {
    dr::rect(c, x, y, w, h, rgb(col));
}
void horizontal(const Ctx& c, unsigned a, unsigned b, unsigned d) {
    box(c, -15, -10, 30, 20.f / 3, a);
    box(c, -15, -10 + 20.f / 3, 30, 20.f / 3, b);
    box(c, -15, -10 + 40.f / 3, 30, 20.f / 3, d);
}
void vertical(const Ctx& c, unsigned a, unsigned b, unsigned d) {
    box(c, -15, -10, 10, 20, a);
    box(c, -5, -10, 10, 20, b);
    box(c, 5, -10, 10, 20, d);
}
void polygon(const Ctx& c, std::initializer_list<Vector2> pts, unsigned col) {
    dr::fillPoly(c, pts.begin(), (int)pts.size(), rgb(col));
}
void cross(const Ctx& c, unsigned base, unsigned stripe, float x, float y, float w) {
    box(c, -15, -10, 30, 20, base);
    box(c, x - w / 2, -10, w, 20, stripe);
    box(c, -15, y - w / 2, 30, w, stripe);
}

void artwork(const Ctx& c, const std::string& code) {
    if (code == "at") horizontal(c, 0xed2939, 0xffffff, 0xed2939);
    else if (code == "be") vertical(c, 0x000000, 0xfdda24, 0xef3340);
    else if (code == "ch") {
        box(c, -15, -10, 30, 20, 0xda291c);
        box(c, -1.8f, -6.5f, 3.6f, 13, 0xffffff);
        box(c, -6.5f, -1.8f, 13, 3.6f, 0xffffff);
    } else if (code == "cn") {
        box(c, -15, -10, 30, 20, 0xde2910);
        dr::star(c, {-9, -4.5f}, 2.8f, 1.1f, 5, -90, rgb(0xffde00));
        for (Vector2 p : {Vector2{-4, -7}, {-2, -4}, {-2.5f, -0.5f}, {-5, 2}})
            dr::star(c, p, 0.85f, 0.34f, 5, -90, rgb(0xffde00));
    } else if (code == "cz") {
        box(c, -15, -10, 30, 10, 0xffffff);
        box(c, -15, 0, 30, 10, 0xd7141a);
        polygon(c, {{-15,-10}, {0,0}, {-15,10}}, 0x11457e);
    } else if (code == "de") horizontal(c, 0x000000, 0xdd0000, 0xffce00);
    else if (code == "dk") cross(c, 0xc8102e, 0xffffff, -3, 0, 2.7f);
    else if (code == "ee") horizontal(c, 0x4891d9, 0x000000, 0xffffff);
    else if (code == "fi") cross(c, 0xffffff, 0x002f6c, -3, 0, 3.2f);
    else if (code == "fr") vertical(c, 0x002395, 0xffffff, 0xed2939);
    else if (code == "gb") {
        box(c, -15, -10, 30, 20, 0x012169);
        polygon(c, {{-15,-10},{-12.7f,-10},{15,8.5f},{15,10},{12.7f,10},{-15,-8.5f}}, 0xffffff);
        polygon(c, {{15,-10},{12.7f,-10},{-15,8.5f},{-15,10},{-12.7f,10},{15,-8.5f}}, 0xffffff);
        polygon(c, {{-15,-10},{-13.8f,-10},{15,9.2f},{15,10}}, 0xc8102e);
        polygon(c, {{15,-10},{13.8f,-10},{-15,9.2f},{-15,10}}, 0xc8102e);
        box(c, -15, -2.7f, 30, 5.4f, 0xffffff);
        box(c, -3.5f, -10, 7, 20, 0xffffff);
        box(c, -15, -1.5f, 30, 3, 0xc8102e);
        box(c, -2, -10, 4, 20, 0xc8102e);
    } else if (code == "hu") horizontal(c, 0xce293d, 0xffffff, 0x477050);
    else if (code == "id" || code == "mc") { box(c,-15,-10,30,10,0xce1126); box(c,-15,0,30,10,0xffffff); }
    else if (code == "ie") vertical(c, 0x169b62, 0xffffff, 0xff883e);
    else if (code == "in") {
        horizontal(c, 0xff9933, 0xffffff, 0x138808);
        dr::ring(c, {0,0}, 2.2f, 0.35f, rgb(0x000080));
        for (int i = 0; i < 24; ++i) {
            const float a = (float)(i * PI / 12);
            dr::strokeSeg(c, {0,0}, {2.2f*std::cos(a), 2.2f*std::sin(a)}, 0.12f, rgb(0x000080));
        }
    } else if (code == "it") vertical(c, 0x009246, 0xffffff, 0xce2b37);
    else if (code == "jp") { box(c,-15,-10,30,20,0xffffff); dr::circle(c,{0,0},6,rgb(0xbc002d)); }
    else if (code == "lu") horizontal(c, 0xef3340, 0xffffff, 0x00a3e0);
    else if (code == "nl") horizontal(c, 0xae1c28, 0xffffff, 0x21468b);
    else if (code == "no") {
        cross(c, 0xba0c2f, 0xffffff, -3, 0, 4);
        box(c, -4.2f, -10, 2.4f, 20, 0x00205b);
        box(c, -15, -1.2f, 30, 2.4f, 0x00205b);
    } else if (code == "pl") { box(c,-15,-10,30,10,0xffffff); box(c,-15,0,30,10,0xdc143c); }
    else if (code == "ro") vertical(c, 0x002b7f, 0xfcd116, 0xce1126);
    else if (code == "ru") horizontal(c, 0xffffff, 0x0039a6, 0xd52b1e);
    else if (code == "se") cross(c, 0x006aa7, 0xfecc00, -3, 0, 2.7f);
    else if (code == "ua") { box(c,-15,-10,30,10,0x0057b7); box(c,-15,0,30,10,0xffd700); }
    else if (code == "us") {
        for (int i = 0; i < 13; ++i) box(c, -15, -10 + i * 20.f/13, 30, 20.f/13,
                                         i % 2 ? 0xffffff : 0xb22234);
        box(c, -15, -10, 12, 20.f*7/13, 0x3c3b6e);
        for (int row = 0; row < 9; ++row)
            for (int col = 0; col < (row % 2 ? 5 : 6); ++col)
                dr::star(c, {-14.1f + col*2.f + (row%2 ? 1.f : 0.f), -9.25f + row*1.3f},
                         0.35f, 0.14f, 5, -90, WHITE);
    } else if (code == "ca") {
        box(c,-15,-10,30,20,0xffffff);
        box(c,-15,-10,7.5f,20,0xff0000); box(c,7.5f,-10,7.5f,20,0xff0000);
        polygon(c, {{0,-7},{1.2f,-3},{3.5f,-4.2f},{2.7f,-1.5f},{5,0},{2,0.8f},
                    {2.6f,3.8f},{0.7f,3.2f},{0.7f,7},{-0.7f,7},{-0.7f,3.2f},
                    {-2.6f,3.8f},{-2,0.8f},{-5,0},{-2.7f,-1.5f},{-3.5f,-4.2f},{-1.2f,-3}}, 0xff0000);
    } else if (code == "br") {
        box(c,-15,-10,30,20,0x009739);
        polygon(c, {{0,-8},{13,0},{0,8},{-13,0}}, 0xffdf00);
        dr::circle(c,{0,0},5.2f,rgb(0x002776));
        for (int i = -3; i <= 3; ++i) dr::circle(c,{float(i*1.25f),float(i%3)-1},0.18f,WHITE);
        dr::strokeSeg(c,{-4.6f,-2},{4.6f,1.5f},0.8f,WHITE);
    }
}

float smooth(float x) { x = std::clamp(x, 0.f, 1.f); return x*x*(3-2*x); }
void checkEntrance(const std::string& in) {
    if (in != "pop" && in != "fade" && in != "none")
        throw std::runtime_error("flags: unknown entrance '" + in + "' (pop, fade or none)");
}
struct Phase { float scale = 1, alpha = 1; };
Phase entrance(const std::string& in, double t, int fps) {
    if (in == "none") return {};
    if (t < 0) return {0,0};
    if (in == "fade") return {1, smooth((float)t/12)};
    SpringOptions opt; opt.fps = fps; opt.config.damping = 12;
    return {(float)spring(t, opt), std::clamp((float)t/6, 0.f, 1.f)};
}

} // namespace

const std::vector<std::string>& countries() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> result;
        for (const Country& item : data) result.emplace_back(item.code);
        return result;
    }();
    return names;
}
std::string title(const std::string& code) { return country(code).name; }

void draw(const Ctx& c, const std::string& code, Vector2 centre, float width, bool badge, float reveal) {
    country(code);
    if (width <= 0 || reveal <= 0) return;
    Ctx k = c;
    k.o = mapP(c, centre);
    k.s *= width / (badge ? 38.f : 30.f) * smooth(reveal);
    if (badge) {
        dr::roundRect(k, -19, -19, 38, 38, 6, rgb(0x20283a));
        dr::roundRect(k, -16, -11, 32, 22, 2, rgb(0xffffff));
    }
    artwork(k, code);
}

void registerPrefabs() {
    for (const std::string& code : countries()) {
        prefab::add({"flag-" + code,
            Json::parse(R"({"size":160,"badge":true,"label":"","ink":"#f2f4ff","in":"pop","delay":0,"fps":30})"),
            [code](TNode& n, const Json& p) {
                const float size = (float)p.num("size", 160);
                const bool badge = p.flag("badge", true);
                const std::string label = p.str("label", ""), in = p.str("in", "pop");
                checkEntrance(in);
                const Color ink = p.color("ink", rgb(0xf2f4ff));
                const int delay = p.integer("delay", 0), fps = p.integer("fps", 30);
                n.fn = [=](const Ctx& c, int frame) {
                    const double t = frameTime(frame) - delay;
                    const Phase ph = entrance(in, t, fps);
                    if (ph.alpha <= 0) return;
                    Ctx k = c; k.a *= ph.alpha;
                    const float fs = size * 0.32f, gap = size * 0.2f;
                    Font font = fonts::ui(true);
                    const float textW = label.empty() ? 0 : dr::measure(font, label.c_str(), fs, 0).x;
                    const float x0 = -(size + (label.empty() ? 0 : gap + textW)) / 2;
                    draw(k, code, {x0 + size/2, 0}, size * ph.scale, badge);
                    if (!label.empty()) dr::text(font, k, label.c_str(), {x0+size+gap,0}, fs, 0, ink, 0, 0.5);
                };
            }});
    }
    prefab::add({"flag-row",
        Json::parse(R"({"countries":["at","de","fr","gb","us","jp"],"size":120,"gap":40,"badge":true,"label":"","ink":"#f2f4ff","stagger":4,"in":"pop","delay":0,"fps":30})"),
        [](TNode& n, const Json& p) {
            std::vector<std::string> list;
            for (const Json& item : p["countries"].asArray()) {
                list.push_back(item.asString()); country(list.back());
            }
            const float size = (float)p.num("size",120), gap = (float)p.num("gap",40);
            const bool badge = p.flag("badge",true);
            const std::string label = p.str("label",""), in = p.str("in","pop");
            checkEntrance(in);
            const Color ink = p.color("ink",rgb(0xf2f4ff));
            const double stagger = p.num("stagger",4);
            const int delay = p.integer("delay",0), fps = p.integer("fps",30);
            n.fn = [=](const Ctx& c, int frame) {
                const double t = frameTime(frame) - delay;
                const int count = (int)list.size();
                const float x0 = -(count*size + std::max(0,count-1)*gap)/2 + size/2;
                for (int i = 0; i < count; ++i) {
                    const Phase ph = entrance(in,t-i*stagger,fps);
                    if (ph.alpha <= 0) continue;
                    Ctx k = c; k.a *= ph.alpha;
                    draw(k,list[i],{x0+i*(size+gap),0},size*ph.scale,badge);
                }
                if (!label.empty() && t >= (count-1)*stagger)
                    dr::text(fonts::ui(true),c,label.c_str(),{0,size*0.95f},size*0.32f,0,ink,0.5,0.5);
            };
        }});
}

} // namespace rmtn::flags
