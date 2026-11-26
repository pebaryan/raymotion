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
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "timeline.hpp"
#include "particles.hpp"
#include "weather.hpp"

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
    // Whether a world point lies under the surface at `seconds`.
    bool submerged(Vector3 p, double seconds) const { return enabled && p.y < height(p.x, p.z, seconds); }
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
    // Hook for lightning (a later release): a flash inside the cloud lights it from within, as a point
    // light of this position, colour and strength (0: none). It is read by the march and does nothing else.
    struct Flash { Vector3 position{ 0, 0, 0 }; Color color{ 200, 210, 255, 255 }; float intensity = 0, radius = 1500; } flash;
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
    Weather weather;                // one wind, clouds, haze... for everything above (weather.enabled)
    Clouds clouds;                  // volumetric clouds (clouds.enabled), drawn after the opaque objects
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
