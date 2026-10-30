// rmtn/rig.hpp -- configurable characters (rig::): one humanoid skeleton that wears
// swappable parts (heads, hair, hats, torsos, arms, hands, legs, feet), rigid like a toy
// brick figure or soft like a rubber-hose cartoon, with an outfit texture and an animated
// face drawn from vectors (procedural or an SVG of named shapes). The same character
// renders as 3D meshes in a g3d scene or as a flat 2D cut-out puppet.
//
//     rig::Look look = rig::Look::toon();
//     look.hair = "hair.bun";
//     look.arms("arm.soft").legs("leg.rigid");
//     look.outfit = rig::Outfit::basic({ 230, 90, 70, 255 }, { 40, 60, 110, 255 });
//     auto hero = std::make_shared<rig::Character>(look);
//     hero->motion.play(rig::clips::idle(), 0).play(rig::clips::wave(rig::Side::Right), 2);
//     hero->face.say(2.2, 3.4, "hello there").expression(3.6, "happy");
//     rig::add(*scene, hero);                                   // 3D (animates from the scene's t)
//     rig::View2D view;                                         // or 2D
//     view.yaw = 20; view.scale = 260; view.origin = { 960, 900 };
//     node->fn = [hero, view](const Ctx& c, int t) { rig::draw(c, *hero, t / 30.0, view); };
//
// Character space: origin between the feet on the ground, +y up, +z the character's
// front, so its left hand is towards +x. Units are metres. Everything is a pure function
// of the time in seconds: poses, face, swaps.
#pragma once
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "paths.hpp"
#include "scene3d.hpp"
#include "timeline.hpp"

namespace rmtn {
namespace rig {

// ------------------------------------------------------------------ skeleton
enum class Joint : int {
    Root, Pelvis, Chest, Neck, Head,
    ShoulderL, ElbowL, WristL, ShoulderR, ElbowR, WristR,
    HipL, KneeL, AnkleL, HipR, KneeR, AnkleR,
    Count
};
constexpr int kJoints = (int)Joint::Count;
enum class Side { Left, Right };
const char* name(Joint j);
Joint parent(Joint j);                       // Root's parent is Root
bool isRight(Joint j);

// Proportions. Joint positions follow from these (arms hang at the sides at rest).
struct Build {
    float head = 0.30f;                      // head height
    float neck = 0.05f;
    Vector3 torso{ 0.36f, 0.42f, 0.20f };    // chest block, waist to shoulders: w, h, d
    float hips = 0.14f;                      // pelvis block height (centred on the hip joints)
    float hipWidth = 0.18f;                  // between the hip joints
    float upperArm = 0.27f, forearm = 0.25f, armRadius = 0.055f, hand = 0.09f;
    float thigh = 0.36f, shin = 0.34f, legRadius = 0.07f, foot = 0.20f, ankle = 0.07f;

