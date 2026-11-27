// rmtn/scene3d.hpp -- GPU 3D scenes on the timeline (the @remotion/three / Three.js half).
//
//     auto scene = std::make_shared<g3d::Scene>();
//     scene->camera.position = { 0, 2, 6 };
//     g3d::Object3D* knot = scene->add(g3d::Geometry::knot(1, 0.35f), { mkColor(0x47dad8), 0.3f });
//     knot->up = [](int t, g3d::Object3D& o) { o.rotation.y = t * 2.f; };
//     scene->lights.push_back(g3d::Light::directional({ 3, 5, 2 }, WHITE, 1.2f, true));
//     addRoot(c.roots, 0, 300)->fn = g3d::draw(scene);
//
// A scene renders offscreen (its own colour and depth) at the canvas resolution, so it is
// supersampled like everything else, and composites over the 2D content beneath it with
// the node's opacity. Geometry, models, textures and shaders are created on first draw,
// so scenes can be built before a window exists.
//
// Conventions: right-handed, +y up; rotations are Euler XYZ in degrees; colours are sRGB
// and lighting is computed in linear space. Like NodeProps, `up` callbacks should set
// absolute values from t so any frame renders the same on its own.
#pragma once
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "timeline.hpp"
#include "particles.hpp"
#include "weather.hpp"
#include "lightning.hpp"

namespace rmtn {
namespace splat { struct Cloud; }
namespace g3d {

// ------------------------------------------------------------------ geometry and models
// A mesh description; the GPU mesh is generated on first use and freed by shutdownGraphics.
class Geometry : public std::enable_shared_from_this<Geometry> {
public:
    enum Buffer : unsigned {
        PositionBuffer = 1u << 0, TexcoordBuffer = 1u << 1, NormalBuffer = 1u << 2,
        ColorBuffer = 1u << 3, TangentBuffer = 1u << 4, Texcoord2Buffer = 1u << 5,
        IndexBuffer = 1u << 6
    };
    explicit Geometry(std::function<Mesh()> make) : make_(std::move(make)) {}
    ~Geometry();
    static std::shared_ptr<Geometry> box(float w, float h, float d);
    static std::shared_ptr<Geometry> sphere(float radius, int rings = 32, int slices = 48);
    static std::shared_ptr<Geometry> plane(float w, float d, int resX = 1, int resZ = 1);  // faces +y
    static std::shared_ptr<Geometry> cylinder(float radius, float height, int slices = 48);   // centred, along y
    static std::shared_ptr<Geometry> cone(float radius, float height, int slices = 48);       // centred, apex +y
    // Ring in the xy plane: `radius` to the tube's centre, `tube` the tube's radius.
    static std::shared_ptr<Geometry> torus(float radius, float tube, int radialSegs = 64, int sides = 24);
    // (p, q) torus knot, as Three.js's TorusKnotGeometry.
    // A slab: a w x h rectangle with corners rounded to `radius` in the xy plane, d deep along
    // z, centred (a phone, a tablet, a card, a window). Sharp front and back edges.
    static std::shared_ptr<Geometry> roundedSlab(float w, float h, float d, float radius, int cornerSegments = 10);
    static std::shared_ptr<Geometry> knot(float radius, float tube, int tubularSegs = 256, int radialSegs = 24,
                                          int p = 2, int q = 3);
    const Mesh& mesh();
    // Reshapes the mesh: `edit` changes its CPU copy (vertices, normals; same counts) before
    // the next draw, which re-uploads them. Safe to call from `up` callbacks every frame
    // (morphs, growing shapes); the latest edit wins.
    void update(std::function<void(Mesh&)> edit);
    // Also synchronize the named fixed-topology GPU buffers after the edit. Before the next
    // draw, the latest edit per buffer set wins; edits for different sets apply in order.
    void update(std::function<void(Mesh&)> edit, unsigned buffers);
private:
    std::function<Mesh()> make_;
    std::vector<std::pair<unsigned, std::function<void(Mesh&)>>> pending_;
    Mesh mesh_{};
    bool loaded_ = false;
};
using GeometryPtr = std::shared_ptr<Geometry>;

// A texture made by code and remade as the frames go by (heat maps, clouds, simulations).
// `make(t)` returns a new image for frame t (the texture takes it over); it is called again
// every `every` frames, the frames between reusing the last. Keep the size the same from
// call to call: the GPU texture is then updated in place.
//
// Or drawn on the GPU like a small canvas: `draw(c, t)` paints with dr::/vg:: into a
// transparent width x height texture (c maps texture px; supersampled `supersample`
// times), so vector faces, logos and labels stay sharp on a mesh. `draw` wins over `make`.
//     auto face = std::make_shared<g3d::DynamicTexture>();
//     face->width = face->height = 512;
//     face->draw = [](const Ctx& c, int t) { vg::fill(c, vg::Path::circle({ 256, 256 }, 200), RED); };
struct DynamicTexture {
    std::function<Image(int t)> make;
    int every = 1;
    std::function<void(const Ctx& c, int t)> draw;
    int width = 512, height = 512, supersample = 2;
    bool repeat = false;        // `draw` canvases clamp at the edge unless set (a tile, e.g. a facade)
};
using DynamicTexturePtr = std::shared_ptr<DynamicTexture>;

// A model file (glTF/GLB, OBJ, IQM, M3D, VOX) and its skeletal animations, loaded on
// first use. Throws std::runtime_error from the first draw when the file cannot be read.
class ModelAsset : public std::enable_shared_from_this<ModelAsset> {
public:
    explicit ModelAsset(std::string path) : path_(std::move(path)) {}
    ~ModelAsset();
    const std::string& path() const { return path_; }
    Model& model();
    int animationCount();
    int animationIndex(const std::string& name);      // -1 when absent
    int animationFrames(int index);                   // keyframes in animation `index`
    // Poses the skeleton (fractional frames interpolate; frames wrap).
    void pose(int animation, float frame);
    // Drives the bone matrices directly (custom skinning, e.g. the rig:: skeleton).
    // Overrides every pose() call until clearBones().
    void poseBones(const Matrix* matrices, int count);
    void clearBones();
    // Blend skinned vertices as dual quaternions (keeps volume at bent joints; the bone
    // matrices must be rigid) instead of linearly.
    bool dualQuaternion = false;
private:
    void ensure();
    std::string path_;
    Model model_{};
    ModelAnimation* anims_ = nullptr;
    int animCount_ = 0;
    bool loaded_ = false;
    bool overrideBones_ = false;
};
using ModelPtr = std::shared_ptr<ModelAsset>;
ModelPtr loadModel(const std::string& path);

// ------------------------------------------------------------------ materials
// How a lit material turns light into colour. The stylised modes use the scene's lights,
// shadows and fog; Toon and Hatch also use its ambient sky and ground colours.
enum class Shading {
    Standard,   // physically based: roughness, metalness, environment reflections
    Toon,       // cel: flat lighting bands, a hard-edged highlight and an optional rim light
    Gooch,      // technical illustration: cool to warm as the key (first) light comes round
    Hatch,      // pen and ink: strokes over the base colour, denser and crossed where darker
};

struct Material {
    Color color = WHITE;            // base colour (multiplies the texture and vertex colours)
    float roughness = 0.5f;         // 0 = mirror-sharp highlights, 1 = matte
    float metalness = 0.0f;         // tints highlights with the base colour, darkens diffuse
    float reflection = 1.0f;        // scales the sky/environment mirrored in it (not its diffuse
                                    // sky light): glass that reflects a dusk sky under a low sky.lighting
    Color emissive = BLACK;         // added light, unaffected by lighting
    float opacity = 1.0f;           // < 1: drawn after opaque objects, back to front
    bool unlit = false;             // flat base colour (no lights, shadows or fog)
    bool doubleSided = false;
    bool castShadow = true, receiveShadow = true;
    bool normalAlphaShade = false;  // the normal map's alpha is the share of direct light that reaches each
                                    // texel (a baked self-shadow; impostors use it): scales every light but the ambient
    bool shadowOnly = false;        // drawn into the shadow maps only: a stand-in that casts what
                                    // the lit object would, from another pose (impostor shadows)
    std::string map;               // optional base-colour texture file
    DynamicTexturePtr dynamicMap;   // or a texture made per frame (wins over `map`); its alpha
                                    // multiplies opacity: give a see-through overlay opacity
                                    // just under 1 so it draws with the transparent objects

    // Cut-out: > 0 drops every fragment whose alpha (texture x colour x vertex colour) is
    // below it, in the picture, the shadows and the ink lines alike; what is kept is opaque
    // (not blended), so leaf and grass cards sort and shadow like solid objects. Alpha is
    // raised with the texture's mip level so a card seen far away does not thin out.
    float alphaCutoff = 0;
    // Light through a thin surface (a leaf, a blade of grass, paper, a lampshade): a light
    // behind it, as seen from the camera, shows through in `translucencyColor` (multiplied by
    // the base colour), brightest when looking towards the light. 0 = opaque to light.
    // Standard shading only.
    float translucency = 0;
    Color translucencyColor = WHITE;
    // Surface detail from textures, in the mesh's uv. `normalMap` is a tangent-space normal
    // map in the OpenGL convention (green = +v); no tangents are needed, the frame comes from
    // the uv's screen derivatives. `normalScale` scales its bumps (0 = flat, < 0 inverts).
    // `roughnessMap` is glTF's metallic-roughness texture: green multiplies `roughness`,
    // blue multiplies `metalness`. Standard shading only.
    std::string normalMap;
    DynamicTexturePtr dynamicNormalMap;     // or one made by code (wins over `normalMap`)
    float normalScale = 1;
    std::string roughnessMap;

    Shading shading = Shading::Standard;
    struct Toon {
        int bands = 2;              // lit steps per light above the shadow (1 = lit or unlit)
        float softness = 0.03f;     // width of each step's edge, as a fraction of the light range
        float highlight = 0.6f;     // strength of the hard highlight (0 = none)
        float highlightSize = 0.12f;// larger = bigger highlight spot
        Color rim = BLANK;          // rim light colour; alpha is its strength (BLANK = none)
        float rimWidth = 0.3f;      // how far the rim reaches in from the silhouette (0..1)
    } toon;
    struct Gooch {
        Color warm{ 255, 200, 90, 255 }, cool{ 50, 80, 190, 255 };
    } gooch;
    struct Hatch {
        Color ink{ 32, 28, 40, 255 };
        float spacing = 7;          // composition px between strokes
        float angle = 40;           // degrees; the cross strokes are at +90 and +45
        float weight = 1;           // stroke thickness multiplier
    } hatch;

    // A cut by a plane in world space: nothing is drawn where dot(position, normal) > offset
    // (so a front can sweep through an object), and a band `edgeWidth` wide in front of the cut
    // glows in `edgeColor` (alpha = strength). The default offset hides nothing. Shadows
    // are not cut.
    struct Clip {
        Vector3 normal{ 1, 0, 0 };
        float offset = 1e9f;
        float edgeWidth = 0;
        Color edgeColor = BLANK;
    } clip;

    // Ink lines, when the scene's lines are on (Scene::lines): colour (BLANK = the scene's)
    // and a multiplier on the scene's line width (0 = no lines around this object).
    Color lineColor = BLANK;
    float lineWidth = 1;

