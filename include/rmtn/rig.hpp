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
    Floor,                                   // the ground under the feet: a child of Root that
                                             //   ignores the pose's offset (boards, scooters)
    Count
};
constexpr int kJoints = (int)Joint::Count;  // the humanoid's joints
constexpr int kMaxJoints = 48;               // room in a Pose for any body plan
enum class Side { Left, Right };
const char* name(Joint j);
Joint parent(Joint j);                       // Root's parent is Root
bool isRight(Joint j);

// A body plan other than the humanoid (quadrupeds, birds, fish...): its joints by name,
// each hanging from an earlier one. Joint 0 is the root, 1 the hips and 2 the middle of the
// body (2D sorts by it). Its joints are addressed as Joint values by index: plan["tail-2"].
struct JointDef {
    std::string name;
    int parent = 0;
    Vector3 rest{ 0, 0, 0 };                 // offset in the parent's frame at rest
    bool right = false;                      // a right-side joint: its pose's y and z negate
    bool grounded = false;                   // hangs from the origin and ignores the pose's offset
};
struct Skeleton {
    std::string kind;                        // "quadruped", ...
    std::vector<JointDef> joints;            // parents before children
    float height = 1;                        // ground to the top at rest
    std::string variant;                     // e.g. the species: picks parts that differ
    std::map<std::string, float> dims;       // measurements the parts and clips read (metres;
                                             //   Build::scaled scales them)
    std::map<std::string, float> traits;     // angles, ratios and flags (never scaled)
    int find(const std::string& name) const; // throws std::runtime_error for unknown names
    Joint operator[](const std::string& name) const { return (Joint)find(name); }
    float dim(const std::string& key, float fallback = 0) const;
    float trait(const std::string& key, float fallback = 0) const;
};
using SkeletonPtr = std::shared_ptr<const Skeleton>;