    static Build adult();                    // ~1.75 m, natural proportions
    static Build toon();                     // big head, short body (the default Look)
    static Build block();                    // brick-figure proportions: big cylinder head, stiff limbs
    static Build chibi();                    // huge head, tiny body
    float height() const;                    // ground to the top of the head
    Build scaled(float k) const;
    static Build mix(const Build& a, const Build& b, float u);
};

// The joint's position in its parent's frame at rest.
Vector3 restOffset(const Build& b, Joint j);

// Joint rotations, Euler XYZ degrees in the parent frame. Right-side joints are mirrored
// (their y and z negate), so one set of numbers poses either side the same way:
//   shoulders/hips: x < 0 swings the limb forward, z > 0 raises it out to the side;
//   elbows: x < 0 bends the forearm forward; knees: x > 0 bends the shin back;
//   chest/neck/head: x > 0 leans forward, y turns towards the character's left, z tilts.
struct Pose {
    std::array<Vector3, kJoints> rot{};
    Vector3 offset{ 0, 0, 0 };               // root translation (bob, jump, sway)
    Vector3& operator[](Joint j) { return rot[(size_t)j]; }
    const Vector3& operator[](Joint j) const { return rot[(size_t)j]; }
    static Pose mix(const Pose& a, const Pose& b, float u);
    Pose operator+(const Pose& o) const;     // layers: rotations and offsets add
    Pose operator*(float k) const;
    Pose mirrored() const;                   // left and right swapped
};

// The rotation actually applied at joint j (the right-side mirror resolved).
Vector3 jointRotation(const Pose& p, Joint j);
// Forward kinematics: every joint's frame in character space (raymath row-vector order,
// as g3d composes objects: local = rotate then translate, world = local * parent).
std::array<Matrix, kJoints> solve(const Build& b, const Pose& p);
Vector3 position(const std::array<Matrix, kJoints>& frames, Joint j);

// ------------------------------------------------------------------ motion
using Clip = std::function<Pose(double seconds)>;   // seconds from the clip's start

namespace clips {
Clip rest();                                 // straight, arms down
Clip hold(Pose p);
Clip idle(float energy = 1);                 // breathing, weight shift, a little arm sway
Clip walk(float cadence = 1);                // in place; `cadence` strides per second per foot
Clip run(float cadence = 1.4f);
Clip wave(Side side = Side::Right);
Clip point(Side side = Side::Right);         // arm out ahead, held
Clip cheer();                                // both arms up, bouncing
Clip jump(float period = 1.2f);              // crouch, leap, land, repeating
Clip sit(float drop = 0.3f);                 // `drop`: how far the hips sink (about the thigh length)
Clip shrug();
// Keyframed poses at times (seconds), eased between (smoothstep); held after the last,
// or looping over the last key's time when `loop`.
Clip keys(std::vector<std::pair<double, Pose>> keys, bool loop = false);
Clip named(const std::string& name);         // "idle", "walk", "run", "wave", "wave-left", ...; throws if unknown
}
// Ground covered per second by clips::walk/run at `cadence` for this build (for moving
// the character so its feet do not slide).
float walkSpeed(const Build& b, float cadence = 1);
float runSpeed(const Build& b, float cadence = 1.4f);

// Which joints a layered clip drives.
enum class Mask { All, Upper, Lower, ArmL, ArmR, Arms, Head };
bool masks(Mask m, Joint j);

// Clips on a timeline: each base clip plays from its start until the next takes over,
// crossfading for `fade` seconds; layers override some joints between `from` and `to`.
class Motion {
public:
    Motion& play(Clip clip, double from, double fade = 0.3);
    Motion& layer(Clip clip, double from, double to, Mask mask, double fade = 0.25);
    Pose at(double seconds) const;
    bool empty() const { return base_.empty(); }
private:
    struct Entry { Clip clip; double from, to, fade; Mask mask; };
    std::vector<Entry> base_, layers_;
};

// ------------------------------------------------------------------ faces
// Expression names: neutral, happy, sad, angry, surprised, wink, sleepy, smug (an SVG
// face may add its own). Mouth shapes are the Preston Blair / Rhubarb Lip Sync set:
//   A closed (M B P)   B slightly open (most consonants, EE)   C open (EH, AE)
//   D wide open (AA)   E rounded (AO, ER)   F puckered (UW, OW, W)   G F/V   H L   X rest
struct FaceState {
    std::string expression = "neutral", previous = "neutral";
    float mix = 1;                           // 0 = previous, 1 = expression
    char mouth = 'X', previousMouth = 'X';
    float mouthMix = 1;
    float blink = 0;                         // 0 open .. 1 shut
    Vector2 look{ 0, 0 };                    // gaze, -1..1 (x to the viewer's right, y down)
    Color skin{ 222, 170, 130, 255 };        // set by the renderers: eyelids are skin
    // How much of `name` shows (its share of the expression crossfade).
    float weight(const std::string& name) const {
        return (expression == name ? mix : 0.f) + (previous == name ? 1 - mix : 0.f);
    }
};

// Draws a face's features (no skin) inside `box`, in the Ctx's units.
class Face {
public:
    virtual ~Face() = default;
    virtual void draw(const Ctx& c, Rectangle box, const FaceState& s) const = 0;
};
using FacePtr = std::shared_ptr<const Face>;

struct FaceStyle {
    enum class Eyes { Dots, Cartoon };
    Eyes eyes = Eyes::Cartoon;
    Color ink{ 30, 26, 34, 255 };            // pupils, brows, mouth line
    Color white{ 250, 248, 244, 255 };
    Color iris{ 70, 110, 160, 255 };         // cartoon eyes
    Color mouth{ 120, 40, 50, 255 };         // inside an open mouth
    Color blush{ 0, 0, 0, 0 };               // cheeks (transparent: none)
    float eyeSize = 1, eyeSpacing = 1, browWeight = 1, lineWeight = 1;
    bool brows = true;
};
// A procedural face: eyes, brows and a mouth whose shapes follow the expression, the
// mouth shape, blinks and gaze.
FacePtr simpleFace(FaceStyle style = {});
// A face from an SVG whose shapes are named by id (one path per id; compound paths ok):
//   eye-L, eye-R              open eyes; eye-L-closed / eye-R-closed are morphed to on a
//                             blink (without them the eye squashes shut)
//   pupil-L, pupil-R          moved with the gaze
//   brow-L, brow-R, mouth     with variants brow-L-<expression>, mouth-<expression>,
//                             mouth-<A..H,X> (speech wins over the expression's mouth)
//   <anything>-<expression>   shown only with that expression (blush-happy, tear-sad)
//   anything else             always drawn
// Changes morph between the shapes (vg::Morph). Throws if the file cannot be read.
FacePtr svgFace(const std::string& file);
FacePtr svgFaceFromText(const std::string& svgText);

// What the face does over time. Blinks come by themselves (seeded, every 2-5 s).
class FaceTrack {
public:
    unsigned seed = 7;
    bool autoBlink = true;
    FaceTrack& expression(double at, const std::string& name, double fade = 0.2);
    FaceTrack& look(double at, Vector2 gaze, double fade = 0.25);
    FaceTrack& blink(double at);
    // Mouth shapes for `text` spread over [from, to] (letters to shapes, a rough guess).
    FaceTrack& say(double from, double to, const std::string& text);
    // Exact mouth cues: (seconds, shape letter), e.g. from Rhubarb Lip Sync.
    FaceTrack& mouths(std::vector<std::pair<double, char>> cues);
    // Rhubarb Lip Sync's TSV output ("0.00\tX" per line), shifted by `offset` seconds.
    FaceTrack& rhubarb(const std::string& file, double offset = 0);
    FaceState at(double seconds) const;
private:
    struct Key { double at; std::string name; double fade; };
    struct LookKey { double at; Vector2 gaze; double fade; };
    std::vector<Key> expressions_;
    std::vector<LookKey> looks_;
    std::vector<double> blinks_;
    std::vector<std::pair<double, char>> mouths_;
};
// Letters of `text` as mouth cues over [from, to].
std::vector<std::pair<double, char>> visemes(const std::string& text, double from, double to);

// ------------------------------------------------------------------ outfits
// One texture wraps the torso, hips, arms and legs. Each region is a strip: across is
// round the body (the front in the middle quarter, the back split at the edges), down is
// from the top of the part to its bottom (an arm from shoulder to wrist).
enum class Region { Torso, Hips, ArmL, ArmR, LegL, LegR, Count };
Rectangle region(Region r);                  // in 0..1 texture units
Rectangle front(Region r);                   // its front quarter (chest prints, buttons)

struct Outfit {
    int size = 1024;                         // texture px (square)
    // Paints the whole texture in px (0..size). Animate with t (frames).
    std::function<void(const Ctx& c, int t, float size)> paint;
    // The colour at (u, v) of a region, for the 2D puppet, which has no textures. Default:
    // `fallback`.
    std::function<Color(Region r, float u, float v)> sample;
    Color fallback{ 200, 200, 200, 255 };
    unsigned id = 0;                         // identity: set by the builders, renewed by over()

