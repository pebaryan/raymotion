// rig:: faces -- the procedural face and SVG faces of named shapes.
#include "rmtn/rig.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <stdexcept>

namespace rmtn {
namespace rig {

namespace {
float lerpf(float a, float b, float u) { return a + (b - a) * u; }
Color lerpc(Color a, Color b, float u) {
    return { (unsigned char)std::lround(lerpf(a.r, b.r, u)), (unsigned char)std::lround(lerpf(a.g, b.g, u)),
             (unsigned char)std::lround(lerpf(a.b, b.b, u)), (unsigned char)std::lround(lerpf(a.a, b.a, u)) };
}
Color faded(Color c, float a) { c.a = (unsigned char)std::lround(c.a * std::clamp(a, 0.f, 1.f)); return c; }

// ------------------------------------------------------------------ procedural face
struct Params {
    float eyeOpen = 1, smile = 0, lid = 0, browY = 0, browTilt = 0;
    float curve = 0.15f, width = 0.30f, open = 0, round = 0, skew = 0, teeth = 0;
    float winkL = 0, pupil = 1;
};

Params mixParams(const Params& a, const Params& b, float u) {
    Params r;
    r.eyeOpen = lerpf(a.eyeOpen, b.eyeOpen, u); r.smile = lerpf(a.smile, b.smile, u); r.lid = lerpf(a.lid, b.lid, u);
    r.browY = lerpf(a.browY, b.browY, u); r.browTilt = lerpf(a.browTilt, b.browTilt, u);
    r.curve = lerpf(a.curve, b.curve, u); r.width = lerpf(a.width, b.width, u); r.open = lerpf(a.open, b.open, u);
    r.round = lerpf(a.round, b.round, u); r.skew = lerpf(a.skew, b.skew, u); r.teeth = lerpf(a.teeth, b.teeth, u);
    r.winkL = lerpf(a.winkL, b.winkL, u); r.pupil = lerpf(a.pupil, b.pupil, u);
    return r;
}

Params expressionParams(const std::string& e) {
    Params p;
    if (e == "happy") { p.curve = 0.9f; p.width = 0.42f; p.smile = 0.45f; p.browY = 0.05f; p.open = 0.28f; p.teeth = 1; }
    else if (e == "sad") { p.curve = -0.6f; p.width = 0.28f; p.browTilt = 0.9f; p.eyeOpen = 0.9f; p.lid = 0.15f; }
    else if (e == "angry") { p.curve = -0.45f; p.width = 0.32f; p.browTilt = -1.0f; p.browY = -0.05f; p.lid = 0.3f; }
    else if (e == "surprised") { p.curve = 0; p.width = 0.18f; p.open = 0.85f; p.round = 1; p.browY = 0.16f;
                                 p.eyeOpen = 1.12f; p.pupil = 0.8f; }
    else if (e == "wink") { p.curve = 0.75f; p.width = 0.38f; p.winkL = 1; p.browY = 0.03f; p.skew = 0.3f; }
    else if (e == "sleepy") { p.lid = 0.6f; p.browY = -0.03f; p.curve = 0.05f; p.width = 0.24f; }
    else if (e == "smug") { p.curve = 0.45f; p.width = 0.3f; p.lid = 0.35f; p.browTilt = -0.25f; p.skew = 0.6f; }
    return p;
}

// The mouth for a speech shape, keeping some of the expression's curve.
Params mouthParams(char m, const Params& e) {
    if (m == 'X' || m == 0) return e;
    Params p = e;
    p.curve = e.curve * 0.4f;
    p.skew = e.skew * 0.5f;
    p.round = 0;
    p.teeth = 0;
    switch (m) {
    case 'A': p.open = 0; p.width = 0.26f; break;
    case 'B': p.open = 0.18f; p.width = 0.34f; p.teeth = 1; break;
    case 'C': p.open = 0.42f; p.width = 0.34f; p.teeth = 0.6f; break;
    case 'D': p.open = 0.8f; p.width = 0.38f; p.teeth = 0.5f; break;
    case 'E': p.open = 0.52f; p.width = 0.25f; p.round = 0.6f; break;
    case 'F': p.open = 0.3f; p.width = 0.14f; p.round = 1; break;
    case 'G': p.open = 0.1f; p.width = 0.30f; p.teeth = 1; break;
    case 'H': p.open = 0.45f; p.width = 0.30f; p.teeth = 0.4f; break;
    default: break;
    }
    return p;
}

class SimpleFace : public Face {
public:
    explicit SimpleFace(FaceStyle s) : st_(s) {}