    // Wet and snowy weather (Scene::ground): false keeps this surface dry and bare under any weather
    // (glass, an interior, a painted sign). Standard materials that are lit only.
    bool weathering = true;
};

// ------------------------------------------------------------------ instancing
// A mesh of fixed topology whose vertices are baked for every frame of an animation, drawn
// many times in one call. `rest` carries what does not change (indices, uvs, vertex
// colours); the per-frame positions and normals are read in the vertex shader from a texture
// the renderer makes from `positions` and `normals`. Build one in `build`, never per frame.
struct AnimatedMesh {
    GeometryPtr rest;                       // any frame's mesh: same indices, uvs and colours
    int frames = 0, vertices = 0;
    std::vector<float> positions, normals;  // frames * vertices * 3, frame-major
    Vector3 centre{ 0, 0, 0 };              // a sphere holding every vertex of every frame
    float radius = 0;
};

// One body of an instanced set. `frame` is a position in the clip [clipStart,
// clipStart + clipLength): its fraction blends two baked frames, and the clip loops.
struct Instance {
    Vector3 position{ 0, 0, 0 };
    float yaw = 0, pitch = 0, bank = 0;     // degrees, as a character's holder and tilt nodes
    float scale = 1;
    float frame = 0;
    int clipStart = 0, clipLength = 1;
    float shade = 1;                        // brightness multiplier
    Color tint = WHITE;                     // colour multiplier (alpha unused): tint x shade per
                                            // channel is kept to 8 bits over 0..2
    // Coverage as a screen door (steps of 1/32): 1 = whole; f > 0 keeps a fraction f of the
    // pixels, f < 0 the other 1 + f, so a body at u and its stand-in at -u fill each pixel
    // once (dithered cross-fades between tiers, no popping). Shadows fade the same way.
    float fade = 1;
    // With fade > 0: also drop the pixels below this level (0..1, steps of 1/32), so the body
    // keeps the band [fadeFrom, fade) of the screen door. Several stand-ins can then share a
    // pixel between them, each in its own band.
    float fadeFrom = 0;
    // An orientation given as directions, for bodies that stand on a curved surface or face a
    // viewer: when `aim` is not zero it replaces pitch, bank and yaw. The body's +z turns to
    // `aim`, and its +y is as near `aimUp` as that allows (so a quad faces the camera and its
    // top leans along the surface's up). `aimUp` parallel to `aim` falls back to world +y.
    Vector3 aim{ 0, 0, 0 };
    Vector3 aimUp{ 0, 1, 0 };
};

// An object's instances: `up` refills `list` each frame from t (pure, like any `up`). They
// are drawn in the object's space with its `material` (opaque or cut out, not blended; the
// colour comes from the mesh's vertex colours, multiplied by the material's and each
// instance's tint and shade). Each one is culled on its own (see
// Object3D::cullRadius), shadow footprint included, so only what can show is submitted.
struct Instances {
    std::shared_ptr<AnimatedMesh> mesh;
    std::vector<Instance> list;
};

// ------------------------------------------------------------------ fields (grass)
// Grass at landscape scale: every blade is made on the GPU from its tile and its number, so
// nothing per blade is stored or sent, and a million blades cost what their pixels cost.
// The area is cut into square tiles; each frame the CPU keeps the tiles that can show (their
// shadows included) and picks a level for each from its distance, and the vertex shader
// places, shapes, colours and bends each blade. The same seed makes the same field, in any
// frame order; the wind is a function of position and time, never integrated.
//
// Blade k of a tile sits at point k of a low-discrepancy sequence, so any first n blades of a
// tile cover it evenly. Far away fewer are drawn (by rank, each shrinking away over a band
// rather than popping) and the ones left grow wider, so the cover holds until the fade.
// `mask`, `ground` and `tall` are sampled over the area once, into a texture of
// `mapResolution` squared, when the field is first drawn: change them, then bump `revision`.
// Drawn with the object's material (its colour multiplies the blade colours; translucency,
// roughness and receiveShadow apply); always double-sided.
struct Field {
    Vector2 min{ -10, -10 }, max{ 10, 10 };     // the area: x and z in the object's space
    float density = 400;                        // blades per square metre where `mask` is 1
    uint32_t seed = 1;

    float height = 0.3f;                        // metres
    float heightVariation = 0.35f;              // +- this fraction, per blade
    float width = 0.012f;                       // metres at the root; blades taper to a point
    float lean = 0.25f;                         // resting bend: the tip's offset over the height
    int segments = 4;                           // along a near blade (far blades have one)
    Color root{ 38, 66, 24, 255 }, tip{ 128, 168, 64, 255 };    // colour from root to tip
    float colorVariation = 0.3f;                // per-blade brightness spread, 0..1
    Color dry{ 176, 156, 86, 255 };             // straw colour...
    float dryness = 0.12f;                      // ...and the fraction of blades turning to it
    int colorBands = 0;                         // > 1: root to tip in this many flat bands (toon looks)

    std::function<float(float x, float z)> ground;      // ground height (null = 0)
    std::function<float(float x, float z)> mask;        // density, 0..1 (null = 1)
    std::function<float(float x, float z)> tall;        // height multiplier (null = 1)
    // Growth: each spot's grass rises when `grow` (0..1, may follow t) passes its arrival
    // (0..1; null = 0, all at once), over 0.15 of `grow`; so blades rise rather than pop.
    // flora::arrivalFrom / arrivalAlong / arrivalInto make the usual fronts.
    std::function<float(float x, float z)> arrival;
    float grow = 1;
    int mapResolution = 256;
    int revision = 0;                           // bump after changing the four maps above

    Vector2 windDirection{ 1, 0.3f };           // x, z; normalised when drawn
    float sway = 0.12f;                         // steady lean downwind and flutter, tip offset / height
    float gust = 0.35f;                         // extra bend while a gust passes
    float gustSpeed = 5;                        // metres a second the gust fronts travel downwind
    float gustScale = 14;                       // metres between gust fronts
    float fps = 30;                             // the film's frames per second: wind moves in seconds
    // With Scene::weather enabled the five wind settings above come from it (see weather.hpp:
    // the direction, sway from the speed, gust, gustSpeed, gustScale); true keeps this field's own.
    bool ownWind = false;

    float tileSize = 4;                       // metres
    float detailDistance = 14;                  // nearer tiles get full blades, further one segment
    float fadeStart = 35, fadeEnd = 70;         // blades thin out over this band (metres from the camera)
    float shadowDistance = 18;                  // with castShadows: only tiles nearer than this to the camera cast (all receive)
    // Blades into the shadow maps, so a tall tuft shades the grass behind it and the ground. On
    // by default, every blade the main pass draws. Each tile within `shadowDistance` of the camera
    // and inside a directional light's shadowExtent is drawn into the maps with the same wind at
    // the same t. A `shadowDensity` below 1 is a saving: a random share of the blades, each widened
    // by 1 / shadowDensity (at most 3 times) so the shadow keeps its weight. The cost is a
    // blocky weave of blade shadows on the soil in the nearest metres under a low sun (shadow-map
    // texels, worse as the share falls). Blades both cast and receive: the lit shader's normal
    // offset and slope bias keep them from speckling themselves. Off: the grass casts nothing.
    bool castShadows = true;
    float shadowDensity = 1;                    // 0.02..1, the share of blades drawn into the shadow maps

    // What the shader draws, mirrored on the CPU for tests and placement: blade k of tile
    // (tx, tz) has its root at x, z in the object's space (before masks and fading).
    Vector2 bladeRoot(int tx, int tz, uint32_t k) const;
    int tilesX() const;
    int tilesZ() const;
    uint32_t bladesPerTile() const;             // density * tileSize^2, rounded
};
// Blades the last rendered frame submitted in its main pass, over every field (before the
// shader thins them by mask and distance; shadow passes not counted): for measuring budgets.
long long fieldBladesSubmitted();
// Blades the last rendered frame submitted to its shadow passes, over every field (a tile
// counted once for each shadow map it is drawn into): a deterministic measure of what
// Field::castShadows costs.
long long fieldShadowBladesSubmitted();
// Instanced bodies (trees, say) the last rendered frame drew in its main pass, culled ones not
// counted, and the triangles they came to: a deterministic measure of what a forest costs.
long long instanceBodiesSubmitted();
long long instanceTrianglesSubmitted();

// ------------------------------------------------------------------ gaussian splats
// A cloud of Gaussian splats (see splat.hpp) drawn in the object's space: one instanced quad
// per splat, depth-sorted back to front each frame, blended over what is behind them and
// hidden by opaque objects in front. Not lit, shadowed, fogged or tinted by water; the colour
// is the splat's own (view-dependent where the cloud has spherical harmonics). Splats are
// meant for up to about a million. `up` sets the look from t.
struct Splats {
    std::shared_ptr<const splat::Cloud> cloud;
    struct Look {
        float scale = 1;            // multiplies every splat's size
        float opacity = 1;          // multiplies every splat's opacity
        int shDegree = 1;           // 0 = flat colour, 1 = view-dependent where the cloud has it
        // A sweeping front, in world space: splats with dot(position, clipNormal) > clipOffset
        // are gone; those within `clipBand` behind it fade out and scatter by up to `scatter`.
        Vector3 clipNormal{ 1, 0, 0 };
        float clipOffset = 1e9f;
        float clipBand = 0.5f;
        float scatter = 0;
    } look;
};

// ------------------------------------------------------------------ 2D surfaces
// A 2D picture placed in the scene: a phone or laptop screen, a sign, a page, a diagram on a
// wall, a label. It is drawn with the ordinary 2D calls (dr::, vg::, text, a timeline tree)
// in its own design pixels, 0..pixels, on its own clock (frame - start), into a texture on
// a flat rectangle `size` metres across, in the object's x-y plane and facing +z.
//
// The texture's resolution follows the camera: each frame the renderer measures how many
// screen pixels one design pixel covers and draws the canvas at that density (in quarter-
// octave steps, rounded up, between minResolution and maxResolution), so text stays sharp as
// the camera pushes in and does not shimmer far away.
//
// By default it is unlit and casts no shadow, so a screen shows its own colours whatever the
// lighting (set material.unlit = false for paper lit by the scene). A background with alpha
// makes it see-through (drawn with the transparent objects, still hidden by what is in front).
//
// billboard = true turns it square to the camera every frame (a label); with pixelSize > 0
// it also keeps a constant size on screen (pixelSize screen px per design px), and `lift`
// moves it that many metres toward the camera so it clears what it labels. Unlike a 2D
// overlay placed with project(), it is hidden by anything in front of it.
//
//     auto ui = std::make_shared<g3d::Surface>();
//     ui->pixels = { 390, 844 };  ui->size = { 0.0715f, 0.1547f };
//     ui->draw = [](const Ctx& c, int t) { dr::rect(c, 0, 0, 390, 844, WHITE); ... };
//     scene->add(ui)->position = { 0, 1, 0 };
//
// To fly into it until it is the frame, see fillCamera and fillCtx below.
struct Surface {
    std::function<void(const Ctx& c, int t)> draw;  // paints design px; t = frame - start
    Nodes nodes;                    // and/or a timeline tree, drawn after `draw` on the same clock
    Vector2 pixels{ 1920, 1080 };   // design size
    Vector2 size{ 1.6f, 0.9f };     // metres (read when added)
    Vector2 anchor{ 0.5f, 0.5f };   // the point of the picture at the object's origin: 0,0 top-left (read when added)
    int start = 0;                  // the scene frame at which the surface's own clock reads 0
    Color background = BLANK;       // filled first; alpha < 255 makes the surface see-through
    float sharpness = 1;            // texture px per screen px wanted (more for text seen at a slant)
    float minResolution = 0.125f, maxResolution = 4;   // texture px per design px
    bool billboard = false;
    float pixelSize = 0;            // billboard: screen px per design px, whatever the distance (0 = metres)
    float lift = 0;                 // metres toward the camera
    float resolution = 1;           // set by the renderer each frame: texture px per design px
};

// ------------------------------------------------------------------ 3D lines
// Strokes in the scene for diagrams: polylines, outlines, leaders and arrows. Points are in
// the object's space (they move with it); the width is in composition px at any distance
// (or metres with worldWidth). Round joins and caps, antialiased, unlit, fogged, and hidden
// by whatever stands in front of them (onTop draws them over everything). They do not write
// depth or cast shadows.
//
//     auto l = std::make_shared<g3d::Lines>();
//     l->arrow({ 0, 0, 0 }, { 1, 0.5f, 0 }, TEAL, 4);                    // 4 px wide
//     l->path({ { -1, 0, 0 }, { -1, 1, 0 }, { 0, 1, 0 } }, WHITE, 2).dash = 10;
//     g3d::Object3D* o = scene->add(l);
//     o->up = [l](int t, g3d::Object3D&) { l->paths[0].to = std::min(1.f, t / 30.f); };   // draws on
//
// Limits: a translucent colour shows darker spots where the round joins overlap the strokes.
struct Lines {
    enum class Head { None, End, Start, Both };
    struct Path {
        std::vector<Vector3> points;
        Color color = WHITE;
        float width = 3;            // composition px (metres with worldWidth)
        bool closed = false;
        float from = 0, to = 1;     // the part drawn, as fractions of the length (animate `to` to draw it on)
        Head head = Head::None;     // arrowheads ride at the ends of the drawn part
        float headSize = 4;         // arrowhead length, in widths (its width is 0.9 of its length)
        float dash = 0, gap = 0;    // dashes along the screen, composition px (0 = solid; gap 0 = dash)
        float dashOffset = 0;       // composition px; animate it for marching ants
    };
    std::vector<Path> paths;
    bool worldWidth = false;
    bool onTop = false;             // ignore depth: drawn over the whole scene
    float lift = 0;                 // metres toward the camera, for lines drawn on a face