    // A top and trousers: `sleeve` is how much of each arm the top covers (0 = none,
    // 1 = to the wrist), `trouser` the same for the legs; the rest shows `skin`.
    static Outfit basic(Color top, Color bottom, Color skin = { 222, 170, 130, 255 },
                        float sleeve = 0.45f, float trouser = 1.0f);
    // An SVG drawn over the whole texture (its document square; see assets/characters/
    // outfit-template.svg). `under` paints first (e.g. basic colours below a print).
    static Outfit svg(const std::string& file, const Outfit& under = {});
    // Adds a layer on top (prints, numbers, animated logos).
    Outfit& over(std::function<void(const Ctx& c, int t, float size)> layer);
    bool painted() const { return (bool)paint; }
};

// ------------------------------------------------------------------ parts
enum class Slot { Head, Hair, Hat, Torso, Hips, Arm, Hand, Leg, Foot, Extra };
const char* name(Slot s);

// Where a primitive takes its colour from.
enum class Paint { Skin, Hair, Shoes, Accent, Metal, Dark, Outfit, Face, Fixed };

// A shape attached to a joint. Super: a superellipsoid (squareV/squareH 1 = round, small
// = boxy; cylinder = squareV small, squareH 1) tapered to `taper` times its width at the
// top, optionally cut to latitudes from..to (-1 bottom, 1 top) for domes and caps. Cone:
// apex up. Tube: a limb from joint a through b to c, bent smoothly by `softness`
// (0 = a sharp elbow, 1 = rubber hose). Model: a mesh file (3D only; `svg` draws in 2D).
struct Prim {
    enum class Kind { Super, Cone, Tube, Model };
    Kind kind = Kind::Super;
    std::string name;
    Joint joint = Joint::Pelvis;
    Vector3 at{ 0, 0, 0 }, rot{ 0, 0, 0 }, size{ 0.1f, 0.1f, 0.1f };
    float squareV = 1, squareH = 1, taper = 1, from = -1, to = 1;
    Paint paint = Paint::Skin;
    Color color{ 255, 255, 255, 255 };       // Paint::Fixed
    Region region = Region::Torso;           // Paint::Outfit: the strip, and the part of it
    float v0 = 0, v1 = 1;                    //   from top (v0) to bottom (v1) this shape covers
    Joint a = Joint::ShoulderL, b = Joint::ElbowL, c = Joint::WristL;   // Tube
    float r0 = 0.05f, r1 = 0.04f, softness = 1;
    std::string model, svg;                  // Model (size = its scale on each axis)
    Vector2 flat{ 0.3f, 0.3f };              // Model: the svg's width and height in 2D
    float roughness = -1;                    // -1: from the Look's finish
    float depth2d = 0;                       // 2D: added to the depth it is sorted by (metres)
};

// A swappable part: the primitives it adds for a build and side.
// `locks` hold joints still while the part is worn (given for the left side; the right
// mirrors them): a brick figure's arm keeps its elbow bent and its leg its knee straight,
// so hands and feet still land where the part's shape ends.
struct PartDef {
    std::string name;
    Slot slot = Slot::Extra;
    std::function<std::vector<Prim>(const Build& b, Side side, float softness)> make;
    std::vector<std::pair<Joint, Vector3>> locks;
};
namespace parts {
void add(PartDef def);                       // register (replaces a part of the same name)
const PartDef& get(const std::string& name); // throws std::runtime_error for unknown names
bool has(const std::string& name);
std::vector<std::string> names(Slot slot);   // sorted
// A model file worn at a joint (hats, helmets, props, prosthetics), scaled and placed in
// the joint's frame; `svg` (optional) stands in for it in 2D, `size` wide and tall.
PartDef model(const std::string& name, Slot slot, const std::string& file, Joint joint,
              Vector3 at, Vector3 rot = { 0, 0, 0 }, float scale = 1,
              const std::string& svg = "", Vector2 size = { 0.3f, 0.3f });
}

enum class Finish { Plastic, Matte, Toon };

// What a character is made of and wears. Part names come from parts::names(); "" = none.
struct Look {
    Build build = Build::toon();
    std::string head = "head.round", hair = "hair.short", hat, torso = "torso.round",
                hips = "hips.round", extra;
    std::string armL = "arm.soft", armR = "arm.soft", handL = "hand.mitten", handR = "hand.mitten",
                legL = "leg.soft", legR = "leg.soft", footL = "foot.shoe", footR = "foot.shoe";
    float softness = 0.8f;                   // soft limbs: 0 angular .. 1 rubber hose
    Color skin{ 222, 170, 130, 255 }, hairColor{ 60, 42, 34, 255 }, shoes{ 50, 50, 58, 255 },
          accent{ 240, 190, 60, 255 }, metal{ 170, 178, 188, 255 }, dark{ 44, 44, 52, 255 };
    Outfit outfit = Outfit::basic({ 70, 130, 200, 255 }, { 50, 60, 90, 255 });
    FacePtr face = simpleFace();
    Rectangle faceBox{ 0.16f, 0.26f, 0.68f, 0.56f };   // features' box on the head's front (0..1)
    Finish finish = Finish::Matte;