    void draw(const Ctx& c, Rectangle box, const FaceState& s) const override {
        const Params ex = mixParams(expressionParams(s.previous), expressionParams(s.expression), s.mix);
        const Params mo = mixParams(mouthParams(s.previousMouth, ex), mouthParams(s.mouth, ex), s.mouthMix);
        Params p = ex;
        p.curve = mo.curve; p.width = mo.width; p.open = mo.open; p.round = mo.round; p.skew = mo.skew; p.teeth = mo.teeth;

        const float w = box.width, h = box.height, cx = box.x + w / 2;
        const float lw = w * 0.014f * st_.lineWeight;
        if (st_.blush.a > 0)
            for (int side : { -1, 1 })
                vg::fill(c, vg::Path::ellipse({ cx + side * w * 0.31f, box.y + h * 0.62f }, w * 0.09f, h * 0.05f), st_.blush);
        for (int side : { -1, 1 }) eye(c, box, s, p, side, lw);
        if (st_.brows) for (int side : { -1, 1 }) brow(c, box, p, side);
        mouth(c, box, p, lw);
    }

private:
    FaceStyle st_;

    // side -1: the eye on the viewer's left (the character's right).
    void eye(const Ctx& c, Rectangle box, const FaceState& s, const Params& p, int side, float lw) const {
        const float w = box.width, h = box.height;
        const Vector2 e{ box.x + w / 2 + side * w * 0.22f * st_.eyeSpacing, box.y + h * 0.36f };
        const bool dots = st_.eyes == FaceStyle::Eyes::Dots;
        const float rx = w * (dots ? 0.06f : 0.105f) * st_.eyeSize;
        const float ry = w * (dots ? 0.085f : 0.135f) * st_.eyeSize * p.eyeOpen;
        const float wink = side > 0 ? p.winkL : 0;          // the character's left eye winks
        const float cover = std::clamp(std::max({ p.lid, s.blink, wink }), 0.f, 1.f);
        const Vector2 gaze{ s.look.x * rx * 0.38f, s.look.y * ry * 0.3f };

        if (cover < 0.97f) {
            if (dots) {
                vg::fill(c, vg::Path::ellipse({ e.x + gaze.x * 0.5f, e.y + gaze.y * 0.5f }, rx, ry), st_.ink);
                vg::fill(c, vg::Path::circle({ e.x - rx * 0.3f + gaze.x * 0.5f, e.y - ry * 0.35f }, rx * 0.28f), faded(WHITE, 0.85f));
            } else {
                vg::fill(c, vg::Path::ellipse(e, rx, ry), st_.white);
                const float ir = rx * 0.68f * p.pupil;
                const Vector2 ic{ e.x + gaze.x, e.y + gaze.y + ry * 0.08f };
                vg::fill(c, vg::Path::circle(ic, ir), st_.iris);
                vg::fill(c, vg::Path::circle(ic, ir * 0.55f), st_.ink);
                vg::fill(c, vg::Path::circle({ ic.x - ir * 0.35f, ic.y - ir * 0.4f }, ir * 0.28f), WHITE);
                vg::stroke(c, vg::Path::ellipse(e, rx, ry), vg::Stroke(st_.ink, lw));
            }
        }
        // Upper lid: skin down to `cover`, its edge inked.
        const Color lidSkin = s.skin;
        if (cover > 0.01f) {
            const float top = e.y - ry * 1.35f, ly = e.y - ry + 2 * ry * cover, half = rx * 1.25f;
            vg::Path lid;
            lid.moveTo({ e.x - half, top }).lineTo({ e.x + half, top }).lineTo({ e.x + half, ly })
               .quadTo({ e.x, ly + ry * 0.35f * (1 - cover) }, { e.x - half, ly }).close();
            vg::fill(c, lid, lidSkin);
            if (cover >= 0.97f) {
                // Shut: a lash line, smiling for a wink or a happy face.
                const float bend = (wink > 0.5f || p.smile > 0.2f) ? -ry * 0.45f : ry * 0.3f;
                vg::stroke(c, vg::Path().moveTo({ e.x - rx * 1.05f, e.y + ry * 0.1f })
                                        .quadTo({ e.x, e.y + ry * 0.1f + bend }, { e.x + rx * 1.05f, e.y + ry * 0.1f }),
                           vg::Stroke(st_.ink, lw * 1.4f));
            } else {
                const float d = (ly - e.y) / ry, chord = rx * std::sqrt(std::max(0.f, 1 - d * d));
                if (chord > rx * 0.1f)
                    vg::stroke(c, vg::Path().moveTo({ e.x - chord, ly })
                                            .quadTo({ e.x, ly + ry * 0.3f * (1 - cover) }, { e.x + chord, ly }),
                               vg::Stroke(st_.ink, lw * 1.2f));
            }
        }
        // Lower lid for a smile: an arch of skin pushing up from below.
        if (p.smile > 0.01f && cover < 0.97f) {
            const float ly = e.y + ry - 2 * ry * p.smile * 0.55f, half = rx * 1.3f, bottom = e.y + ry * 1.4f;
            vg::Path lid;
            lid.moveTo({ e.x - half, bottom }).lineTo({ e.x - half, ly + ry * 0.2f })
               .quadTo({ e.x, ly - ry * 0.55f * p.smile }, { e.x + half, ly + ry * 0.2f }).lineTo({ e.x + half, bottom }).close();
            vg::fill(c, lid, lidSkin);
        }
    }