    // Builders: each adds a path and returns it, so it can be tuned in place.
    Path& path(std::vector<Vector3> points, Color color = WHITE, float width = 3, bool closed = false);
    Path& line(Vector3 a, Vector3 b, Color color = WHITE, float width = 3);
    Path& arrow(Vector3 from, Vector3 to, Color color = WHITE, float width = 3, Head head = Head::End);
    // A circle (or an arc from a0 to a1 degrees) round `normal`; angle 0 lies along `start`
    // (any vector not parallel to the normal; default: the one nearest world x).
    Path& circle(Vector3 centre, Vector3 normal, float radius, Color color = WHITE, float width = 3,
                 float a0 = 0, float a1 = 360, Vector3 start = { 0, 0, 0 }, int segments = 72);
    // An axis-aligned box's 12 edges, as 6 paths.
    void box(Vector3 min, Vector3 max, Color color = WHITE, float width = 3);
};

// ------------------------------------------------------------------ objects
struct Object3D {
    std::string name;
    Vector3 position{ 0, 0, 0 };
    Vector3 rotation{ 0, 0, 0 };    // Euler XYZ, degrees
    Vector3 scale{ 1, 1, 1 };
    bool visible = true;            // hides the subtree too
    // Culling, opt-in: a bounding sphere for the whole subtree, centred at `cullCentre` in
    // this node's own space. While it is out of sight (outside the camera's frustum, and no
    // shadow of it reaching the view; see Scene::cullFloor) the subtree is skipped whole: its
    // `up` callbacks do not run, its canvas textures are not drawn, and none of it is
    // submitted. This node's own `up` still runs. Frames stay pure functions of t: every `up`
    // assigns from t, so the first frame back in view recomputes everything. 0 = never.
    float cullRadius = 0;
    Vector3 cullCentre{ 0, 0, 0 };
    bool culled = false;            // set by render() each frame

    GeometryPtr geometry;           // draw a mesh with `material`...
    Material material;
    ModelPtr model;                 // ...or a model file
    bool modelMaterials = true;     // model: keep the file's colours/textures (else `material`)
    std::vector<Color> modelMaterialTints; // optional per-material RGBA multipliers, indexed by Model::materials
    int animation = -1;             // model: animation index to pose, -1 = bind pose
    float animationFrame = 0;       // model: keyframe (fractional ok)
    // Particles born in this object's space (fx::Emitter3D). With a `geometry` too, each
    // particle is an instance of it in `material` (tinted by the particle's colour), and the
    // geometry itself is not drawn.
    std::shared_ptr<const fx::Emitter3D> emitter;
    std::shared_ptr<Instances> instances;               // many copies of one baked mesh
    std::shared_ptr<Field> field;                       // grass made on the GPU
    std::shared_ptr<Splats> splats;                     // a Gaussian splat cloud
    std::shared_ptr<Surface> surface;                   // a 2D picture (with its own geometry and dynamicMap)
    std::shared_ptr<Lines> lines;                       // strokes and arrows

    std::function<void(int t, Object3D& self)> up;      // animate from the local frame
    std::vector<std::unique_ptr<Object3D>> children;

    Object3D* add(GeometryPtr g, Material m = {});
    Object3D* add(ModelPtr m);
    Object3D* add(fx::Emitter3D e);                    // particles (billboards, streaks, quads)
    Object3D* add(fx::Emitter3D e, GeometryPtr g, Material m = {});   // mesh particles
    Object3D* add(std::shared_ptr<Instances> i, Material m = {});      // instanced bodies
    Object3D* add(std::shared_ptr<Field> f, Material m = {});          // a grass field
    Object3D* add(std::shared_ptr<Splats> s);                          // a splat cloud
    Object3D* add(std::shared_ptr<Surface> s);                         // a 2D picture
    Object3D* add(std::shared_ptr<Lines> l);                           // strokes and arrows
    Object3D* add();                                   // empty group
};

// ------------------------------------------------------------------ lights and camera
struct Light {
    enum class Type { Directional, Point, Spot };
    Type type = Type::Directional;
    Color color = WHITE;
    float intensity = 1.0f;
    Vector3 position{ 3, 5, 2 };    // directional: shines from here towards target
    Vector3 target{ 0, 0, 0 };      // directional and spot
    float range = 0;                // point/spot: light fades to 0 at this distance (0 = no falloff)
    float innerAngle = 20, outerAngle = 30;           // spot cone, degrees from its axis
    // Shadows: any light type. A directional or spot light uses one shadow-atlas tile, a
    // point light six (one per cube face); up to 16 tiles per scene, in light order.
    bool castShadow = false;
    float shadowExtent = 8;         // directional: half-size of the shadowed area around target
    float shadowBias = 0.02f;       // world units, on top of an automatic slope/texel bias
    std::function<void(int t, Light& self)> up;

    static Light directional(Vector3 from, Color c = WHITE, float intensity = 1, bool shadow = false);
    static Light point(Vector3 at, Color c = WHITE, float intensity = 1, float range = 0);
    static Light spot(Vector3 from, Vector3 to, Color c = WHITE, float intensity = 1,
                      float outerAngle = 30, bool shadow = false);
};

struct Camera {
    Vector3 position{ 0, 2, 6 }, target{ 0, 0, 0 }, upVector{ 0, 1, 0 };
    float fov = 45;                 // vertical, degrees (perspective)
    bool orthographic = false;
    float orthoHeight = 10;         // visible world height (orthographic)
    float nearPlane = 0.05f, farPlane = 500;
    // Depth of field: things at focusDistance are sharp; blur grows with
    // aperture * |depth - focus| / depth, up to maxBlur composition pixels. aperture 0 = off.
    // tilt (degrees, perspective only) tilts the plane of focus about the camera's horizontal
    // axis, as a tilt-shift lens does: positive leans its top away (more of a receding ground
    // in focus), negative towards the camera (a thin sharp band: the miniature look).
    struct DepthOfField { float focusDistance = 5, aperture = 0, maxBlur = 12, tilt = 0; } dof;
    std::function<void(int t, Camera& self)> up;
};

// Whether a sphere may be seen from `cam` at the given width/height `aspect`: false only
// when it lies wholly outside the view frustum (with a small margin). What culling tests.
bool visibleSphere(const Camera& cam, float aspect, Vector3 centre, float radius);

// ------------------------------------------------------------------ skies
// A procedural sky: a gradient from the ground through the horizon to the zenith, the sun
// with its glow and the haze round it, stars, and drifting clouds. It is drawn behind the
// scene and lights it (as an environment: diffuse from its blur, reflections of it), and
// fog can take its colours. Remade every frame, so any of it can be animated (Scene::up).
//
//     scene->sky = g3d::Sky::at(18.2f);           // dusk, the sun low in the west
//     scene->sky.sunLight = 0;                    // the sun where light 0 shines from
//     scene->fogMode = g3d::FogMode::Exponential; scene->fogDensity = 0.03f; scene->fogFromSky = true;
struct Sky {
    bool enabled = false;
    Color zenith{ 52, 104, 190, 255 };      // straight up
    Color horizon{ 176, 204, 234, 255 };    // at the horizon
    Color ground{ 72, 70, 66, 255 };        // below the horizon
    float curve = 0.45f;                    // how quickly the horizon gives way to the zenith (smaller: higher)
    Vector3 sunDirection{ 0.35f, 0.45f, -0.8f };    // towards the sun (any length)
    int sunLight = -1;                      // >= 0: take the direction from this light instead
    Color sunColor{ 255, 246, 226, 255 };
    float sunSize = 1.2f;                   // angular radius, degrees (0: no disc)
    float sunGlow = 0.5f;                   // brightness of the glow round the sun
    Color haze{ 255, 196, 140, 255 };       // the horizon's colour under the sun
    float hazeAmount = 0.35f;
    float stars = 0;                        // 0..1: how many show (fading out towards the horizon)
    float clouds = 0;                       // 0..1: cover
    Color cloudColor{ 255, 255, 255, 255 }, cloudShade{ 150, 158, 176, 255 };   // sunlit side, underside
    float cloudScale = 1;                   // larger: smaller clouds
    Vector2 cloudDrift{ 0.0004f, 0.0001f }; // per frame
    // Added to the drift's travel (sky-image units, not per frame). Scene::weather sets it, from
    // the wind's travel, so a changing wind does not make the clouds swim; leave it 0 otherwise.
    Vector2 cloudShift{ 0, 0 };
    // With Scene::weather enabled: the cover follows weather.clouds.cover, the colours grey with it
    // and the clouds drift downwind (cloudDrift and cloudShift are replaced). true keeps this sky's own.
    bool ownClouds = false;
    // With Scene::weather enabled: the sky lights the scene less under cloud (`lighting` x
    // 1 - 0.35 cover), and with `sunLight` set that light dims (x 1 - 0.75 cover) while the ambient
    // light rises a little (x 1 + 0.35 cover). true keeps the scene's own light.
    bool ownLight = false;
    unsigned seed = 1;
    int resolution = 1024;                  // width of the sky's equirectangular image (2:1)
    float lighting = 1;                     // how strongly it lights the scene
    bool show = true;                       // draw it as the backdrop

    // Presets by the hour of a day (0..24): night, dawn, morning, noon, evening, dusk; the
    // sun rises in the east (+x) at 6, is highest at noon (to the south, -z) and sets at 18.
    static Sky day();
    static Sky dawn();
    static Sky dusk();
    static Sky night();
    static Sky at(float hour);
    static Sky mix(const Sky& a, const Sky& b, float t);    // colours and amounts; the rest from b at t >= 0.5
};

// How fog thickens with distance. Exponential fogs use fogDensity (per world unit) and,
// with fogFalloff > 0, thin out with height above fogHeight (ground fog in the valleys).
enum class FogMode { Linear, Exponential, Exponential2 };

// ------------------------------------------------------------------ water
// A sea, lake or pool: an endless surface at `level` that rolls with a sum of waves, seen from
// above (the sky or environment reflected by Fresnel, sun glints, foam on the crests) and from
// below (a bright window onto the sky ringed by mirror, the way it looks from a diver's eye).
// Under it, everything lit is tinted and darkened with depth and distance (red first), and
// the sun that reaches it is refracted by the waves into moving caustics.
//
//     scene->water = g3d::Water::calm();
//     scene->water.level = 0;
//     scene->water.openDepth = 30;      // a deep sea: no floor shows through; 0 = clear glass
//
// Everything is a pure function of time, and `height` gives the CPU the same surface the
// shader draws, so a boat can ride it. Reflections are of the sky or environment, not of the
// objects in the scene.
struct Water {
    bool enabled = false;
    float level = 0;                    // mean surface height (world y)

    // One wave of the sum: it travels towards `direction` (degrees about +y, 0 = +x, 90 = +z).
    // sharpness 1 is a sine; larger narrows the crests and flattens the troughs.
    struct Wave {
        float direction = 0, wavelength = 8, amplitude = 0.1f, sharpness = 1, speed = 1;
    };
    std::vector<Wave> waves;            // up to 8
    float speed = 1;                    // times every wave's speed
    float fps = 30;                     // the film's frames per second: waves move in seconds
    // Metres of open water upwind. Under Scene::weather the wind can only raise waves as high as
    // this fetch allows (JONSWAP, see Weather::waveHeight): a 20 m pond ruffles, a bay chops, the
    // open sea swells. Default 50 km; calm() 2 km (a lagoon), choppy() 10 km (a bay), storm()
    // 200 km (open sea). Without weather it is unused.
    float fetch = 50000;
    // The shading band-limits the waves by the pixel footprint: a wave under a few pixels long fades out of
    // the normal, and the slope variance it takes away is added to the glint's roughness, so a far or tiny
    // chop reads as a broad rough sheen rather than an aliased pattern. false draws every wave at every
    // distance (for comparison). Only the shading filters: height(), slope() and the vertices are exact.
    bool filter = true;
    // With Scene::weather enabled the waves follow its wind: they head downwind (their spread of
    // directions kept, about their amplitude-weighted mean heading) and their height and length
    // become those the wind raises over `fetch` (authored shape kept).
    // true keeps this water's own waves. Use waterOf(scene) for the surface that is drawn.
    bool ownWind = false;

