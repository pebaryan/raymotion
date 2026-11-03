// devices.cpp -- device prefabs round a g3d::Surface (see rmtn/devices.hpp).
#include "rmtn/devices.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/fonts.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace rmtn {
namespace g3d {
namespace {

// The parts of the picture outside its rounded corners, painted over in `col`. `mask` picks
// the corners: 1 top-left, 2 top-right, 4 bottom-right, 8 bottom-left.
void cornerCaps(const Ctx& c, Vector2 size, float r, Color col, int mask) {
    if (r <= 0) return;
    const float w = size.x, h = size.y;
    struct Corner { int bit; Vector2 tip, centre; float a0; };
    const Corner corners[4] = { { 1, { 0, 0 }, { r, r }, 180 }, { 2, { w, 0 }, { w - r, r }, 270 },
                                { 4, { w, h }, { w - r, h - r }, 0 }, { 8, { 0, h }, { r, h - r }, 90 } };
    for (const Corner& k : corners) {
        if (!(mask & k.bit)) continue;
        std::vector<Vector2> pts{ k.tip };
        for (int i = 0; i <= 12; i++) {
            const float a = (k.a0 + 90.f * i / 12) * DEG2RAD;
            pts.push_back({ k.centre.x + r * std::cos(a), k.centre.y + r * std::sin(a) });
        }
        dr::fillPoly(c, pts.data(), (int)pts.size(), col);
    }
}

Material solid(Color col, float metal, float rough) {
    Material m;
    m.color = col;
    m.metalness = metal;
    m.roughness = rough;
    return m;
}

// A slab lying flat (its depth along y), centred at `at`.
Object3D* flatSlab(Object3D& parent, float w, float d, float t, float r, Vector3 at, const Material& m) {
    Object3D* o = parent.add(Geometry::roundedSlab(w, d, t, r), m);
    o->position = at;
    o->rotation = { -90, 0, 0 };
    return o;
}

// The screen: the surface sized to `w` metres across (its height from its pixels), rounded
// corners painted in, placed on `parent` at `at`.
Object3D* screenOn(Object3D& parent, const std::shared_ptr<Surface>& ui, float w, float radius, Color capColor,
                   int capMask, Vector3 at) {
    const float px = std::max(1.f, ui->pixels.x);
    ui->size = { w, w * ui->pixels.y / px };
    const float rPx = radius * px / w;
    if (rPx > 0 && capMask) {
        auto inner = ui->draw;
        const Vector2 size = ui->pixels;
        ui->draw = [inner, size, rPx, capColor, capMask](const Ctx& c, int t) {
            if (inner) inner(c, t);
            cornerCaps(c, size, rPx, capColor, capMask);
        };
    }
    Object3D* o = parent.add(ui);
    o->position = at;
    return o;
}

std::shared_ptr<Surface> keyboard(Color body) {
    auto k = std::make_shared<Surface>();
    k->pixels = { 1000, 380 };
    k->background = { (unsigned char)(body.r * 0.9f), (unsigned char)(body.g * 0.9f), (unsigned char)(body.b * 0.9f), 255 };   // the deck
    const Color key{ 26, 28, 32, 255 };                                                   // dark keys, on any case
    k->draw = [key](const Ctx& c, int) {
        constexpr float gap = 8, left = 12, span = 976;
        auto row = [&](float y, float kh, const std::vector<float>& units) {   // key widths in units, filling the row
            float sum = 0;
            for (float u : units) sum += u;
            const float unit = (span - gap * (units.size() - 1)) / sum;
            float x = left;
            for (float u : units) {
                dr::roundRect(c, x, y, unit * u, kh, 7, key);
                x += unit * u + gap;
            }
        };
        row(21, 34, std::vector<float>(14, 1));                                // function keys
        for (int r = 1; r <= 4; r++) row(12 + r * 61, 52, std::vector<float>(r == 4 ? 12 : 14 - (r == 3), 1));
        row(12 + 5 * 61, 52, { 1, 1, 1, 1.3f, 5.4f, 1.3f, 1, 1, 1 });         // the space bar
    };
    return k;
}

std::shared_ptr<Surface> addressBar(Vector2 pixels, const DeviceStyle& st) {
    auto b = std::make_shared<Surface>();
    const float h = std::round(pixels.x * 0.044f);
    b->pixels = { pixels.x, h };
    b->background = st.chrome;
    const std::string url = st.url;
    const Color field{ (unsigned char)std::min(255, st.chrome.r + 14), (unsigned char)std::min(255, st.chrome.g + 14),
                       (unsigned char)std::min(255, st.chrome.b + 14), 255 };
    const float w = pixels.x;
    b->draw = [url, field, w, h](const Ctx& c, int) {
        const float r = h * 0.12f, cy = h * 0.5f;
        const Color dots[3] = { { 255, 95, 87, 255 }, { 254, 188, 46, 255 }, { 40, 200, 64, 255 } };
        for (int i = 0; i < 3; i++) dr::circle(c, { h * 0.55f + i * h * 0.42f, cy }, r, dots[i]);
        const float fw = std::min(w * 0.5f, w - h * 4), fx = (w - fw) / 2;
        dr::roundRect(c, fx, h * 0.2f, fw, h * 0.6f, h * 0.3f, field);
        dr::text(fonts::ui(false), c, url.c_str(), { w / 2, cy + h * 0.02f }, h * 0.34f, 0, { 90, 96, 108, 255 }, 0.5f, 0.5f);
    };
    return b;
}

} // namespace

static Device build(Object3D* root, DeviceKind kind, std::shared_ptr<Surface> ui, const DeviceStyle& st) {
    Device dv;
    dv.ui = ui;
    dv.root = root;
    const Material body = solid(st.body, st.metalness, st.roughness);
    const Material glass = solid(st.bezel, 0.f, 0.12f);
    const float aspect = std::max(1.f, ui->pixels.y) / std::max(1.f, ui->pixels.x);
    switch (kind) {
        case DeviceKind::Phone:
        case DeviceKind::Tablet: {
            const bool phone = kind == DeviceKind::Phone;
            const float w = st.width > 0 ? st.width : phone ? 0.0715f : 0.18f, h = w * aspect;
            const float bezel = phone ? 0.0022f : 0.009f, depth = phone ? 0.0078f : 0.0065f;
            const float screenR = phone ? 0.0085f * w / 0.0715f : 0.006f, bodyR = screenR + bezel;
            const float bw = w + 2 * bezel, bh = h + 2 * bezel;
            root->add(Geometry::roundedSlab(bw, bh, depth, bodyR), body);
            root->add(Geometry::roundedSlab(bw - 0.0008f, bh - 0.0008f, 0.0004f, bodyR - 0.0004f), glass)->position = { 0, 0, depth / 2 + 0.0001f };
            dv.screen = screenOn(*root, ui, w, screenR, st.bezel, 15, { 0, 0, depth / 2 + 0.0006f });
            if (phone) {                                          // side keys
                const Material keys = solid(st.body, st.metalness, st.roughness * 0.8f);
                root->add(Geometry::box(0.0012f, 0.010f, 0.003f), keys)->position = { -bw / 2 - 0.0004f, bh * 0.22f, 0 };
                root->add(Geometry::box(0.0012f, 0.010f, 0.003f), keys)->position = { -bw / 2 - 0.0004f, bh * 0.14f, 0 };
                root->add(Geometry::box(0.0012f, 0.016f, 0.003f), keys)->position = { bw / 2 + 0.0004f, bh * 0.18f, 0 };
            }
            dv.screenSize = { w, h };
            break;
        }
        case DeviceKind::Laptop: {
            const float w = st.width > 0 ? st.width : 0.30f, h = w * aspect;
            const float side = 0.008f * w / 0.30f, top = 0.010f * w / 0.30f, chin = 0.018f * w / 0.30f;
            const float pw = w + 2 * side, ph = h + top + chin, lidT = 0.005f * w / 0.30f;
            const float baseD = ph * 0.95f, baseT = 0.014f * w / 0.30f, r = 0.012f * w / 0.30f;
            flatSlab(*root, pw, baseD, baseT, r, { 0, baseT / 2, 0 }, body);
            const auto keys = keyboard(st.body);
            keys->size = { pw * 0.86f, pw * 0.86f * keys->pixels.y / keys->pixels.x };
            Object3D* kb = root->add(keys);
            kb->material.unlit = false;
            kb->position = { 0, baseT + 0.0003f, -baseD * 0.13f };
            kb->rotation = { -90, 0, 0 };
            flatSlab(*root, pw * 0.38f, baseD * 0.30f, 0.0004f, r * 0.5f, { 0, baseT + 0.0002f, baseD * 0.28f },
                     solid({ (unsigned char)std::min(255, st.body.r + 10), (unsigned char)std::min(255, st.body.g + 10),
                             (unsigned char)std::min(255, st.body.b + 12), 255 }, st.metalness, st.roughness * 0.6f));
            Object3D* lid = dv.lid = root->add();
            lid->position = { 0, baseT, -baseD / 2 + lidT };
            lid->rotation = { 90 - st.lidOpen, 0, 0 };
            Object3D* panel = lid->add(Geometry::roundedSlab(pw, ph, lidT, r), body);
            panel->position = { 0, ph / 2, -lidT / 2 };
            lid->add(Geometry::roundedSlab(pw - 0.001f, ph - 0.001f, 0.0004f, r - 0.0005f), glass)->position = { 0, ph / 2, 0.0002f };
            dv.screen = screenOn(*lid, ui, w, 0.003f * w / 0.30f, st.bezel, 3, { 0, chin + h / 2, 0.0007f });
            dv.screenSize = { w, h };
            break;
        }
        case DeviceKind::Monitor: {
            const float w = st.width > 0 ? st.width : 0.60f, h = w * aspect, k = w / 0.60f;
            const float side = 0.008f * k, chin = 0.02f * k, depth = 0.02f * k, r = 0.006f * k;
            const float pw = w + 2 * side, ph = h + side + chin, lift = 0.11f * k;
            flatSlab(*root, 0.24f * k, 0.17f * k, 0.01f * k, 0.02f * k, { 0, 0.005f * k, 0.02f * k }, body);
            root->add(Geometry::box(0.05f * k, lift + ph * 0.5f, 0.018f * k), body)->position = { 0, (lift + ph * 0.5f) / 2 + 0.01f * k, -depth };
            Object3D* panel = root->add();
            panel->position = { 0, lift + ph / 2, 0 };
            panel->add(Geometry::roundedSlab(pw, ph, depth, r), body);
            panel->add(Geometry::roundedSlab(pw - 0.001f, ph - 0.001f, 0.0004f, r - 0.0005f), glass)->position = { 0, 0, depth / 2 + 0.0001f };
            dv.screen = screenOn(*panel, ui, w, 0, st.bezel, 0, { 0, (chin - side) / 2, depth / 2 + 0.0006f });
            dv.screenSize = { w, h };
            break;
        }
        case DeviceKind::Browser: {
            const float w = st.width > 0 ? st.width : 0.80f, h = w * aspect, k = w / 0.80f;
            const auto bar = addressBar(ui->pixels, st);
            const float barH = w * bar->pixels.y / std::max(1.f, bar->pixels.x);
            const float frame = 0.003f * k, r = 0.012f * k, depth = 0.004f * k;
            const float fw = w + 2 * frame, fh = h + barH + 2 * frame;
            root->add(Geometry::roundedSlab(fw, fh, depth, r + frame), body);
            const float barY = fh / 2 - frame - barH / 2, pageY = barY - barH / 2 - h / 2;
            screenOn(*root, bar, w, r, st.body, 3, { 0, barY, depth / 2 + 0.0005f });
            dv.screen = screenOn(*root, ui, w, r, st.body, 12, { 0, pageY, depth / 2 + 0.0005f });
            dv.screenSize = { w, h };
            break;
        }
    }
    return dv;
}

Device addDevice(Object3D& parent, DeviceKind kind, std::shared_ptr<Surface> ui, const DeviceStyle& style) {
    return build(parent.add(), kind, std::move(ui), style);
}
Device addDevice(Scene& scene, DeviceKind kind, std::shared_ptr<Surface> ui, const DeviceStyle& style) {
    return build(scene.add(), kind, std::move(ui), style);
}

} // namespace g3d
} // namespace rmtn