    void brow(const Ctx& c, Rectangle box, const Params& p, int side) const {
        const float w = box.width, h = box.height;
        const float ex = box.x + w / 2 + side * w * 0.22f * st_.eyeSpacing;
        const float y = box.y + h * (0.14f - p.browY) - (st_.eyes == FaceStyle::Eyes::Dots ? -h * 0.04f : 0.f);
        const float half = w * 0.11f;
        const float inner = -p.browTilt * h * 0.06f;      // inner end: up for sad, down for angry
        const Vector2 a{ ex - side * half, y + inner }, b{ ex + side * half, y + h * 0.01f };
        const Vector2 mid{ (a.x + b.x) / 2, std::min(a.y, b.y) - h * 0.03f };
        vg::Stroke s(st_.ink, w * 0.028f * st_.browWeight);
        vg::stroke(c, vg::Path().moveTo(a).quadTo(mid, b), s);
    }

    void mouth(const Ctx& c, Rectangle box, const Params& p, float lw) const {
        const float w = box.width, h = box.height, cx = box.x + w / 2, my = box.y + h * 0.78f;
        const float W = std::max(0.04f, p.width) * w;
        if (p.open < 0.06f) {
            const Vector2 l{ cx - W / 2, my - p.curve * h * 0.06f - p.skew * h * 0.03f };
            const Vector2 r{ cx + W / 2, my - p.curve * h * 0.06f + p.skew * h * 0.03f };
            vg::stroke(c, vg::Path().moveTo(l).quadTo({ cx, my + p.curve * h * 0.1f }, r), vg::Stroke(st_.ink, lw * 1.3f));
            return;
        }
        const float upper = h * p.open * lerpf(0.03f, 0.08f, p.round), lower = h * p.open * 0.13f;
        auto blob = [&](float scaleW, float up, float low) {
            std::vector<Vector2> pts;
            const int n = 28;
            for (int i = 0; i < n; i++) {
                const float th = 2 * PI * i / n, co = std::cos(th), si = std::sin(th);
                const float sharp = lerpf(1.6f, 1.0f, p.round);        // pointed corners unless rounded
                const float x = (co < 0 ? -1.f : 1.f) * std::pow(std::fabs(co), 1 / sharp);
                float y = si > 0 ? low * si : up * si;
                y -= (p.curve * h * 0.07f) * co * co - p.skew * h * 0.03f * co;
                pts.push_back({ cx + x * W / 2 * scaleW, my + y });
            }
            return vg::Path::smooth(pts, true);
        };
        const vg::Path shape = blob(1, upper, lower);
        vg::fill(c, shape, st_.mouth);
        if (p.teeth > 0.05f && p.round < 0.7f)
            vg::fill(c, blob(0.82f, upper * 0.85f, std::min(lower * 0.3f, h * 0.025f)), faded(WHITE, p.teeth));
        if (p.open > 0.35f)
            vg::fill(c, vg::Path::ellipse({ cx, my + lower * 0.62f }, W * 0.2f, lower * 0.28f), lerpc(st_.mouth, { 230, 110, 120, 255 }, 0.55f));
        vg::stroke(c, shape, vg::Stroke(st_.ink, lw));
    }
};

// ------------------------------------------------------------------ SVG faces
const char* const kExpressions[] = { "neutral", "happy", "sad", "angry", "surprised", "wink", "sleepy", "smug" };
const char* const kBases[] = { "eye-L", "eye-R", "pupil-L", "pupil-R", "brow-L", "brow-R", "mouth" };

class SvgFace : public Face {
public:
    explicit SvgFace(vg::Drawing d) : d_(std::move(d)) {
        for (size_t i = 0; i < d_.shapes.size(); i++) {
            const std::string& id = d_.shapes[i].id;
            ids_[id] = (int)i;
            std::string base;
            for (const char* b : kBases)
                if (id == b || id.rfind(std::string(b) + "-", 0) == 0) { base = b; break; }
            if (!base.empty()) {
                if (!firstOf_.count(base)) firstOf_[base] = (int)i;
                continue;
            }
            for (const char* e : kExpressions) {
                const std::string suffix = std::string("-") + e;
                if (id.size() > suffix.size() && id.compare(id.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    only_[(int)i] = e;
                    break;
                }
            }
        }
    }

    void draw(const Ctx& c0, Rectangle box, const FaceState& s) const override {
        const Ctx c = vg::fit(c0, d_, box);
        std::map<int, std::string> baseAt;          // the paint position each base draws at
        for (const auto& [base, i] : firstOf_) baseAt[i] = base;
        for (size_t i = 0; i < d_.shapes.size(); i++) {
            auto b = baseAt.find((int)i);
            if (b != baseAt.end()) { drawBase(c, b->second, s); continue; }
            const std::string& id = d_.shapes[i].id;
            bool isBasePart = false;
            for (const char* base : kBases)
                if (id == base || id.rfind(std::string(base) + "-", 0) == 0) isBasePart = true;
            if (isBasePart) continue;
            float alpha = 1;
            auto o = only_.find((int)i);
            if (o != only_.end()) alpha = s.weight(o->second);
            if (alpha > 0.001f) vg::draw(c, d_.shapes[i].path, fade(d_.shapes[i].style, alpha));
        }
    }

private:
    vg::Drawing d_;
    std::map<std::string, int> ids_, firstOf_;
    std::map<int, std::string> only_;
    mutable std::mutex mu_;
    mutable std::map<std::pair<int, int>, vg::Morph> morphs_;

    static vg::Style fade(vg::Style st, float a) {
        st.fill = faded(st.fill, a);
        st.stroke.color = faded(st.stroke.color, a);
        return st;
    }
    int find(const std::string& id) const {
        auto it = ids_.find(id);
        return it == ids_.end() ? -1 : it->second;
    }
    // The shape for `base` under expression `e` (its variant, else the plain one).
    int variant(const std::string& base, const std::string& e) const {
        const int v = find(base + "-" + e);
        return v >= 0 ? v : find(base);
    }
    vg::Path morph(int a, int b, float u) const {
        if (a == b || u <= 0) return d_.shapes[(size_t)a].path;
        if (u >= 1) return d_.shapes[(size_t)b].path;
        std::lock_guard<std::mutex> lock(mu_);
        auto key = std::make_pair(a, b);
        auto it = morphs_.find(key);
        if (it == morphs_.end())
            it = morphs_.emplace(key, vg::Morph(d_.shapes[(size_t)a].path, d_.shapes[(size_t)b].path)).first;
        return it->second.at(u);
    }
    vg::Style style(int a, int b, float u) const {
        return vg::mix(d_.shapes[(size_t)a].style, d_.shapes[(size_t)b].style, u);
    }
    // Draws `base` crossfaded between two shapes (either may be -1).
    void blend(const Ctx& c, int a, int b, float u, const std::function<vg::Path(vg::Path)>& shape = nullptr) const {
        if (a < 0) a = b;
        if (b < 0) b = a;
        if (a < 0) return;
        vg::Path p = morph(a, b, u);
        if (shape) p = shape(p);
        vg::draw(c, p, style(a, b, u));
    }

    void drawBase(const Ctx& c, const std::string& base, const FaceState& s) const {
        if (base == "mouth") {
            auto rest = [&](const std::string& e) { return variant("mouth", e); };
            auto shapeFor = [&](char m, const std::string& e) {
                const int v = m != 'X' && m != 0 ? find(std::string("mouth-") + m) : -1;
                return v >= 0 ? v : rest(e);
            };
            if (s.mouth != 'X' || s.mouthMix < 1)
                blend(c, shapeFor(s.previousMouth, s.expression), shapeFor(s.mouth, s.expression), s.mouthMix);
            else blend(c, rest(s.previous), rest(s.expression), s.mix);
            return;
        }
        if (base == "brow-L" || base == "brow-R") {
            blend(c, variant(base, s.previous), variant(base, s.expression), s.mix);
            return;
        }
        const bool left = base.back() == 'L';
        const std::string eyeBase = left ? "eye-L" : "eye-R";
        const float wink = left ? s.weight("wink") : 0.f;
        const float shut = std::clamp(std::max(s.blink, wink), 0.f, 1.f);
        const int eyeA = variant(eyeBase, s.previous), eyeB = variant(eyeBase, s.expression);
        if (eyeA < 0 && eyeB < 0) return;
        const Rectangle eb = d_.shapes[(size_t)(eyeB >= 0 ? eyeB : eyeA)].path.bounds();
        const Vector2 centre{ eb.x + eb.width / 2, eb.y + eb.height / 2 };
        const int closed = find(eyeBase + "-closed");
        auto squash = [&](vg::Path p) {
            if (closed >= 0 || shut <= 0) return p;
            return p.scaled(1, std::max(0.06f, 1 - shut), centre);
        };
        if (base[0] == 'e') {
            if (closed >= 0 && shut > 0) {
                const int open = s.mix >= 0.5f ? eyeB : eyeA;
                blend(c, open, closed, shut);
            } else blend(c, eyeA, eyeB, s.mix, squash);
            return;
        }
        // Pupils follow the gaze and hide as the eye shuts.
        if (shut > 0.8f && closed >= 0) return;
        const int pa = variant(base, s.previous), pb = variant(base, s.expression);
        const Vector2 d{ s.look.x * eb.width * 0.22f, s.look.y * eb.height * 0.22f };
        blend(c, pa, pb, s.mix, [&](vg::Path p) { return squash(p.translated(d)); });
    }
};
} // namespace

FacePtr simpleFace(FaceStyle style) { return std::make_shared<SimpleFace>(style); }
FacePtr svgFace(const std::string& file) { return std::make_shared<SvgFace>(vg::Drawing::load(file)); }
FacePtr svgFaceFromText(const std::string& svgText) { return std::make_shared<SvgFace>(vg::Drawing::parse(svgText)); }

} // namespace rig
} // namespace rmtn