    // What the surface mirrors. Off (the default) reflects the sky only, as always: no extra pass, no pixel
    // changes. Scene also reflects what stands in the scene: the scene is drawn again, mirrored about the water
    // level (everything below the level cut away, so nothing under the water leaks in), into an offscreen
    // image that the surface samples where it is seen from above. That image holds the opaque objects,
    // grass, trees, other transparent objects and the volumetric clouds; the sky is not drawn into it, the
    // water's own sky reflection shows where nothing was drawn. Not in it: the water itself, line art,
    // billboard particles and splats. Half and Quarter draw the pass at that share of the screen's width and
    // height (a quarter of the cost, softer edges). The pass is skipped when no water can be on screen, from
    // under the surface and with an orthographic camera.
    enum class Reflections { Off, Scene, Half, Quarter };
    Reflections reflections = Reflections::Off;
    // How far the wave normals bend the reflection: 1 follows the optics (a reflected object is displaced as
    // the tilted surface turns the ray), 0 is a flat mirror, 2 doubles it. The normal is the shading's own,
    // band-limited one, so far water does not shimmer, and the bending fades with distance.
    // The slope variance the filter takes out also blurs the reflection (a few taps of a mipmapped image).
    // It also follows how tall the sea is: scaled by smoothstep(0.01 m, 0.1 m) of the significant wave height
    // Hs = 4 sqrt(sum a^2 / 2), so a lake rippled a centimetre high stays a mirror (a 1 m/s wind over 900 m is
    // Hs 1.5 cm) and a chop of 10 cm and more breaks the image fully. This is a choice, not optics: steep
    // millimetre ripples would smear a grazing reflection, and a calm lake should not.
    float reflectionDistort = 1;
    // true: what lies under the surface is seen refracted from above. The picture of the opaque scene is
    // copied before the water is drawn; the surface looks through it along the refracted ray (Snell, index
    // 1.333, steered by the same normal) and absorbs over the real path through the water, from the depth
    // buffer, with `absorb`. Off: the objects are tinted where they are, by the straight view path, as always.
    // Seen from under the surface nothing changes either way.
    bool refraction = false;

    // An open sea from a wave spectrum, in place of the handful of sinusoids in `waves` (W9). Off by default: a
    // scene that does not enable it draws exactly as before. The surface is an FFT ocean (Tessendorf): a
    // directional JONSWAP spectrum of the wind (speed, fetch) is turned into heights, horizontal displacement
    // (choppiness), slopes and a whitecap map on a periodic grid, by an inverse FFT on the CPU every frame, and
    // uploaded as textures. Frame t is a pure function of t: h(k, t) = h0(k) e^{-i w t} + conj(h0(-k)) e^{+i w t},
    // with h0 drawn from a seeded hash and w = sqrt(g k).
    //   - Cascades: the spectrum is split by wavenumber into up to four grids of 128 x 128 with patch sizes
    //     about 7.5 times apart (about 300 m, 40 m, 5 m, 0.7 m for a 10 m/s sea; see patchSizes()), so each carries
    //     only its own band (cross-faded, no energy counted twice) and no one patch is a recognisable tile. The
    //     lookup coordinate of each is also bent by a slow sine warp (`decorrelate`) so its repeats are not exact copies.
    //   - Height, slope and ride: height() and slope() evaluate the same grids as the mesh. They include the cascades
    //     whose patch is at least 8 m (the finer ones only shade: ripples a few centimetres high, which the mesh
    //     cannot show). Choppy displacement is inverted by four fixed-point steps. They include the curvature.
    //   - Distance: the mesh is a set of rings round the camera, out past the sea horizon. Its vertices read the
    //     mip level that matches their spacing, so the geometry fades from fine to coarse without popping; the
    //     shading reads the level that matches the pixel's footprint, and the slope variance it takes away goes
    //     into the glint's roughness (as for `filter`). Far sea is a smooth sheen, not a pattern.
    //   - Horizon: with `curvature` the surface falls by d^2 / 2R below the level (R = 6 371 km, the clouds' planet,
    //     d from the origin), so from height h the horizon sits sqrt(2 h / R) below horizontal. The sky behind
    //     it is filled in to that horizon, and the sea fades into its colour over `visibility` metres.
    //   - Whitecaps: where the Jacobian of the displacement (J < 1 where the surface bunches up) is lowest, in
    //     the cascade of the breaking waves, with the area following the wind (about 0.1 % at 4 m/s, 2 % at
    //     10 m/s, 20 % at 20 m/s, less on a short fetch) and the foam fading over a few seconds after each crest.
    //     Spray streaks along the wind begin at 12 m/s and are full at 20.
    // The sea is seen above or below the surface as before. Objects below it are tinted against the mean level.
    // Scene::weather (unless ownWind) sets windSpeed and windDirection; `fetch` above is the fetch. Hs follows
    // Weather::waveHeight(fetch); the spectrum is rescaled so its realised significant height is exactly that.
    struct Spectrum {
        bool enabled = false;
        int cascades = 3;               // 1..4 grids (each 128 x 128)
        float windSpeed = 10;           // m/s at 10 m, when no weather sets it
        float windDirection = 0;        // degrees about +y the waves travel towards (0 = +x, 90 = +z)
        float gamma = 3.3f;             // JONSWAP peak enhancement
        float spread = 8;               // directional spreading: at the peak energy goes as cos^(2 spread)(angle / 2) off the wind (about 29 degrees' standard deviation); shorter waves spread wider
        float choppiness = 1;           // horizontal displacement lambda (0: pure vertical); crests sharpen as it grows
        float swell = 0;                // 0..1: the share of the variance that is a longer, narrower swell from another heading
        float swellDirection = 30;      // degrees off the wind the swell travels
        float patch = 0;                // metres: the largest cascade's patch (0: 8 peak wavelengths, 120 to 2000 m)
        float visibility = 60000;       // metres over which the sea fades into the horizon haze (0: no haze)
        float spray = 1;                // 0..1 strength of the wind-streaked spray at high winds
        bool curvature = true;          // the sea follows the planet's curvature (R = 6 371 km about the world origin)
        bool decorrelate = true;        // bend each cascade's lookup so tiles are not exact repeats (false: for comparison)
        unsigned seed = 1;              // the realisation
        // How much the sea looks like a gale (W5), 0..1; -1: from the wind, smoothstep(12, 20 m/s). At 1 the sea is darker and
        // greyer (it scatters back the grey sky's light, and the foam and spray are white on it), the crests are sharper
        // (choppiness x 1.5, only under a Weather: a sea that was set up by hand keeps its choppiness), foam lies in streaks
        // along the wind down the troughs as well as in whitecaps, and a haze of spray hangs over the surface.
        float storm = -1;
    } spectrum;

    Color deep{ 6, 62, 84, 255 };       // what the water scatters back
    Vector3 absorb{ 0.34f, 0.07f, 0.04f };   // loss per metre of path, per red/green/blue
    float openDepth = 0;                // > 0: water this deep under the surface, so it is opaque
    float caustics = 1;                 // 0..1: strength of the caustics on what is below
    float foam = 0.6f;                  // 0..1: foam on the highest crests
    float sunGlint = 1;                 // strength of the sun's glitter

    static Water calm();                // a lagoon: small ripples, a gentle swell
    static Water choppy();              // a breezy lake or bay
    static Water storm();               // open sea: steep crests, foam

    // The surface height at (x, z) at `seconds`, and its slope there (d height / dx, dz).
    float height(float x, float z, double seconds) const;
    Vector2 slope(float x, float z, double seconds) const;
    // The spectrum's significant wave height Hs (m), the patch sizes of its cascades (largest first, m), and the
    // fraction of the sea under whitecaps (the area law, before the foam's own strength): from `spectrum`, `fetch`.
    float spectrumHs() const;
    std::vector<float> patchSizes() const;
    float whitecapCover() const;
    // The mean square slope (both axes) the shading uses: the grids' plus the unresolved tail that widens the glint (spectrum only).
    float shadingSlopeVariance(float* tail = nullptr) const;
    // The horizon dip (radians below horizontal) of an eye `above` metres over the sea: atan(sqrt(2 above / R)).
    static float horizonDip(float above);
    static constexpr float kPlanetRadius = 6371000.f;
    // Whether a world point lies under the surface at `seconds`.
    bool submerged(Vector3 p, double seconds) const { return enabled && p.y < height(p.x, p.z, seconds); }
};

// ------------------------------------------------------------------ water bodies
// A lake, pool or river: water that fills a shape instead of the whole world, in Scene::waterBodies. It is
// drawn by the same shader as Scene::water (Fresnel sky, W6 reflection and refraction, band-limited waves, the
// weather's wind and fetch, rain rings) with these differences:
//   - Its look is `look`, a Water of its own; `look.level` and `look.enabled` are ignored (`level` is used).
//     Its surface geometry is flat (at `level`, plus a river's fall): waves show in the shading and in
//     height(), which a boat may ride, not in the silhouette.
//   - Shore: the depth of the opaque scene behind the surface gives the water's depth to what lies under it.
//     The surface fades to nothing over `edgeSoft` metres of depth, so there is no line where it meets the bank,
//     and a foam band of lace (broken by noise, swashing in and out, band-limited by the pixel footprint) lies in
//     the first `foamWidth` metres of depth. The shore is only as exact as the depth buffer. A polygon should
//     reach under the bank, as an edge above the bed is not softened. It needs the opaque scene copied once a
//     frame (like Water::refraction, about 0.6 ms at 1080p).
//   - Rivers have a flow: the surface's ripples are carried along the path at `flowSpeed` by the two-phase
//     flow-map method (two samples half a period apart, cross-faded by a triangle weight, so there is no seam).
//     A river's `look.waves` are usually empty: its detail is the flow ripples.
//   - It is seen from above only: from under its surface (the camera's y below `level`) it is not drawn.
//   - Reflections: one planar reflection pass is drawn per frame, at the level of the sea if that reflects, else
//     of the first body whose look reflects. Bodies at that level (within 1 mm) share it; bodies at other levels
//     reflect the sky only. Objects under a body are not tinted unless the body refracts (the bed in view is
//     tinted by the body's own surface either way).
//   - Up to 32 bodies draw (the first ones).
//
//     auto lake = g3d::WaterBody::lake({ {-20, -10}, {20, -10}, {25, 15}, {-15, 12} }, 0.5f);
//     scene->waterBodies.push_back(lake);
//     auto river = g3d::WaterBody::river({ {0, 0}, {10, 4}, {22, 3}, {35, 12} }, 4.f, 0.5f, 1.2f);
//     scene->waterBodies.push_back(river);
struct WaterBody {
    // The shape: a polygon (outline in x, z, with optional holes), or a river path (two or more points in x, z,
    // smoothed with a Catmull-Rom curve). A body with a path of two points or more is a river; otherwise the
    // outline is used.
    std::vector<Vector2> outline;
    std::vector<std::vector<Vector2>> holes;
    std::vector<Vector2> path;
    std::vector<float> width;       // river: the width in metres at each path point (one value for all; empty: 4)
    // Heights: `level` is the surface's height (the reference level for the shader and the reflection). A river
    // may fall along its path: `fall` metres of drop per metre of path, or `levels` (one absolute height per path
    // point, which wins). A river's reflection is drawn at `level`, so keep its drop to a few metres.
    float level = 0;
    float fall = 0;
    std::vector<float> levels;
    // River flow: the speed along the path in metres a second at the middle of the channel (the banks flow at
    // 65 %). With narrowFaster the speed at each point is flowSpeed times (mean width / width there), as a
    // constant discharge would give (limited to 0.4 to 2.5 times). `flowPeriod` is the flow map's cycle in seconds.
    float flowSpeed = 1;
    bool narrowFaster = false;
    float flowPeriod = 1;
    // The strength (slope) and the length in metres of a river's largest ripples.
    float ripple = 0.16f, rippleLength = 0.9f;
    Water look;                     // waves, colour, absorption, reflections, refraction... (lake() and river() fill it)
    // Metres of open water upwind for the weather's waves: 0 takes the body's own size (its longest extent).
    float fetch = 0;
    // The shore: depth in metres over which the surface fades in (0.3), the width of the foam band (0.4) and
    // the foam's strength, 0 to 1 (1; 0 turns the shore foam off).
    float edgeSoft = 0.3f, foamWidth = 0.4f, shoreFoam = 1;

    // A lake or pool: a calm look, a fetch of its own size, lit through to the bed.
    static WaterBody lake(std::vector<Vector2> outline, float level);
    // A river: path points, a constant width, the surface level at the start and the flow speed.
    static WaterBody river(std::vector<Vector2> path, float width, float level, float flowSpeed);