// Proportions. Joint positions follow from these (arms hang at the sides at rest).
struct Build {
    float head = 0.30f;                      // head height
    float neck = 0.05f;
    Vector3 torso{ 0.36f, 0.42f, 0.20f };    // chest block, waist to shoulders: w, h, d
    float hips = 0.14f;                      // pelvis block height (centred on the hip joints)
    float hipWidth = 0.18f;                  // between the hip joints
    float upperArm = 0.27f, forearm = 0.25f, armRadius = 0.055f, hand = 0.09f;
    float thigh = 0.36f, shin = 0.34f, legRadius = 0.07f, foot = 0.20f, ankle = 0.07f;
    SkeletonPtr plan;                        // another body plan (null: the humanoid above)

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
// The skeleton a build uses: its joint count, names, parents and sides.
int jointCount(const Build& b);
const char* name(const Build& b, Joint j);
Joint parent(const Build& b, Joint j);
bool isRight(const Build& b, Joint j);
bool isGrounded(const Build& b, Joint j);    // Root and Floor, or a plan's grounded joints
Joint joint(const Build& b, const std::string& name);   // throws for unknown names

// Joint rotations, Euler XYZ degrees in the parent frame. Right-side joints are mirrored
// (their y and z negate), so one set of numbers poses either side the same way:
//   shoulders/hips: x < 0 swings the limb forward, z > 0 raises it out to the side;
//   elbows: x < 0 bends the forearm forward; knees: x > 0 bends the shin back;
//   chest/neck/head: x > 0 leans forward, y turns towards the character's left, z tilts.
struct Pose {
    std::array<Vector3, kMaxJoints> rot{};
    Vector3 offset{ 0, 0, 0 };               // root translation (bob, jump, sway)
    Vector3& operator[](Joint j) { return rot[(size_t)j]; }
    const Vector3& operator[](Joint j) const { return rot[(size_t)j]; }
    static Pose mix(const Pose& a, const Pose& b, float u);
    Pose operator+(const Pose& o) const;     // layers: rotations and offsets add
    Pose operator*(float k) const;
    Pose mirrored() const;                   // left and right swapped (humanoid joints)
};

// The rotation actually applied at joint j (the right-side mirror resolved).
Vector3 jointRotation(const Pose& p, Joint j);
Vector3 jointRotation(const Build& b, const Pose& p, Joint j);
// Forward kinematics: every joint's frame in character space (raymath row-vector order,
// as g3d composes objects: local = rotate then translate, world = local * parent).
// Entries past jointCount(b) are unused.
using Frames = std::array<Matrix, kMaxJoints>;
Frames solve(const Build& b, const Pose& p);
Vector3 position(const Frames& frames, Joint j);

// ------------------------------------------------------------------ reaching (IK)
// Two-bone IK on `side`'s arm: shoulder and elbow turned so the wrist lands on `target`
// (character space; out of reach, the arm points at it, straight). The elbow bends
// towards `pole`, a direction given for the left arm (the right arm mirrors it).
Pose reach(const Build& b, Pose p, Side side, Vector3 target, Vector3 pole = { 0.5f, -0.4f, -1 });
// The same for a leg: hip and knee turned so the ankle lands on `target`.
Pose plant(const Build& b, Pose p, Side side, Vector3 target, Vector3 pole = { 0, 0, 1 });
// Joint j's rotation chosen so its frame's +y points along `up` and its +z towards
// `forward` (character space).
Pose orient(const Build& b, Pose p, Joint j, Vector3 up, Vector3 forward);
// A hand placed: the palm's centre on `at`, the fingers pointing along `fingers`, the palm
// facing `palm` (character space). Held props sit in the palm, so this is how a pose
// holds a phone up (palm behind it) or closes a fist round a pole (fingers forward, palm
// towards the pole, thumb along it).
Pose hand(const Build& b, Pose p, Side side, Vector3 at, Vector3 fingers, Vector3 palm,
          Vector3 pole = { 0.5f, -0.4f, -1 });
// Where `side`'s palm centre is in a pose (character space).
Vector3 palmAt(const Build& b, const Pose& p, Side side);
// Two-bone IK on any limb that hangs along its joints' -y at rest (every plan's legs do):
// `upper` and `mid` turned so `end` lands on `target`, `mid` jutting towards `pole`
// (character space, not mirrored). `flex` is the sign of mid's x bend: +1 a knee (the
// lower bone swings back), -1 an elbow (it swings forward).
Pose limb(const Build& b, Pose p, Joint upper, Joint mid, Joint end, Vector3 target, Vector3 pole, float flex);

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

// ---- with furniture and props. Targets are in character space: place the character's
// root where the action happens (for a chair, the floor under the seat's centre) and give
// the rest relative to it. Furniture is real size, so a short build perches on a tall seat.
// Seated on a seat whose top is `seat` above the floor: thighs along it, feet planted when
// the shins reach the floor, hanging (and swinging a little) when they don't.
Clip seated(const Build& b, float seat);
// Seated, typing on a keyboard whose centre is at `keys`, head bowed to a screen behind it.
Clip typing(const Build& b, float seat, Vector3 keys, float rate = 1);
// A phone (prop.phone) in the `side` hand: thumbing it at chest height, or held to the ear.
Clip texting(const Build& b, Side side = Side::Right);
Clip calling(const Build& b, Side side = Side::Right);
// A tablet (prop.tablet) resting in the left hand, the right tapping it.
Clip tablet(const Build& b);
// A placard (prop.sign) held up on its pole in both hands, pumped `beat` times a second.
Clip protest(const Build& b, float beat = 1);
// A riot line: a shield (prop.shield) braced in the left hand, a baton (prop.baton) in the
// right, knees bent; `advance`: stepping in place behind the shield.
Clip shieldLine(const Build& b, bool advance = false);
// On a skateboard (ride.skateboard, lying along the character's x): rolling towards +x,
// the right foot pushing off every `period` seconds (0 = cruising).
Clip skate(const Build& b, float period = 2.4f);
// On a kick scooter (ride.scooter, along +z): the left foot on the deck, the right kicking
// every `period` seconds (0 = both feet on the deck).
Clip scooter(const Build& b, float period = 1.6f);
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
enum class Slot { Head, Hair, Hat, Torso, Hips, Arm, Hand, Leg, Foot, Extra, Hold, Ride };
const char* name(Slot s);

// Where a primitive takes its colour from. Label: `color`, with the look's `sign` text
// (or the prim's own `label`) across its front, readable from the back too.
enum class Paint { Skin, Hair, Shoes, Accent, Metal, Dark, Outfit, Face, Fixed, Label };

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
    std::string label;                       // Paint::Label text ("" = the look's `sign`)
    float from2d = -2;                       // 2D: where `from` rises to as the part's +z faces
                                             //   the viewer (a cap of hair's hairline; < -1: off)
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
    // Held in each hand (prop.phone, prop.tablet, prop.sign, prop.shield, ...) and ridden
    // (ride.skateboard, ride.scooter). Pose the hands with rig::hand() or the clips below.
    std::string holdL, holdR, ride;
    std::string sign;                        // text on Paint::Label parts (placards, shields)
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

// ------------------------------------------------------------------ animals
// Four-legged animals: a body from the hips ("pelvis") through "spine" to the withers
// ("chest"); "neck-1", "neck-2", "head"; "tail-1".."tail-3"; and four legs that hang along
// -y: front "shoulder-L", "elbow-L", "wrist-L" (a horse's knee), "paw-front-L"; hind
// "hip-L", "knee-L" (the stifle), "hock-L", "paw-hind-L"; the same with -R; and "floor".
// +z is the animal's front, so its left legs are towards +x. Lengths are metres.
struct QuadSpec {
    std::string species = "dog";             // picks the head, tail and coat parts
    float bodyLength = 0.5f;                 // hips to withers
    float bodyRadius = 0.14f, rump = 1, chest = 1;   // the barrel; its ends, times bodyRadius
    float frontUpper = 0.19f, frontLower = 0.2f, frontCannon = 0.11f;   // shoulder to paw
    float hindUpper = 0.2f, hindLower = 0.2f, hindCannon = 0.1f;        // hip to paw
    float paw = 0.04f;                       // ground to the paw joint
    float legRadius = 0.045f;
    float stance = 0.09f;                    // half the distance between the left and right legs
    float neckLength = 0.2f, neckRadius = 0.07f, neckPitch = 45;        // degrees above level
    float headLength = 0.22f, headRadius = 0.085f, headPitch = 10;      // degrees nose-down
    float tailLength = 0.35f, tailRadius = 0.025f, tailPitch = 20;      // degrees above level
    bool hooves = false;                     // hooves (Look::shoes) or paws (the coat)