    Look& arms(const std::string& part) { armL = armR = part; return *this; }
    Look& hands(const std::string& part) { handL = handR = part; return *this; }
    Look& legs(const std::string& part) { legL = legR = part; return *this; }
    Look& feet(const std::string& part) { footL = footR = part; return *this; }

    static Look toon();                      // soft limbs, round head, cartoon face
    static Look block();                     // brick figure: cylinder head, rigid limbs, plastic
    static Look avatar();                    // blocky game avatar: box head, rigid limbs
    static Look robot();
};

// Every part a look wears, as (slot key, part name), e.g. ("armL", "arm.soft").
std::vector<std::pair<std::string, std::string>> slots(const Look& look);

// ------------------------------------------------------------------ characters
class Character {
public:
    explicit Character(Look look = Look::toon());
    Motion motion;
    FaceTrack face;
    // Optional: how soft the soft limbs are at each moment (0..1), overriding the looks'
    // `softness` (a dial from angular to rubber hose).
    std::function<float(double seconds)> softness;
    // From `at` seconds the character wears `look`; parts that change pop in over `pop`
    // seconds (0 = cut). Its parts must be registered by then (unknown names throw here).
    Character& wear(double at, Look look, double pop = 0.35);

    const Look& lookAt(double seconds) const;
    // The look whose `slot` (a key from slots()) is showing, and how far its pop has got
    // (0..1, overshooting slightly on the way).
    int lookIndex(const std::string& slot, double seconds) const;
    float popAt(const std::string& slot, double seconds) const;
    const std::vector<Look>& looks() const { return looks_; }
    double changeTime(int lookIndex) const { return times_[(size_t)lookIndex]; }
    // The motion's pose with the worn parts' joint locks applied (rest when motion is empty).
    Pose poseAt(double seconds) const;
private:
    std::vector<Look> looks_;
    std::vector<double> times_, pops_;
    std::vector<std::map<std::string, std::string>> sigs_;   // per look: slot -> what it shows
};
using CharacterPtr = std::shared_ptr<Character>;

// The primitives a look puts on its skeleton (every slot, resolved to its part).
std::vector<Prim> prims(const Look& look);
std::vector<Prim> prims(const Look& look, const std::string& slot);
// `p` with the joint locks of the parts `look` wears.
Pose locked(const Look& look, Pose p);

// ---- 3D: a subtree under `parent`, named "character", animated by its own `up` from the
// scene's frame t (seconds = t / fps). Move/turn the returned root to place it.
g3d::Object3D* add(g3d::Object3D& parent, CharacterPtr ch, int fps = 30);
g3d::Object3D* add(g3d::Scene& scene, CharacterPtr ch, int fps = 30);

// ---- 2D: a flat cut-out puppet, the 3D shapes projected orthographically and drawn
// back to front with an ink outline.
struct View2D {
    float yaw = 0;                           // degrees; 0 faces the viewer, 90 shows its left side
    float pitch = 0;                         // degrees; > 0 looks down on it
    float scale = 300;                       // px per metre
    Vector2 origin{ 0, 0 };                  // where the feet stand, in node units
    Color ink{ 28, 24, 34, 255 };
    float outline = 3;                       // px (0 = none)
    bool shade = true;                       // far-side limbs a little darker
    Color shadow{ 0, 0, 0, 50 };             // ground shadow under the feet (transparent: none)
};
void draw(const Ctx& c, const Character& ch, double seconds, const View2D& view = {});
// Where a character-space point lands in 2D (for labels, props and speech bubbles).
Vector2 project(const View2D& view, Vector3 p);

// Prefab "character": props mode ("2d" | "3d"), preset ("toon" | "block" | "avatar" |
// "robot"), any part key (head, hair, hat, torso, hips, arms, hands, legs, feet, armL...),
// colours (skin, hair, shoes, accent, top, bottom), softness, clip, expression, say,
// yaw, scale, x, y.
void registerPrefabs();

} // namespace rig
} // namespace rmtn