    bool isRiver() const { return path.size() >= 2; }
    // The reference surface height at (x, z): `level` for a polygon, the river's level at the closest path point.
    float levelAt(float x, float z) const;
    // The surface height at (x, z) at `seconds`, as the water look's waves give it (see Water::height), with the
    // weather worked in when `weather` is given.
    float height(float x, float z, double seconds, const Weather* weather = nullptr) const;
    // Whether the point lies inside the shape (a river: within half its width of the smoothed path).
    bool contains(float x, float z) const;
    // The x and z extent (min x, min z, max x, max z).
    Vector4 extent() const;
    // The speed of the flow along the path (m/s) at distance `arc` metres along it.
    float speedAt(float arc) const;
};

// The mesh a body is drawn with, for the CPU (the engine builds it once and draws it): positions (x, y, z) with
// y the offset from `level`; a normal slot holding (flow speed m/s, flow heading rad about +y from +x, river
// width m); texture coordinates (u across 0..1, v the arc length in metres along a river; polygons use
// (x, z)); 16-bit triangle indices. A polygon is triangulated by ear clipping with holes; a river is a strip
// along the smoothed path. Empty when the shape is degenerate.
struct WaterBodyMesh {
    std::vector<float> position, normal, uv;
    std::vector<unsigned short> index;
};
WaterBodyMesh waterBodyMesh(const WaterBody& body);
// A fingerprint of everything the mesh depends on (to cache it).
uint64_t waterBodyKey(const WaterBody& body);

// ------------------------------------------------------------------ wakes and ripples
// A boat's Kelvin wake, in Scene::wakes. The boat goes along `path` at `speed`; the wake is worked out from
// that path and the time alone (no simulation, so a frame is a pure function of t). It is drawn on the sea
// and on every water body, as a perturbation of the shading normal and as foam:
//   - Waves. The disturbance the hull made at each earlier moment spreads out as a ring with deep-water
//     dispersion (a wave of number k travels at half its phase speed, and the ring's phase is g T^2 / 4 D
//     at age T and distance D). The rings of the last `life` seconds are summed, 64 emission points spaced
//     closer the more recent they are, and only the parts of the sum that add up coherently survive: they
//     lie on the transverse and divergent crests of the Kelvin V, whose half angle is asin(1/3) = 19.47
//     degrees at any speed (a steady boat in deep water). Amplitude falls as 1 / sqrt(distance) behind the hull and
//     with age; the hull's size cuts the short waves (k beam over 2 and more).
//   - Foam: a white fringe along the first crests next to the hull and a bow wave, and a turbulent trail (the
//     propwash) of `trail` seconds that widens, breaks up and fades.
//   - Bow spray: wakeSpray() below is a particle emitter for it.
// The waves shift the shading only: the sea's mesh stays as it is (surfaceHeight() includes them, for riding).
// Up to four wakes draw (the first ones). They do not alter the water's height under a hull.
struct Wake {
    std::vector<Vector2> path;      // (x, z) waypoints; a Catmull-Rom curve through them (two at least)
    float speed = 4;                // metres a second along the path (constant)
    float start = 0;                // second at which the boat is at path[0]; it stops at the last point
    float length = 6, beam = 1.6f;  // the hull's size in metres
    float strength = 1;             // times the wave height, which follows the beam and the Froude number U / sqrt(g length)
    float life = 30;                // seconds the waves last (also how far back the sum reaches)
    float foam = 1;                 // 0..1: the foam (0 turns it off)
    float trail = 9;                // seconds the propwash stays white
    float level = 0;                // the water's mean height (for the spray, which starts on it)

    static constexpr int kSamples = 64;     // emission points in the sum

    // Metres of the curve, and the seconds the boat is under way.
    float arcLength() const;
    float duration() const { return speed > 0 ? arcLength() / speed : 0; }
    // The boat's place (x, z) and heading (a unit vector) at `seconds`; false before `start`. After the end it
    // stays at the last point, heading as at the end.
    bool at(double seconds, Vector2* position, Vector2* heading) const;
    // The emission table the shader reads: sample j was made at age T_j = life (j / 63)^2 (so the last is `life`
    // old), (x, z, heading x, heading z); a zero heading marks "no boat then". `out` has kSamples entries.
    void emissions(double seconds, Vector4* out) const;
    // The wave height (m) and slope (d height / dx, dz) at (x, z) at `seconds`: the CPU twin of the shader's sum.
    // `foot` is the metres a pixel covers there (0: every wave). Each call builds the table: for many points use
    // WakeField below.
    float height(float x, float z, double seconds, float foot = 0) const;
    Vector2 slope(float x, float z, double seconds, float foot = 0) const;
    // The wave amplitude scale, metres (from the beam, the Froude number and `strength`).
    float amplitude() const;
};
// A wake at one moment, with its table built: ask it many points.
class WakeField {
public:
    WakeField(const Wake& wake, double seconds);
    float height(float x, float z, float foot = 0) const { return sum(x, z, foot, nullptr); }
    Vector2 slope(float x, float z, float foot = 0) const { Vector2 g; sum(x, z, foot, &g); return g; }
    // The bow position and heading now (false before the start).
    bool boat(Vector2* position, Vector2* heading) const { *position = pos_; *heading = dir_; return present_; }
private:
    float sum(float x, float z, float foot, Vector2* slope) const;
    Vector4 table_[Wake::kSamples] = {};
    float life_ = 30, speed_ = 4, beam_ = 1.6f, amp_ = 0.1f;
    Vector2 pos_{ 0, 0 }, dir_{ 0, 0 };
    bool present_ = false;
};

// A stone dropped into the water, in Scene::ripples: at `time` seconds a dip of `strength` metres and about
// `size` metres across is made at (x, z), and a train of rings spreads out and fades. The rings are worked
// out from the capillary-gravity dispersion of water, omega^2 = g k + 7.3e-5 k^3: each wave number k travels at its
// own group speed (long waves faster, then a slowest 0.18 m/s near k = 210, then short waves faster again), so
// the train is a few crests at a speed of about 0.25 m/s for a 4 cm stone, with the long waves running ahead and the
// fine ones behind. The rings are round, and fade over `life` seconds. Up to 16 draw (the first ones).
// The shading's normal carries them; surfaceHeight() includes them.
struct Ripple {
    float x = 0, z = 0;             // where it fell
    float time = 0;                 // when (seconds)
    float strength = 0.08f;         // depth of the dip, metres
    float size = 0.04f;             // metres across (the stone)
    float life = 10;                // seconds until the rings have faded (they are a seventh of their size at life)
    float level = 0;                // the water's height there (for the splash)

    // Radial wave: the CPU twin of the shader. `age` seconds after the drop, at distance r metres: height (m) and the
    // slope along the radius. `foot` as in Wake::height.
    float height(float r, double age, float foot = 0) const;
    float slope(float r, double age, float foot = 0) const;
    // The same, from a point (x, z) and the time: height and (dx, dz) slope.
    float height(float px, float pz, double seconds, float foot) const;
    Vector2 slope(float px, float pz, double seconds, float foot) const;
    // The group speed (m/s) of waves of wave number k (rad/m): d omega / dk.
    static float groupSpeed(float k);
    // The wave number the train is made of most: where the dip's spectrum, weighted k^1.5, peaks (1.2 / size).
    float peakWave() const { return 1.22f / std::max(size, 0.005f); }
};

// ------------------------------------------------------------------ volumetric clouds
// A layer of real clouds, raymarched through a shell round a planet of Earth's size (centre 6 371 km
// below the world origin, so the layer curves away to the horizon and is seen from orbit as well as
// from the ground). They are lit by the sun with self-shadow, a silver lining and dark bases; they
// can be flown through (the view greys out inside and clears again outside); they cast moving
// shadows on whatever the sun reaches; and they fade into the haze with distance.
//
//     scene->clouds.enabled = true;
//     scene->clouds.base = 1500; scene->clouds.top = 3200;      // metres above y = 0
//     scene->clouds.cover = 0.45f;
//     scene->clouds.type = g3d::CloudType::Cumulus;
//
// With Clouds enabled the Sky's flat painted layer (Sky::clouds) is not drawn for that frame. The
// sky's own lighting of the scene (its diffuse and reflections) is still the cloud-free sky: the
// volumetric clouds do not darken image-based light yet.
//
// What is drawn: after the opaque objects and the sky; the march stops at the opaque depth (a
// hill can poke into a low cloud, clouds behind a mountain are hidden) and at the water plane
// (water behind a cloud is hidden by it). Transparent objects are not in the depth buffer, so one
// in front of a cloud is covered by it. Not drawn with an orthographic camera or from under the
// water. Distant clouds fade by their own aerial perspective (`haze`), not by the scene's fog.
// Everything is a pure function of the frame: nothing carries from one frame to the next, and the
// step noise is jittered per pixel (interleaved gradient noise), never accumulated over time.
//
// Cost: the march is the price of a frame. `quality` and `resolution` trade it: Film at half size
// is meant for about 40 ms at 1920x1080 on a mid-range GPU, Preview for a few.
struct Clouds {
    enum class Quality { Preview, Film, High };
    bool enabled = false;               // false: nothing here is read, no pixel changes
    // The layer: world y of its base and top. The planet's surface passes through y = 0, so these are
    // heights above the ground under the origin. The shape of the profile between them depends on `type`.
    float base = 1500, top = 3500;
    float cover = 0.5f;                 // 0..1: the share of the sky under cloud (looking up, about that share of pixels)
    CloudType type = CloudType::Cumulus;    // Cumulus: puffs with flat bases; Stratus: a thin sheet near the base;
                                            // Cumulonimbus: tall towers with an anvil; Deck: a flat-topped sea of cloud
                                            // (under a Weather the layer top is raised to at least 12 km for Cumulonimbus)
    float density = 1;                  // times the extinction: 0.5 thin and see-through, 2 dense and dark
    float detail = 0.5f;                // 0..1: how far fine noise erodes the edges (wispy, cauliflower)
    unsigned seed = 1;                  // another seed, another pattern of clouds
    float evolution = 1;                // metres a second the shape noise slides through the clouds, so they slowly change (0: only drift)
    Quality quality = Quality::Film;    // march steps: Preview 28, Film 96, High 160; light steps 4, 6, 10
    float resolution = 0;               // fraction of the frame the march renders at (then a depth-aware filter); 0: by quality (1/2, 1, 1)
    float haze = 1;                     // aerial perspective: 1 clouds fade to the horizon colour over ~80 km; 0 none
    bool shadows = true;                // a ground shadow map of the clouds, multiplied into the sun's light
    float shadowExtent = 8000;          // metres: the width of the area round the view the shadow map covers
    // The drift when no weather sets it: metres a second (x, z) and the film's rate.
    Vector2 drift{ 3, 1 };
    float fps = 30;
    // With Scene::weather enabled the cover and type come from it and the clouds drift with its wind
    // (Weather::cloudTravel, so a changing wind does not make them swim). true keeps these. This is
    // its own flag, not Sky::ownClouds, because the volumetric layer needs no Sky.
    bool ownWeather = false;
    // A point light inside the cloud: it lights the cloud from within, with this position, colour and strength
    // (0: none, the default). It is read by the march and does nothing else; Scene::lightning lights the
    // clouds by itself, from each strike's channel.
    struct Flash { Vector3 position{ 0, 0, 0 }; Color color{ 200, 210, 255, 255 }; float intensity = 0, radius = 1500; } flash;
    // Cumulonimbus (W5), with anvil 1: a few storm cells, each a broad lumpy column about as wide as it is tall that goes
    // up to the tropopause and flattens there into an anvil growing out of its top: a wedge thick against the tower, thinner
    // and higher downwind (the wind's direction under a Weather, else `drift`), fraying out along the wind and at its edges,
    // with a smooth fibrous face. Between the cells is ordinary cumulus (more of it with more `cover`). The cells are sparse
    // (about one in 40 x 40 km, at most 4 in 60 x 60 km) and come from the seed, or from `storms`. anvil 0: the older
    // dense field of sheared towers, kept for comparison.
    // towerVariety 1: every cell has its own seed-driven shape (radius, the anvil's length and width, its turn off the
    // wind, the lumps, its place in the shape noise), so neighbours are not clones. 0: all alike.
    // Rain shafts (Cumulonimbus with anvil 1): 0..1, the veil of rain that hangs under each storm cell from its base to the ground, slanted
    // downwind and streaky; 0: none (the default). With a Weather it follows the weather's rain intensity (at least this). The shell then
    // reaches down to the ground under the cells, which costs a few extra empty march steps.
    float shafts = 0;
    float anvil = 1;
    float towerVariety = 1;
    struct Storm {
        float x = 0, z = 0;             // centre in the pattern's frame (it drifts with the clouds: world = this + Clouds::travel)
        float radius = 5500;            // metres; the core is twice this wide
        float top = 0;                  // metres above the ground the tower reaches (<= 0: 0.93 of the way up the layer)
    };
    std::vector<Storm> storms;          // explicit cells; empty: from the seed and `cover`
    // Test and comparison switch. false: every ray starts at the same place in its step and the entry
    // into the cloud is not refined, so the steps show as rings (it exists to prove the jitter and the
    // refinement hide them).
    bool jitter = true;
    // Comparison switches, to prove what they hide. denoise false: the march's own per-pixel noise, white-ish
    // (interleaved gradient noise) instead of the blue-noise tile, and no bilateral filter on the result.
    // extruded true: the old shape, a 2D coverage map pushed straight up by a height profile (flat vertical
    // sides, hard surfaces), instead of the 3D shaping.
    bool denoise = true;
    bool extruded = false;
    bool nearestSlice = false;                  // comparison switch: sample the noise atlas at the nearest slice (bands in height)