    static QuadSpec dog();
    static QuadSpec cat();
    static QuadSpec horse();
    static QuadSpec cow();
    static QuadSpec giraffe();
    static QuadSpec turtle();
};
Build quadruped(const QuadSpec& spec);

// Two-legged animals, birds and dinosaurs: the hips ("pelvis") at the middle of the body,
// "spine" and "chest" ahead of them; "neck-1", "neck-2", "head" and "jaw"; "tail-1".."tail-3";
// legs "hip-L", "knee-L", "ankle-L" (the backward-bending joint), "foot-L" (where the toes
// meet, on the ground); "shoulder-L", "elbow-L", "wrist-L", "tip-L" -- wings, which run out
// sideways at rest (pose them with rig::wings), or arms, which hang; the same with -R; and
// "floor". +z is the front. Lengths are metres.
struct BipedSpec {
    std::string species = "chicken";         // picks the head, tail and leg details
    float bodyLength = 0.26f, bodyRadius = 0.1f, bodyPitch = 20;          // degrees nose-up
    float thigh = 0.07f, shank = 0.085f, tarsus = 0.075f;                  // hip to foot
    float foot = 0.014f, toe = 0.06f;        // ground to the foot joint; the middle toe
    float legRadius = 0.016f, stance = 0.04f, rake = 15;                  // tarsus degrees off upright
    float neckLength = 0.1f, neckRadius = 0.032f, neckPitch = 72;
    float headLength = 0.05f, headRadius = 0.04f, headPitch = 0;          // beak or snout length
    float tailLength = 0.12f, tailRadius = 0.03f, tailPitch = 50;
    float upperArm = 0.07f, forearm = 0.08f, hand = 0.1f, armRadius = 0.016f;   // wing or arm bones
    float chord = 0.13f;                     // wing feathers' depth
    bool wings = true;

    static BipedSpec chicken();
    static BipedSpec duck();
    static BipedSpec trex();
};
Build biped(const BipedSpec& spec);
// Legless animals, snakes and fish: "body-0" (behind the head) through "body-<n-1>" to
// "tip", each joint one segment behind the last; "head", "jaw" and "tongue" (snakes; turned
// 180 degrees in y it is stowed); "fin-L", "fin-R" (a fish's pectorals); "floor". Lying or
// resting on the ground at rest; +z is the front. Lengths are metres.
struct ChainSpec {
    std::string species = "snake";           // picks the head and the fins
    float length = 1.6f;                     // nose to tail tip
    float radius = 0.035f;                   // the body's widest half-width
    float depth = 1;                         // its height, times its width (a fish: > 1)
    int segments = 14;                       // body joints (3..38)
    float headLength = 0.06f, headRadius = 0.03f;
    float fin = 0;                           // a fish's tail-fin length

