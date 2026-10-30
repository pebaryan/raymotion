// Shared by the rig's 3D builder and 2D puppet: shape surfaces, limb curves, colours.
#pragma once
#include "rmtn/rig.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace rmtn {
namespace rig {
namespace detail {

inline float spow(float v, float e) { return (v < 0 ? -1.f : 1.f) * std::pow(std::fabs(v), e); }

// The point of the unit superellipse |x|^(2/e) + |y|^(2/e) = 1 in direction `g` (radians
// from the x axis): (cos g, sin g) scaled onto the curve. Sampling by direction spreads
// vertices evenly round boxy shapes, and it stays exact where cos or sin is ~0 (raising
// those to 1/e directly would fold the surface along its axes).
inline Vector2 superDir(float g, float e) {
    const float c = std::cos(g), s = std::sin(g), q = 2 / e;
    const float n = std::pow(std::pow(std::fabs(c), q) + std::pow(std::fabs(s), q), 1 / q);
    return { c / n, s / n };
}

// A point of a Super prim in its own frame (centred, before at/rot): latitude `lat`
// (-pi/2 bottom .. pi/2 top) and longitude `lon` (0 = +x, pi/2 = +z front), both
// directions from the centre.
inline Vector3 superPoint(const Prim& p, float lat, float lon) {
    const float eV = std::max(0.08f, p.squareV), eH = std::max(0.08f, p.squareH);
    const Vector2 v = superDir(lat, eV), h = superDir(lon, eH);
    const float A = p.size.x / 2, B = p.size.y / 2, C = p.size.z / 2;
    const float y = B * v.y;
    const float k = 1 + (p.taper - 1) * (y / std::max(1e-6f, B) + 1) / 2;   // bottom 1 .. top `taper`
    return { A * v.x * h.x * k, y, C * v.x * h.y * k };
}

// The latitude range a Super covers (from/to are sines).
inline float latFrom(const Prim& p) { return std::asin(std::clamp(p.from, -1.f, 1.f)); }
inline float latTo(const Prim& p) { return std::asin(std::clamp(p.to, -1.f, 1.f)); }

// A prim's frame relative to its joint, scaled about its centre by `pop`.
inline Matrix primMatrix(const Prim& p, float pop) {
    return MatrixMultiply(MatrixMultiply(MatrixScale(pop, pop, pop), MatrixRotateXYZ(Vector3Scale(p.rot, DEG2RAD))),
                          MatrixTranslate(p.at.x, p.at.y, p.at.z));
}

// A limb's centreline from joint a through b to c, `n` + 1 points: a polyline at
// softness 0, the quadratic through b at 1 (it passes b at the same share of its length).
inline std::vector<Vector3> tubeCurve(const Frames& f, const Prim& p, int n) {
    const Vector3 A = position(f, p.a), B = position(f, p.b), C = position(f, p.c);
    const float l1 = Vector3Distance(A, B), l2 = Vector3Distance(B, C);
    const float tau = std::clamp(l1 / std::max(1e-6f, l1 + l2), 0.15f, 0.85f);
    const float w = std::clamp(p.softness, 0.f, 1.f) * 0.9f + 0.1f;
    const Vector3 Q = Vector3Scale(Vector3Subtract(Vector3Subtract(B, Vector3Scale(A, (1 - tau) * (1 - tau))), Vector3Scale(C, tau * tau)),
                                   1 / (2 * tau * (1 - tau)));
    std::vector<Vector3> pts;
    for (int i = 0; i <= n; i++) {
        const float t = (float)i / n, mt = 1 - t;
        const Vector3 bez = Vector3Add(Vector3Add(Vector3Scale(A, mt * mt), Vector3Scale(Q, 2 * mt * t)), Vector3Scale(C, t * t));
        const Vector3 lin = t <= tau ? Vector3Lerp(A, B, t / tau) : Vector3Lerp(B, C, (t - tau) / (1 - tau));
        pts.push_back(Vector3Lerp(lin, bez, w));
    }
    return pts;
}

// An outfit's dominant colour over a part's strip (v0..v1 of its region): what a flat fill
// shows when it cannot carry the texture. Cached by outfit id.
Color outfitColor(const Outfit& o, Region r, float v0, float v1);
// A Paint::Label face: `bg` filled over `box` (when `fill`), then `text` (lines split at
// \n) centred and fitted, in ink that reads on `bg`.
void drawLabel(const Ctx& c, Rectangle box, const std::string& text, Color bg, bool fill = true);
// The dominant colour of each of `n` rows down that strip, top first: horizontal stripes
// survive, a small motif does not. Cached by outfit id.
std::vector<Color> outfitRows(const Outfit& o, Region r, float v0, float v1, int n);

inline Color paintColor(const Look& l, const Prim& p) {
    switch (p.paint) {
    case Paint::Skin: case Paint::Face: return l.skin;
    case Paint::Hair: return l.hairColor;
    case Paint::Shoes: return l.shoes;
    case Paint::Accent: return l.accent;
    case Paint::Metal: return l.metal;
    case Paint::Dark: return l.dark;
    case Paint::Fixed: case Paint::Label: return p.color;
    case Paint::Outfit:
        return outfitColor(l.outfit, p.region, p.v0, p.v1);
    }
    return WHITE;
}

// ---- props sized to the body that uses them (shared by the parts and the clips that pose
// hands and feet on them). k scales real sizes to the build: 1 for a 1.75 m figure.
inline float propScale(const Build& b) { return b.height() / 1.75f; }
inline float hipHeight(const Build& b) { return b.ankle + b.shin + b.thigh; }
struct BoardGeom { float top, length, width, wheel; };          // skateboard along x
inline BoardGeom boardGeom(const Build& b) {
    const float k = propScale(b);
    return { 0.11f * k, 0.80f * k, 0.21f * k, 0.028f * k };
}
struct ScooterGeom { float top, length, width, wheel, barY, barZ, barW; };   // along +z
inline ScooterGeom scooterGeom(const Build& b) {
    const float k = propScale(b);
    ScooterGeom g{ 0.09f * k, 0.56f * k, 0.13f * k, 0.05f * k, 0, 0, 0 };
    g.barY = g.top + hipHeight(b) + b.torso.y * 0.22f;         // about the navel
    g.barZ = g.length * 0.5f + 0.04f * k;
    g.barW = std::max(b.torso.x * 1.5f, 0.42f * k);
    return g;
}
// A placard: its pole runs up through the fists (the upper fist `grip` from the bottom,
// at height `hand` when held up), long enough that the board clears the bearer's head.
struct SignGeom { float pole, grip, width, height, radius, hand; };
inline SignGeom signGeom(const Build& b) {
    const float k = propScale(b);
    SignGeom g{ 0, 0.30f * k, 0.62f * k, 0.44f * k, 0.016f * k, 0 };
    const float shoulder = hipHeight(b) + b.hips / 2 + b.torso.y - b.armRadius * 1.2f;
    g.hand = shoulder + 0.42f * (b.upperArm + b.forearm);
    g.pole = b.height() + 0.06f * k - g.hand + g.height + g.grip;
    return g;
}
struct ShieldGeom { float width, height, below; };               // `below`: under the fist
inline ShieldGeom shieldGeom(const Build& b) {
    const float k = propScale(b);
    return { 0.56f * k, 1.0f * k, 0.55f * k };
}

// The slot keys in drawing order.
// The animal parts (rig_creature.cpp), added to the registry with the built-ins.
void creatureParts(std::map<std::string, PartDef>& registry);
// A pose carried by a character's mounts (rig_act.cpp).
Pose mounted(const Character& ch, double seconds, Pose p);
// Apes (rig_ape.cpp): their head parts, their looks (false: not an ape) and knuckle-walking.
void apeParts(std::map<std::string, PartDef>& registry);
bool apeLook(const std::string& species, Look& look);
float knuckleCadence(const Build& b);
float knuckleSpeed(const Build& b, float cadence);
// Birds and dinosaurs (rig_bird.cpp).
void birdParts(std::map<std::string, PartDef>& registry);
bool birdLook(const std::string& species, Look& look);
bool isBiped(const Build& b);
float bipedCadence(const Build& b, Gait g);
float bipedSpeed(const Build& b, Gait g, float cadence);
Clip bipedStand(const Build& b);
Clip bipedGait(const Build& b, Gait g, float cadence);
// Snakes and fish (rig_chain.cpp).
void chainParts(std::map<std::string, PartDef>& registry);
bool chainLook(const std::string& species, Look& look);
bool isChain(const Build& b);
float chainCadence(const Build& b, Gait g);
float chainSpeed(const Build& b, Gait g, float cadence);
Clip chainStand(const Build& b);
Clip chainGaitClip(const Build& b, Gait g, float cadence);

inline const std::vector<std::string>& slotKeys() {
    static const std::vector<std::string> keys = { "head", "hair", "hat", "torso", "hips", "armL", "armR", "handL", "handR",
                                                   "legL", "legR", "footL", "footR", "extra", "holdL", "holdR",
                                                   "ride" };
    return keys;
}

} // namespace detail
} // namespace rig
} // namespace rmtn