    static constexpr float kPlanetRadius = 6371000;     // metres
    // The metres the clouds have moved (x, z) by `seconds` under this weather: with weather enabled
    // (and not ownWeather) the wind's run, else `drift` x seconds.
    Vector2 travel(const Weather& w, double seconds) const;
};

// ------------------------------------------------------------------ precipitation
// GPU rain, snow and sleet in a volume that follows the camera (Scene::precipitation), made the way
// the grass Field is: no drop exists on the CPU. Every drop is generated in the vertex shader from a
// hash of (tile, number), as an instanced quad, and its world position is a pure function of t.
//
// How the volume is laid out. Space is cut into fixed world tiles `kTile` metres across and `kCell`
// metres tall, indexed (ix, iy, iz). Drop k of a tile starts at a hashed spot on the tile's base and
// falls through the tile's height with period kCell / its speed, then wraps to the top, so a drop's
// place is a function of the tile, k and t alone. Moving the camera changes which tiles are drawn,
// never where a drop is. Wind slants every trajectory (the drop at height h above its tile's base
// sits `slant * h` upwind of its spot there), so the lines of fall are parallel and one top-down map
// can tell which of them a roof covers.
//
// Rain is drawn as a streak along its velocity, as long as speed x `exposure` (the shutter), thin
// and semi-transparent, lit by the scene's lights (a drop between the camera and a lamp or the low sun
// glints) and fogged by the scene's fog. Snow is a soft round flake that sways on a deterministic
// sinusoid and drifts with the wind. Sleet mixes the two. Far drops are thinned by rank (the first
// n of a tile are kept, so any prefix is an even sample) with the survivors keeping the weight;
// beyond `radius` a cheaper layer of sparse, wider and fainter streaks carries the rain to
// `farRadius` as curtains.
//
// Occlusion. Each frame the opaque scene is drawn into a depth map from above, looking along the
// fall, over `radius` round the camera. A drop that lies deeper than the map at its place has a
// roof, a tree or a bridge over it and is not drawn: under a porch there is no rain, beside it there
// is. Objects with Object3D::cullRadius that lie in the volume are kept even when out of view, as the
// map needs them. The map reaches 80 m above the camera. Transparent objects and grass do not block.
//
// Splashes (rain and the rain share of sleet): where a drop's period reaches the surface the map
// found (the ground, a roof) or the water level, a ring and a few droplets appear for about 0.2 s.
// Their position and time come from the same hash, so they are pure functions of t too. Snow
// makes none. The splash tiles reach `splashRadius`.
//
// Under Scene::weather (enabled) the kind, the intensity and the wind come from the weather
// (Weather::precipitation, Weather::wind); `ownWeather` and `ownWind` keep this struct's own. A weather
// whose kind is None draws nothing then. Rain follows the wind by Weather::kRainFollow, snow by
// kSnowFollow. Reflections in the water do not include precipitation; the water's own splashes
// are drawn in the main pass. fx::rain3d and fx::snow3d are unchanged.
struct Downfall {
    bool enabled = false;               // false: nothing is drawn and no pixel changes
    Precipitation kind = Precipitation::Rain;       // Rain, Snow or Sleet (None draws nothing)
    float intensity = 0.5f;             // 0..1: drizzle to downpour; drop count is proportional to it

    // Rain: terminal speeds m/s (bigger drops fall faster) and drop diameters in metres, many small
    // and few large. Snow: speeds and flake sizes. Densities are drops a cubic metre at intensity 1.
    Vector2 rainSpeed{ 6, 9 }, rainSize{ 0.0005f, 0.004f };
    Vector2 snowSpeed{ 0.5f, 1.5f }, snowSize{ 0.010f, 0.030f };
    float rainDensity = 90, snowDensity = 150;
    float flutter = 0.25f;              // metres: the sideways sway of a flake (a sinusoid, 1 to 2 rad/s)

    float radius = 30;                  // metres: the near volume, drawn drop by drop
    float farRadius = 150;              // metres: the far curtains reach this far
    bool farLayer = true;               // false: nothing beyond `radius`
    float farDensity = 0.15f;           // far drops a cubic metre (they are drawn wide and faint)
    float exposure = 1.f / 60.f;        // seconds: the streak is speed x exposure long. Comp::motionBlur blurs on top of it, so shorten it then
    float opacity = 1;                  // overall strength of the drops, 0..2
    bool occlusion = true;              // roofs, trees and bridges keep the rain off what is under them
    int occlusionSize = 1024;           // texels across the map (60 m / 1024 = 6 cm for the default radius)
    bool splashes = true;
    float splashRadius = 12;            // metres from the camera that splashes are drawn
    uint32_t seed = 1;                  // another seed, another pattern of drops
    float fps = 30;                     // the film's frames per second (the weather's, when it sets the wind)

    // The wind when no weather sets it: the direction in degrees about +y it blows towards (0 = +x, 90 = +z)
    // and its speed in m/s. A drop takes up Weather::kRainFollow (snow kSnowFollow) of it sideways.
    float windDirection = 17, windSpeed = 0;
    bool ownWeather = false;            // with Scene::weather: keep this struct's kind and intensity
    bool ownWind = false;               // with Scene::weather: keep this struct's wind
    bool worldFixed = true;             // comparison switch: false carries the drops along with the camera, to prove the test sees it

    static constexpr float kTile = 5, kCell = 10;           // the near layer's tile: metres across and tall
    static constexpr float kFarTile = 30, kFarCell = 60;    // the far layer's

    // The drift of a falling drop, metres sideways for each metre of fall (x, z): wind x follow / the
    // mean fall speed. Sleet falls at the rain's slant.
    Vector2 slant(bool snow) const;
    // How many drops a near tile holds at intensity 1 (kind decides the density), and a far tile.
    uint32_t dropsPerTile() const;
    uint32_t farDropsPerTile() const;
    // What the shader makes of drop k of tile (ix, iy, iz) at `seconds`, on the CPU (for tests). `faraway`
    // selects the far layer's tile. `rank` is (k + 0.5) / the tile's drops: below it a drop is kept.
    struct Drop {
        Vector3 position{ 0, 0, 0 }, velocity{ 0, 0, 0 };
        float diameter = 0, rank = 0;
        bool snow = false;
        float phase = 0;                // 0 at the top of its tile to 1 at the base: how far through its fall
    };
    Drop drop(int ix, int iy, int iz, uint32_t k, double seconds, bool faraway = false) const;
};
// `d` as drawn at this moment: with the weather enabled, its kind, intensity and wind from `w` (unless
// ownWeather / ownWind); the fps from the weather when it sets the wind. Else `d` itself.
Downfall inWeather(const Downfall& d, const Weather& w);
// Drops (near and far layers) and splashes the last rendered frame submitted in its main pass, after
// the tiles outside the view are dropped and far ones thinned (the shader thins further): for budgets.
long long precipitationDropsSubmitted();
long long precipitationSplashesSubmitted();

// ------------------------------------------------------------------ wet ground and settled snow
// What rain and snow do to surfaces (the lit shader, every Standard material with `weathering`). The state
// comes from the weather's history (Weather::ground: wetness 0..1 and the depth of snow in metres, accumulated
// as a pure function of t) unless pinned here. Nothing is drawn while both are zero (the shader keeps no extra
// path then), and nothing without Scene::weather enabled or a pin.
//
// Wetness w. A porous surface darkens (albedo x (1 - 0.45 w)) and gets smoother (roughness x (1 - 0.65 w)),
// and a water film adds a Fresnel reflection of the sky (F0 0.02). It dries from the top of slopes first:
// the wetness a surface keeps is clamp((w - 0.6 s) / (1 - 0.6 s)), s = 1 - normal.y. Where the ground is flat
// (normal.y above 0.96) and low in a world-space noise (puddleSize metres across, thresholded by w so that
// puddles grow as it rains and shrink from their edges as it dries) there is a puddle: a still film that
// mirrors, ringed by rain. With `reflections` on, flat wet ground and puddles also reflect what stands in the
// scene by a screen-space march of the opaque picture (a second draw of the plain meshes over the first, where
// the picture is the copy made before it), the sky where the ray leaves it or finds nothing.
//
// Rain on puddles and on open water (Scene::water) draws expanding rings: where Scene::precipitation rains
// (Downfall::kind Rain or Sleet, the weather's intensity) each square metre gets about `rippleRate` x
// intensity impacts a second, each a ring that spreads at 0.4 m/s for 1.1 s with a wavelength of 3.5 cm, its
// slope dying away, summed where they overlap. Impact places and times are hashed from the cell, slot, period
// and Downfall::seed, as a pure function of t, with the same hash as the splashes; the rings are a thinned
// stream of their own, not the splashes' impact points. Surfaces under a roof or a tree (the precipitation's
// occlusion map, drawn while it is raining) get no rain, so no puddle and no ring and no snow there.
//
// Snow depth d metres. Snow covers faces that point up: the mask is smoothstep(0.5, 0.9, normal.y) x
// smoothstep(noise - 0.15, noise + 0.15, d / 0.05) with a metre-scale noise, so cover creeps in as patches
// and whole by 5 cm; steep faces and the undersides of overhangs stay bare (normal.y alone: the roof of a
// shelter is told by the occlusion map). Snow is white and rough, a little blue in shadow (lit by the sky).
// Trees and grass are tinted by cover the same way, blades whitening at the tips first.
struct Ground {
    float wetness = -1;                 // pin 0..1 (-1: from the weather)
    float snowCover = -1;               // pin 0..1 of Weather::kSnowFull (-1: from the weather)
    bool ownWeather = false;            // with Scene::weather enabled: true keeps this struct's pins and ignores the weather's ground
    float puddles = 1;                  // 0 none, 1 as the wetness gives, 2 more of the ground under water
    float puddleSize = 3;               // metres across the puddle pattern
    float rippleRate = 6;               // rings a square metre a second at intensity 1 (at most 9.7)
    bool reflections = true;            // wet ground reflects the scene (a screen-space march), not only the sky
    bool snowOnGrass = true;            // fields and trees whiten with cover
    uint32_t seed = 1;                  // another pattern of puddles, snow patches and rings
    int debug = 0;                      // 1: puddles and water draw their ripple normals as colour (for tests)

