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
private:
    std::function<Mesh()> make_;
    Mesh mesh_{};
    bool loaded_ = false;
};
using GeometryPtr = std::shared_ptr<Geometry>;

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
private:
    void ensure();
    std::string path_;
    Model model_{};
    ModelAnimation* anims_ = nullptr;
    int animCount_ = 0;
    bool loaded_ = false;
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

    // Ink lines, when the scene's lines are on (Scene::lines): colour (BLANK = the scene's)
    // and a multiplier on the scene's line width (0 = no lines around this object).
    Color lineColor = BLANK;
    float lineWidth = 1;
};

// ------------------------------------------------------------------ objects
struct Object3D {
    std::string name;
    Vector3 position{ 0, 0, 0 };
    Vector3 rotation{ 0, 0, 0 };    // Euler XYZ, degrees
    Vector3 scale{ 1, 1, 1 };
    bool visible = true;            // hides the subtree too

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

    std::function<void(int t, Object3D& self)> up;      // animate from the local frame
    std::vector<std::unique_ptr<Object3D>> children;

    Object3D* add(GeometryPtr g, Material m = {});
    Object3D* add(ModelPtr m);
    Object3D* add(fx::Emitter3D e);                    // particles (billboards, streaks, quads)
    Object3D* add(fx::Emitter3D e, GeometryPtr g, Material m = {});   // mesh particles
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
    struct DepthOfField { float focusDistance = 5, aperture = 0, maxBlur = 12; } dof;
    std::function<void(int t, Camera& self)> up;
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
    int shadowMapSize = 1024;       // per atlas tile (shrunk if the atlas would pass 8192)

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

// A TNode::fn that renders `scene` (the node's local frame drives the animation).
inline std::function<void(const Ctx&, int)> draw(ScenePtr scene, Rectangle viewport = {},
                                                 std::shared_ptr<Camera> camera = nullptr) {
    return [scene, viewport, camera](const Ctx& c, int t) { render(*scene, t, c, viewport, camera.get()); };
}

} // namespace g3d
} // namespace rmtn
