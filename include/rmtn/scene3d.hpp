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
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "timeline.hpp"
#include "particles.hpp"

namespace rmtn {
namespace splat { struct Cloud; }
namespace g3d {

// ------------------------------------------------------------------ geometry and models
// A mesh description; the GPU mesh is generated on first use and freed by shutdownGraphics.
class Geometry : public std::enable_shared_from_this<Geometry> {
public:
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
    static std::shared_ptr<Geometry> knot(float radius, float tube, int tubularSegs = 256, int radialSegs = 24,
                                          int p = 2, int q = 3);
    const Mesh& mesh();
    // Reshapes the mesh: `edit` changes its CPU copy (vertices, normals; same counts) before
    // the next draw, which re-uploads them. Safe to call from `up` callbacks every frame
    // (morphs, growing shapes); the latest edit wins.
    void update(std::function<void(Mesh&)> edit);
private:
    std::function<Mesh()> make_;
    std::function<void(Mesh&)> pending_;
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
    Color emissive = BLACK;         // added light, unaffected by lighting
    float opacity = 1.0f;           // < 1: drawn after opaque objects, back to front
    bool unlit = false;             // flat base colour (no lights, shadows or fog)
    bool doubleSided = false;
    bool castShadow = true, receiveShadow = true;
    std::string map;                // optional base-colour texture file
    DynamicTexturePtr dynamicMap;   // or a texture made per frame (wins over `map`); its alpha
                                    // multiplies opacity: give a see-through overlay opacity
                                    // just under 1 so it draws with the transparent objects

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
};

// An object's instances: `up` refills `list` each frame from t (pure, like any `up`). They
// are drawn in the object's space with its `material` (opaque only; the colour comes from the
// mesh's vertex colours, multiplied by the material's). Each one is culled on its own (see
// Object3D::cullRadius), shadow footprint included, so only what can show is submitted.
struct Instances {
    std::shared_ptr<AnimatedMesh> mesh;
    std::vector<Instance> list;
};

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
    int animation = -1;             // model: animation index to pose, -1 = bind pose
    float animationFrame = 0;       // model: keyframe (fractional ok)
    // Particles born in this object's space (fx::Emitter3D). With a `geometry` too, each
    // particle is an instance of it in `material` (tinted by the particle's colour), and the
    // geometry itself is not drawn.
    std::shared_ptr<const fx::Emitter3D> emitter;
    std::shared_ptr<Instances> instances;               // many copies of one baked mesh
    std::shared_ptr<Splats> splats;                     // a Gaussian splat cloud

    std::function<void(int t, Object3D& self)> up;      // animate from the local frame
    std::vector<std::unique_ptr<Object3D>> children;

    Object3D* add(GeometryPtr g, Material m = {});
    Object3D* add(ModelPtr m);
    Object3D* add(fx::Emitter3D e);                    // particles (billboards, streaks, quads)
    Object3D* add(fx::Emitter3D e, GeometryPtr g, Material m = {});   // mesh particles
    Object3D* add(std::shared_ptr<Instances> i, Material m = {});      // instanced bodies
    Object3D* add(std::shared_ptr<Splats> s);                          // a splat cloud
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

// ------------------------------------------------------------------ scenes
struct Scene {
    Camera camera;
    std::vector<std::unique_ptr<Object3D>> objects;
    std::vector<Light> lights;      // up to 8
    Color skyColor{ 200, 214, 255, 255 }, groundColor{ 60, 52, 48, 255 };
    float ambient = 0.35f;          // hemisphere light from sky (above) and ground (below)
    // Image-based lighting from an equirectangular image (PNG/JPG, or HDR for linear
    // radiance). It replaces the hemisphere: diffuse from its blurred mips, reflections
    // sharp to blurry with roughness.
    std::string environment;
    std::function<Image()> environmentGenerator;    // used when `environment` is empty; called once
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
    Object3D* add(std::shared_ptr<Splats> s);
    Object3D* add(Object3D* object); // takes ownership; supports procedural physics objects
    Object3D* add();
    Object3D* find(const std::string& name);         // depth-first, nullptr when absent
};
using ScenePtr = std::shared_ptr<Scene>;

// A procedural studio environment for Scene::environmentGenerator: a vertical gradient
// (top, horizon, bottom) with `panels` soft light boxes above the horizon that shiny
// objects reflect.
std::function<Image()> studioEnvironment(Color top, Color horizon, Color bottom, int panels = 3);

// Renders `scene` at frame t into the current canvas target, inside `viewport` (local to
// ctx; empty = the whole composition), composited with ctx opacity. `camera` overrides
// scene.camera, e.g. several views of one scene. Outside presentFrame it does nothing.
void render(Scene& scene, int t, const Ctx& c, Rectangle viewport = {}, Camera* camera = nullptr);

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