    // The most rings a square metre can carry (cells x slots): rippleRate is clamped to it.
    static constexpr float kCell = 0.75f, kPeriod = 1.1f, kSpeed = 0.4f, kWavelength = 0.035f, kRingWidth = 0.045f;
    static constexpr int kSlots = 6;
    static constexpr float maxRate() { return (float)kSlots / (kPeriod * kCell * kCell); }
    // The rings the shader draws at ground point (x, z) at `seconds` for `rate` rings a square metre a second:
    // centre and age of each alive ring within kCell of the point's cell (for tests).
    struct Ring { Vector2 centre; float age; };
    // `precipSeed` is Downfall::seed: the rings hash it in with `seed` (the shader gets precipSeed * 2654435761 + seed).
    std::vector<Ring> rings(float x, float z, double seconds, float rate, uint32_t precipSeed = 0) const;
};
// The ground as drawn at `seconds`: this scene's pin, else the weather's (a pin or the accumulation, if the
// weather is enabled), else dry and bare.
Weather::GroundState groundOf(const struct Scene& scene, double seconds);

// ------------------------------------------------------------------ lightning
// Strikes of lightning (Scene::lightning), drawn in 3D: a bolt from rmtn/lightning.hpp (midpoint displacement,
// branches that thin and shorten with depth), as camera-facing ribbons with a thin hot core and a wide soft glow,
// added over the picture after the clouds, hidden by what stands in front of them.
//
//     scene->lightning.enabled = true;
//     scene->lightning.strikes.push_back({ 4.2f, { 300, 1800, -900 }, { 340, 0, -880 }, 7 });   // time (s), start, end, seed
//     scene->lightning.rate = 8;                    // and random strikes: 8 a minute over `area`
//
// A strike's time profile (bolt::profile): the leader grows the channel in `leaderTime` seconds, the return stroke
// holds full brightness for `minVisible` seconds (so a 24 or 30 fps film always catches it), then 2 to 4
// restrikes flicker over 0.2 to 0.5 s with the channel glowing between them, then the strike is gone. Every part
// is a pure function of the film time, so it holds under motion blur (sub-frame times are asked for).
//
// What a strike lights, while it is bright (`flash` of its profile, times `intensity`):
//   * the scene: a point light at the channel (blue-white, `light` strong, reaching `lightRange` metres, no shadows),
//     added to the scene's own lights for the frame (so the lit shader, the rain, the water and the wet ground all
//     take it; the last of 8 lights is replaced when the scene has 8). The strongest strike lights the scene,
//   * the clouds from inside (Clouds enabled, `cloudLight`): a glow round the channel's length in the cloud, lighting
//     each sample of the march by the optical depth between it and the channel, so a cloud shines from within
//     and its edges glow. The strongest strike lights the clouds.
// A strike that stays in the cloud (its end above groundY + 50 m) is drawn as a faint wide glow only: its channel is
// inside the cloud and the cloud's own glow is the picture.
//
// Where strikes come from. `strikes` are explicit: times in seconds, with the start (in the cloud) and end
// (the ground, or inside the cloud). Strikes from the rate: time is cut in half-second slots; slot k holds one
// strike with probability rate x 0.5 / 60, at a time and place hashed from (seed, k), so the same time always
// gives the same strikes and the long-run count follows the rate. A strike picked from the rate starts
// inside the cloud base layer over `area` (a disc), and `groundShare` of them reach the ground. The rate is
// Lightning::rate, or Weather::lightningRate when that is negative.
//
// With Clouds enabled and `followClouds`, a strike's authored position is where it lies in the cloud's own frame: the
// position drifts with the clouds' wind run at the strike's time (Clouds::travel), so a strike keeps under its
// tower as it moves across the sky.
struct Lightning {
    bool enabled = false;
    struct Strike {
        float time = 0;                         // seconds of film time at which the leader starts
        Vector3 start{ 0, 1800, 0 }, end{ 0, 0, 0 };
        uint32_t seed = 1;                      // the bolt's shape and timing
        float intensity = 1;                    // times Lightning::intensity
    };
    std::vector<Strike> strikes;                // explicit strikes
    float rate = -1;                            // strikes a minute from `area`; < 0 takes Weather::lightningRate (0 without a weather)
    Vector2 area{ 0, 0 };                       // centre (x, z) and
    float radius = 3000;                        // radius of the disc the rate's strikes fall in
    float cloudBase = -1;                       // y of the cloud base the rate's strikes start under or in; < 0: Clouds::base if enabled, else 1500
    float groundY = 0;                          // y of the ground the rate's strikes end on
    float groundShare = 0.45f;                  // share of the rate's strikes that reach the ground
    uint32_t seed = 1;                          // another seed, other times and places for the rate's strikes
    bool followClouds = true;
    Color color{ 188, 204, 255, 255 };          // the glow's and the light's colour (the core is whiter)
    float intensity = 1;                        // brightness of the channel
    float width = 0.8f;                         // metres across the main channel's hot core
    float minPixels = 1.4f;                     // never thinner than this on screen (px at 1080 lines)
    float glow = 1;                             // the glow's width and strength, times
    float light = 1;                            // strength of the point light (0: none)
    float lightRange = 3500;                    // metres: where it fades to nothing
    float cloudLight = 1;                       // strength of the clouds' inner glow (0: none)
    float leaderTime = 0.035f;                  // seconds
    float minVisible = 0.07f;                   // seconds the return stroke holds full brightness
    bolt::Shape shape;
    float fps = 30;                             // the film's frames per second (the weather's when it sets the clouds)
};

// One strike alive at a moment: its profile there.
struct LightningActive {
    Lightning::Strike strike;                   // positions after followClouds
    float tau = 0;                              // seconds since it began
    bolt::Profile profile;
    bool toGround = false;
};

// A strike's place for thunder: where it lies, how far from the camera, and when the sound arrives.
struct Thunder {
    double strikeTime = 0;                      // seconds
    double arrives = 0;                         // strikeTime + distance / 343
    float distance = 0;                         // metres from the camera (to the nearest of the channel's ends and middle)
    Vector3 position{ 0, 0, 0 };                // that nearest point
    bool toGround = false;
    uint32_t seed = 1;
};
struct Scene;
// The strikes that begin in [from, to) seconds of film time (explicit and from the rate), with the weather and the
// clouds of the scene as they are now. Sorted by time. The weather's lightning rate is the scene's current one:
// a rate that changes over the film needs this called at each frame (thunderCues does).
std::vector<Lightning::Strike> lightningStrikes(const Scene& scene, double from, double to);
// The strikes alive at `seconds` with their time profiles (what render() draws).
std::vector<LightningActive> lightningAt(const Scene& scene, double seconds);
// The thunder of a film: every strike of `frames` frames at `fps`, with its distance from the camera and the time
// its sound arrives (thunderDelay), sorted by arrival. It steps the scene's `up` and camera `up` frame by frame, as
// a render would, so a weather animated in `up` sets the rate where it is; it leaves the scene at its last frame.
// Place the clips with the audio track: for each cue add a node starting at round(arrives * fps) with
// media::audio(node, "thunder.wav", { .volume = ... }) (see docs/weather.md, "Thunder on the soundtrack").
std::vector<Thunder> thunderCues(Scene& scene, int frames, float fps);

// ------------------------------------------------------------------ light in the air
// Sun shafts in the scene's fog (Scene::volumetric). Off by default: with quality Off nothing is drawn and the
// picture is the analytic fog's, bit for bit.
//
// The fog is the scene's own (fogMode, fogDensity, fogHeight, fogFalloff, or the weather's haze), so the
// medium is the one the surfaces are already fogged by; nothing is fogged twice. Each view ray is marched, at
// reduced resolution, from the eye to the opaque surface under the pixel (or to where the fog is thick), and at
// every step the fog's in-scatter is lit by the sun: the fog colour (what fully lit fog looks like) times
//   ambient + (1 - ambient) * sunlit * phase,
// where `sunlit` is the sun's shadow map (the directional light's tile) times the clouds' ground shadow, and
// `phase` is a Henyey-Greenstein lobe, compressed so that the shaft toward the sun is bright and the one away
// from it dimmer, but the mean over all directions is the plain fog colour. With anisotropy 0 and no
// shadow the sum is exactly the analytic fog (the march only replaces it by its shaded version). The picture
// is a pure function of the frame: no temporal accumulation; the march's per-pixel jitter (interleaved gradient
// noise on the low-resolution pixel grid, the same every frame) is removed by a depth-aware spatial filter.
// What the opaque depth buffer does not hold (transparent surfaces, the sea) is marched as if it were not there,
// to the sea's plane for the sea.
struct Volumetric {
    enum class Quality { Off, Low, Medium, High };
    Quality quality = Quality::Off;     // Off draws nothing. The march's steps and size (a share of the picture's width and height): Low 16, 0.35; Medium 32, 0.5; High 48, 0.75
    float density = 0;                  // > 0: this exponential fog (per metre) replaces the scene's and the weather's; 0 keeps them
    float heightFalloff = 0;            // > 0 replaces the fog's height falloff: density * exp(-falloff * (y - heightBase)) (per metre)
    float heightBase = 0;               // the height at which the density is `density` (used with heightFalloff)
    float anisotropy = 0.7f;            // Henyey-Greenstein g, 0..0.95: 0 scatters evenly, 0.7 favours the way to the sun
    float ambient = 0.35f;              // the share of the fog's brightness that does not need the sun (fog in shadow)
    bool scatterLights = false;         // spot and point lights light the fog as well (cones in the mist); their shadows count
    bool enabled() const { return quality != Quality::Off; }
};

// Distant things fade toward the sky's horizon colour and blue-shift with distance: Rayleigh-like extinction
// (blue goes first) with the haze's density falling off exponentially with altitude (scaleHeight), so a ridge
// 30 km off is paler and bluer, and from high up the same distance is hazier less. The colour it fades to is the sky's horizon
// colour in that direction (the one the sea's haze and the fog use), so there is no seam where land meets sky.
// Applies to everything in the opaque depth buffer (terrain, buildings, trees); the clouds keep their own haze
// (Clouds::haze) and the spectrum sea its own (it uses the same colour). Off by default.
struct AerialPerspective {
    bool enabled = false;
    float scaleHeight = 2500;           // metres: the haze's density falls as exp(-altitude / scaleHeight)
    float strength = 1;                 // 1: the green light is cut to 1/e (37%) over 30 km of sea-level air (red 1.3 times further, blue 1.7 times less); 0 none
};

// ------------------------------------------------------------------ scenes
struct Scene {
    Camera camera;
    std::vector<std::unique_ptr<Object3D>> objects;
    std::vector<Light> lights;      // up to 8
    Color skyColor{ 200, 214, 255, 255 }, groundColor{ 60, 52, 48, 255 };
    float ambient = 0.35f;          // hemisphere light from sky (above) and ground (below)
    // Image-based lighting from an equirectangular image (PNG/JPG, or HDR for linear
    // radiance). It replaces the hemisphere: diffuse from its blurred mips, reflections
    // sharp to blurry with roughness. It repeats round the azimuth and is clamped over the
    // poles, so straight up shows the top row only (the same file used as a material map
    // keeps repeating).
    std::string environment;
    // A skybox given as cube faces instead of one equirectangular image. Either six files in
    // the order +x, -x, +y, -y, +z, -z, or ONE file holding a horizontal cross (a 4x3 grid of
    // square faces: row 0 has +y in column 1; row 1 is -x, +z, +x, -z; row 2 has -y in column 1).
    // Skybox packs are labelled for left-handed engines (OpenGL cube maps); raymotion is
    // right-handed. So the +x and -x faces swap sides, as in three.js: the face labelled +x
    // appears in the world -x direction and the face labelled -x at world +x, while +y, -y,
    // +z and -z stay where they are. This keeps every face unmirrored on screen (a camera with
    // +y up sees its image left on the screen's left and top at the top for the four side
    // faces) and the sky continuous across the seams. +y's image bottom edge meets +z, and
    // -y's image top edge meets +z. Faces must be square and equal in size and kind
    // (8-bit PNG/JPG, or all HDR for linear radiance). They are converted once on the CPU to
    // an equirectangular image (4 x face width, at most 4096) with bilinear sampling inside
    // each face, which image-based lighting, showEnvironment, environmentRotation and
    // fogFromSky then use unchanged. Any other count, a non-square or mismatched face, or a
    // missing file throws std::runtime_error. Precedence: `environment` (a file) wins over
    // `environmentFaces`, which wins over `environmentGenerator`.
    std::vector<std::string> environmentFaces;
    std::function<Image()> environmentGenerator;    // used when no file or faces are set; called once
    float environmentIntensity = 1;
    float environmentRotation = 0;  // degrees about +y
    bool showEnvironment = false;   // also draw it as the backdrop
    Color background = BLANK;       // BLANK: the 2D content beneath shows through
    Color fogColor = BLACK;
    float fogNear = 0, fogFar = 0;  // linear fog by view distance; fogFar <= fogNear disables
    FogMode fogMode = FogMode::Linear;
    float fogDensity = 0;           // exponential fogs; 0 disables
    float fogHeight = 0, fogFalloff = 0;    // height fog: density * exp(-falloff * (y - fogHeight))
    bool fogFromSky = false;        // fog takes the sky's (or environment's) colour in each direction
    Sky sky;                        // a procedural sky (sky.enabled); replaces `environment`
    Water water;                    // a water surface (water.enabled)
    std::vector<WaterBody> waterBodies;     // lakes, pools and rivers, drawn with the same shader (see WaterBody)
    std::vector<Wake> wakes;                // boats' Kelvin wakes on the sea and the bodies (see Wake)
    std::vector<Ripple> ripples;            // stones dropped in: rings on the sea and the bodies (see Ripple)
    Weather weather;               // one wind, clouds, haze... for everything above (weather.enabled)
    Clouds clouds;                  // volumetric clouds (clouds.enabled), drawn after the opaque objects
    Downfall precipitation;         // GPU rain, snow or sleet round the camera (precipitation.enabled)
    Ground ground;                  // wet ground, puddles, ripples and snow cover (needs weather.enabled, or a pin here)
    Lightning lightning;            // bolts, the flash they make, and the clouds lit from inside (lightning.enabled)
    Volumetric volumetric;          // sun shafts in the fog (volumetric.quality, Off by default)
    AerialPerspective aerial;       // distant land fades blue (aerial.enabled, off by default)
    // With weather enabled the haze sets the fog: exponential, density from the haze, taking the
    // sky's colour (fogMode, fogDensity and fogFromSky are replaced; fogHeight and fogFalloff stay).
    // true keeps the scene's own fog.
    bool ownFog = false;
    int shadowMapSize = 1024;       // per atlas tile (shrunk if the atlas would pass 8192)
    // Culling (Object3D::cullRadius) keeps an out-of-view subtree whose shadow can fall into
    // view. It assumes shadows land no lower than this y, so it needs only follow the light
    // down to here. Scenes whose shadowed lights are not all directional are never culled.
    float cullFloor = 0;