    static ChainSpec snake();
    static ChainSpec fish();
};
Build chain(const ChainSpec& spec);

// A bird's wings: `spread` 0 folded along the body .. 1 open, raised `flap` degrees at the
// shoulder (negative: the downstroke; the tips lag), swept back by `sweep` (0..1). A biped
// with arms is returned unchanged.
Pose wings(const Build& b, Pose p, float spread, float flap = 0, float sweep = 0);
// A finished animal (build, parts, colours): "dog", "cat", "horse", "cow", "giraffe" or
// "turtle";
// "chicken", "duck" or "trex" (two legs, rig::biped); "snake" or "fish" (rig::chain); or an ape -- "chimp", "gorilla", "orangutan" -- which is a humanoid build (long arms, short
// legs) and plays any humanoid clip as well as those below. Throws for unknown names.
Look animal(const std::string& species);
std::vector<std::string> animals();

// Footfall patterns: walk (one foot at a time, three down), trot (diagonal pairs), canter
// (three beats) and gallop (four beats with a moment in the air).
enum class Gait { Walk, Trot, Canter, Gallop };
namespace clips {
// An animal standing: breathing, the tail swinging, now and then a look round. (A humanoid
// build -- an ape -- plays idle().)
Clip stand(const Build& b);
// A gait in place, paws planted while they are down. `cadence`: strides per second
// (0 = what an animal of this size naturally picks). Two-legged animals walk (Gait::Walk)
// or run (any other gait); snakes slither and fish swim, slower (Walk) or faster. A
// humanoid build knuckle-walks (Gait::Walk only).
Clip gait(const Build& b, Gait g, float cadence = 0);
// Humanoid clips for apes (any humanoid build can play them):
// on all fours, leaning on the knuckles, feet and knuckles planted while down; move the
// character by gaitSpeed(b, Gait::Walk, cadence).
Clip knuckleWalk(const Build& b, float cadence = 0);
// Hanging from a bar `bar` metres up (along x, through z = 0) by both hands, swaying
// `swing` degrees.
Clip hang(const Build& b, float bar, float swing = 6);
// Swinging hand over hand along bars `spacing` apart at height `bar`, in place: move the
// character forward by 2 * spacing * rate per second (`rate` swings of each arm per
// second; 0 = a natural pendulum rate, see brachiateRate).
Clip brachiate(const Build& b, float bar, float spacing, float rate = 0);
// Sitting astride (a horse, a bike, a branch), the hips at the root's origin so a Mount puts
// them on the seat; a little sway with the ride. Grips place the hands and feet.
Clip ride(const Build& b);
}
float brachiateRate(const Build& b);
namespace clips {
// A bird in the air, in place (move its node along its +z): wingbeats `rate` per second
// (0 = flapRate), the body level, the legs tucked. glide: wings held out, banking gently.
Clip fly(const Build& b, float rate = 0);
Clip glide(const Build& b);
}
float flapRate(const Build& b);
// Ground covered per second by clips::gait (move the animal by this so its paws hold still).
float gaitSpeed(const Build& b, Gait g, float cadence = 0);
float gaitCadence(const Build& b, Gait g);   // the natural cadence
namespace clips {
// A four-legged animal swimming in place (move its node along +z), legs stroking in
// diagonal pairs: a turtle's front flippers sweep wide, a dog paddles.
Clip paddle(const Build& b, float rate = 0);
}

// ------------------------------------------------------------------ characters
class Character;
// One character carried by another -- riding, perched, piggyback, in a hand, in a cockpit.
// From `from` to `until` seconds its root follows the carrier's `joint`, offset by `at` and
// turned by `rot` (degrees) in that joint's frame, easing in and out over `blend` seconds;
// its own clip's root motion still applies on top (in the joint's frame). Then `grips` reach
// its hands and plant its feet on points of the carrier (`at` in the carrier joint's frame;
// humanoid riders). Both characters must stand on the same node (3D) or share a View2D
// (2D): the rider is moved within that space. Overlapping mounts blend in the order added
// (a hand-off: the next one's `from` before the last one's `until`). No cycles.
enum class Limb { HandL, HandR, FootL, FootR };
struct Grip {
    Limb limb = Limb::HandL;
    Joint joint = Joint::Root;
    Vector3 at{ 0, 0, 0 };
};
struct Mount {
    std::weak_ptr<const Character> carrier;
    Joint joint = Joint::Root;
    Vector3 at{ 0, 0, 0 }, rot{ 0, 0, 0 };
    double from = 0, until = 1e30, blend = 0.3;
    std::vector<Grip> grips;
};

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
    // The motion's pose with the worn parts' joint locks applied (rest when motion is empty),
    // then carried by its mounts.
    Pose poseAt(double seconds) const;
    Character& mount(Mount m) { mounts_.push_back(std::move(m)); return *this; }
    const std::vector<Mount>& mounts() const { return mounts_; }
private:
    std::vector<Mount> mounts_;
    std::vector<Look> looks_;
    std::vector<double> times_, pops_;
    std::vector<std::map<std::string, std::string>> sigs_;   // per look: slot -> what it shows
};
using CharacterPtr = std::shared_ptr<Character>;

// A saddle on a four-legged animal: the rider's hips on its back, feet in stirrups, hands
// on the reins. Play clips::ride on the rider.
Mount saddle(std::shared_ptr<const Character> horse, double from = 0, double until = 1e30);
// Joint j's frame in the character's space at a moment (its look and pose then).
Matrix frameAt(const Character& ch, double seconds, Joint j);

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