    // Ink lines found in screen space from depth, normals and object boundaries: outer
    // silhouettes, inner contours where a surface passes in front of itself, and creases.
    // Constant width in composition px for any mesh; opaque objects only. Drawn before
    // depth of field, so they blur with what they outline.
    struct Lines {
        bool enabled = false;
        Color color{ 22, 20, 30, 255 };
        float width = 2;            // composition px (times each material's lineWidth)
        float creaseAngle = 40;     // degrees between surface normals that make a crease line
        float depthThreshold = 0.04f;   // relative depth jump that makes a contour line
        float wobble = 0;           // hand-drawn displacement, composition px (0 = straight)
        float wobbleScale = 60;     // composition px per wobble wave
        int boilEvery = 2;          // frames per wobble pattern ("on twos"); 0 = never changes
    } lines;
    std::function<void(int t, Scene& self)> up;

    Object3D* add(GeometryPtr g, Material m = {});
    Object3D* add(ModelPtr m);
    Object3D* add(fx::Emitter3D e);
    Object3D* add(fx::Emitter3D e, GeometryPtr g, Material m = {});
    Object3D* add(std::shared_ptr<Instances> i, Material m = {});
    Object3D* add(std::shared_ptr<Field> f, Material m = {});
    Object3D* add(std::shared_ptr<Splats> s);
    Object3D* add(std::shared_ptr<Surface> s);
    Object3D* add(std::shared_ptr<g3d::Lines> l);    // strokes and arrows (not `lines`, the ink outlines)
    Object3D* add(Object3D* object); // takes ownership; supports procedural physics objects
    Object3D* add();
    Object3D* find(const std::string& name);         // depth-first, nullptr when absent
};
using ScenePtr = std::shared_ptr<Scene>;

// ------------------------------------------------------------------ weather (weather.hpp)
// What each system draws when Scene::weather is enabled: its own struct with the weather's
// values worked in (unless its `own...` flag is set). render() calls these every frame on
// copies, never on your structs; they are public so a test or a boat can read the same numbers.
//
// The wind, in each system's own units (speed s in m/s, direction d the unit vector (x, z)):
//   Field         windDirection = d (turned into the field's object space by `world`), sway =
//                 0.015 s (at most 0.6), gust = 0.7 * wind.gust, gustSpeed and gustScale as given.
//   ForestSpec    windDirection = d, wind = s / 8 (at least 0.05), gust = 1.2 * wind.gust,
//                 gustSpeed and gustScale as given (flora::inWeather in flora.hpp).
//   Water         every wave's direction turned by (wind direction - the waves' mean heading), so
//                 they head downwind with their spread kept; their significant height and longest
//                 wavelength become the fetch-limited JONSWAP values for the wind speed and
//                 Water::fetch (Weather::waveHeight, wavePeakLength), shape kept, steepness 2a/L
//                 clamped to 0.12.
//   Emitter3D     a sideways velocity of windFollow * s / fps along d, in the emitter's own space.
//                 With windFollow > 0 the emitter's own sideways direction is dropped, so the wind
//                 alone sets the slant of rain and snow (Weather::kRainFollow, kSnowFollow).
//   Sky           clouds drift along d: cloudShift = -d * 0.0012 * (metres of travel), since the
//                 sky shader moves a cloud against the sign of its offset.
// The sky and fog follow the cover and haze; see Weather and Sky::ownClouds, ownLight, Scene::ownFog.
Field inWeather(const Field& f, const Weather& w, const Matrix* world = nullptr);
Water inWeather(const Water& water, const Weather& w);
Sky inWeather(const Sky& sky, const Weather& w, double frame);
fx::Emitter3D inWeather(const fx::Emitter3D& e, const Weather& w, const Matrix& world);
// The water as drawn at this moment: scene.water, with the weather worked in if it is enabled.
// Its height() and slope() are the surface the shader draws (for a boat to ride).
Water waterOf(const Scene& scene);
// The look of a water body as drawn: body.look at body.level with the fetch (its own size when 0), and the
// weather worked in if it is enabled. Its height() and slope() are the waves the shader draws on the body.
Water waterOf(const Scene& scene, const WaterBody& body);
// The surface a boat floats on at (x, z) at `seconds`, as drawn: the first water body that contains the point,
// else the sea (else a flat sheet at y = 0), with the waves, every wake and every ripple added. (The shader's
// mesh does not carry the wakes and ripples, only its shading does; this is the height a floating thing should
// follow.) `seconds` is on the water's clock: frameTime(t) / water.fps.
float surfaceHeight(const Scene& scene, float x, float z, double seconds);
// Its slope (d height / dx, dz).
Vector2 surfaceSlope(const Scene& scene, float x, float z, double seconds);

// How a floating object sits on the waves, for ride().
struct Ride {
    float length = 4, beam = 1.5f;      // the hull's footprint in metres (along its heading, across it)
    float draft = 0;                    // metres the object's origin lies below the mean surface under it
    float heading = NAN;                // degrees about +y (0 = +x, 90 = +z) the hull points to; NaN: from the
                                        // object's rotation.y as set before (a yaw: its +z shows to (sin y, cos y))
    float tilt = 1;                     // 0..1: how much of the surface's pitch and roll the hull takes
    int along = 5, across = 3;          // sample grid over the footprint (2..9 each)
    int skipWake = -1;                  // >= 0: leave Scene::wakes[skipWake] out of the surface (a boat does not ride its own bow wave)
};
// Sets `object`'s height, pitch and roll (and keeps its x, z) from the surface under its footprint: the height is the
// mean of an along x across grid of samples, and the tilt is the plane fitted through them, so a long hull does
// not jitter on waves shorter than itself. It works for the sea, a water body, and the wakes and ripples on them
// (surfaceHeight). The object's +z is its bow; call it from an `up` callback with
// `seconds = frameTime(t) / fps`. Returns the heading (a unit vector in x, z) it used.
Vector2 ride(Object3D& object, const Scene& scene, double seconds, const Ride& how = {});

// Bow spray for wake `w`: a fx::Emitter3D in world space (add it at the origin: scene.add(g3d::wakeSpray(w, fps))).
// Droplets leave the bow for as long as the boat is under way, more of them and higher the faster it goes, and
// fall back on the water. `fps` is the film's.
fx::Emitter3D wakeSpray(const Wake& w, float fps = 30);
// A small splash for ripple `r` (a burst of droplets thrown up where the stone fell).
fx::Emitter3D splash(const Ripple& r, float fps = 30);

// The rainbow (Weather::rainbow), as it is drawn at `seconds`. A pass over the picture after the clouds, in the
// sky and over geometry farther than the rain's distance, hidden behind nearer things (the depth buffer; the sea's
// own plane counts), not drawn in the water's reflection. `strength` is 0 when there is no bow: Mode::Off, no
// rain (Auto), the sun behind cloud or down, or above 42 degrees (Auto). The bow's centre is the point opposite
// the sun, `antiSolar`; its primary arc is at 42 degrees from it (red outside), the secondary at 51 (reversed,
// a quarter as bright), a dark band between them (Alexander's) and a lighter sky inside the primary. Light of each
// wavelength is spread over the bow by the geometry of a raindrop (Descartes' angle for water's index at that
// wavelength, and a smooth falloff on the shadow side), then turned to colour; so the bow is a soft spectral arc.
struct RainbowFrame {
    float strength = 0;
    Vector3 sun{ 0, 1, 0 }, antiSolar{ 0, -1, 0 };   // unit vectors: towards the sun, and the bow's centre
    float elevation = 90;                             // the sun's height above the horizon, degrees
};
RainbowFrame rainbowOf(const Scene& scene, double seconds);
// The angle (degrees) from the anti-solar point of the primary bow at wavelength `nm` (red 42.3, violet 40.6) and of
// the secondary (red 50.4, violet 53.7), from the drop's index of refraction at that wavelength (Descartes).
float rainbowAngle(float nm, int order);

// The storm cells of `c` whose centres lie in the box [x0, x1] x [z0, z1] of the pattern's frame (world minus
// Clouds::travel): `c.storms` if it lists any, else one possible cell in each 40 km square of a jittered grid (present
// with a probability that grows with the cover, 0.3 to 0.8), each with its own radius of 4.5 to 7 km. Pure in the seed.
std::vector<Clouds::Storm> stormCells(const Clouds& c, float x0, float z0, float x1, float z1);

// The clouds as drawn this moment: `c` with the weather's cover and type worked in (unless
// c.ownWeather or the weather is off). Their drift is c.travel(w, seconds).
Clouds inWeather(const Clouds& c, const Weather& w);

// A procedural studio environment for Scene::environmentGenerator: a vertical gradient
// (top, horizon, bottom) with `panels` soft light boxes above the horizon that shiny
// objects reflect.
std::function<Image()> studioEnvironment(Color top, Color horizon, Color bottom, int panels = 3);

// Renders `scene` at frame t into the current canvas target, inside `viewport` (local to
// ctx; empty = the whole composition), composited with ctx opacity. `camera` overrides
// scene.camera, e.g. several views of one scene. Outside presentFrame it does nothing.
void render(Scene& scene, int t, const Ctx& c, Rectangle viewport = {}, Camera* camera = nullptr);

// Inside an `up` callback: the camera of the view being drawn, after its own `up` (so tiers
// by distance, billboards and the like can follow it). A default camera outside a draw.
const Camera& drawingCamera();
// Inside an `up` callback: the scene being drawn (its lights are final for this frame), or null
// outside a draw.
const Scene* drawingScene();
// Inside an `up` callback: the height in composition px of the view being drawn (the viewport's,
// or the whole composition's), or 0 outside a draw. With the camera's fov it gives how many
// pixels something of a known size and distance covers.
float drawingViewportHeight();

// The camera as render() sees it at frame t: `camera` with its `up` applied (to a copy).
Camera cameraAt(const Camera& camera, int t);

// Where a world point lands, in composition px, through `camera` drawing into `viewport`
// (the rectangle given to render; pass the composition's full size when that was empty).
// False when the point is behind the camera. For 2D labels that follow 3D objects (they
// always face the viewer):
//     const g3d::Camera cam = g3d::cameraAt(scene->camera, t);
//     Vector2 at;
//     if (g3d::project(cam, { 0, 80, 0 }, { 0, 0, 1920, 1080 }, at)) dr::text(..., at, ...);
bool project(const Camera& camera, Vector3 world, Rectangle viewport, Vector2& out);

// Flying into a Surface until it is the frame. fillCamera returns `base` moved square in front
// of the surface (object `o`, anywhere in `scene`; it and its parents are posed for frame t by
// running their `up`) at the distance where the surface just fills a frame of `aspect` (width / height),
// centred, its top up; `margin` > 0 leaves that fraction of the frame around it. Blend toward
// it over a push-in; on the frame the camera reaches it, the scene shows the surface pixel for
// pixel as fillCtx would draw it directly, so a cut to the 2D layer there is invisible:
//     Ctx d = g3d::fillCtx(*ui, 1920, 1080);       // design px -> composition px
//     ui->draw(d, t - ui->start);
Camera fillCamera(const Camera& base, Scene& scene, Object3D& o, int t, float aspect, float margin = 0);
Ctx fillCtx(const Surface& s, float width, float height, float margin = 0);

// A camera part way from `a` to `b` (u = 0..1): position, target, up, fov, planes and ortho
// height blended; depth of field and `up` from `a`. For push-ins (ease u yourself).
Camera mix(const Camera& a, const Camera& b, float u);

// Euler angles for Object3D::rotation (degrees) that turn the object's front (+z) to
// `forward` and its top (+y) as near to `up` as it can.
Vector3 lookRotation(Vector3 forward, Vector3 up = { 0, 1, 0 });

// A TNode::fn that renders `scene` (the node's local frame drives the animation).
inline std::function<void(const Ctx&, int)> draw(ScenePtr scene, Rectangle viewport = {},
                                                 std::shared_ptr<Camera> camera = nullptr) {
    return [scene, viewport, camera](const Ctx& c, int t) { render(*scene, t, c, viewport, camera.get()); };
}

} // namespace g3d
} // namespace rmtn
