// scene3d.cpp -- GPU 3D: lazy GPU resources, the lit/shadow shaders and scene rendering.
#include "rmtn/scene3d.hpp"
#include "rmtn/render.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

// Defined and exported by raylib 6.2 (rmodels.c) but missing from its public header:
// poses the bones without CPU skinning, for skinning in our vertex shader.
extern "C" void UpdateModelAnimationBones(Model model, ModelAnimation anim, float frame);

namespace rmtn {
namespace g3d {
namespace {

constexpr int MAX_LIGHTS = 8;
constexpr int MAX_BONES = 128;
constexpr int SHADOW_SLOT = 14;          // texture unit for the shadow map (material maps use 0..11)

// ---- shaders ---------------------------------------------------------------------------
// Attribute and uniform names are raylib's defaults, so DrawMesh binds them.
const char* SKIN_VS_HEAD = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexColor;
in vec4 vertexBoneIndices; in vec4 vertexBoneWeights;
uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;
uniform int skinned;
uniform mat4 boneMatrices[128];
mat4 skinMatrix() {
    if (skinned == 0) return mat4(1.0);
    return boneMatrices[int(vertexBoneIndices.x)] * vertexBoneWeights.x
         + boneMatrices[int(vertexBoneIndices.y)] * vertexBoneWeights.y
         + boneMatrices[int(vertexBoneIndices.z)] * vertexBoneWeights.z
         + boneMatrices[int(vertexBoneIndices.w)] * vertexBoneWeights.w;
}
)";

const char* LIT_VS_BODY = R"(
uniform mat4 lightVP;
out vec3 fragPos; out vec2 fragTexCoord; out vec3 fragNormal; out vec4 fragColor; out vec4 fragLightPos;
void main() {
    mat4 skin = skinMatrix();
    vec4 pos = skin * vec4(vertexPosition, 1.0);
    vec4 world = matModel * pos;
    fragPos = world.xyz;
    fragNormal = mat3(matNormal) * (mat3(skin) * vertexNormal);
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragLightPos = lightVP * world;
    gl_Position = mvp * pos;
}
)";

const char* DEPTH_VS_BODY = R"(
void main() { gl_Position = mvp * (skinMatrix() * vec4(vertexPosition, 1.0)); }
)";

const char* DEPTH_FS = R"(#version 330
out vec4 finalColor;
void main() { finalColor = vec4(1.0); }
)";

// Colours arrive as sRGB (texture, colDiffuse, vertex colour) or already linear (scene
// uniforms, converted on the CPU); lighting is linear and the result is re-encoded.
const char* LIT_FS = R"(#version 330
in vec3 fragPos; in vec2 fragTexCoord; in vec3 fragNormal; in vec4 fragColor; in vec4 fragLightPos;
uniform sampler2D texture0; uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 skyColor; uniform vec3 groundColor;
uniform int lightCount;
uniform int lightType[8]; uniform vec3 lightPos[8]; uniform vec3 lightDir[8]; uniform vec3 lightColor[8];
uniform float lightRange[8]; uniform float lightCosInner[8]; uniform float lightCosOuter[8];
uniform float roughness; uniform float metalness; uniform vec3 emissive; uniform int unlit; uniform float opacity;
uniform int receiveShadow; uniform int shadowLight; uniform sampler2D shadowMap;
uniform float shadowBias; uniform float shadowTexel;
uniform vec3 fogColor; uniform float fogNear; uniform float fogFar;
out vec4 finalColor;

float shadowFactor() {
    vec3 p = fragLightPos.xyz / fragLightPos.w * 0.5 + 0.5;
    if (p.z >= 1.0 || p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0) return 1.0;
    float lit = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            lit += (p.z - shadowBias > texture(shadowMap, p.xy + vec2(x, y) * shadowTexel).r) ? 0.0 : 1.0;
    return lit / 9.0;
}

void main() {
    vec4 base = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (unlit == 1) { finalColor = vec4(base.rgb, base.a * opacity); return; }
    vec3 albedo = pow(base.rgb, vec3(2.2));
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 v = normalize(viewPos - fragPos);

    vec3 color = mix(groundColor, skyColor, n.y * 0.5 + 0.5) * albedo;
    float shininess = exp2(10.0 * (1.0 - clamp(roughness, 0.0, 1.0)) + 1.0);
    vec3 specColor = mix(vec3(0.04), albedo, metalness);
    vec3 diffColor = albedo * (1.0 - metalness);
    for (int i = 0; i < 8; i++) {
        if (i >= lightCount) break;
        vec3 l;
        float atten = 1.0;
        if (lightType[i] == 0) l = -lightDir[i];
        else {
            vec3 d = lightPos[i] - fragPos;
            float dist = length(d);
            l = d / max(dist, 1e-4);
            if (lightRange[i] > 0.0) {
                float k = clamp(1.0 - (dist * dist) / (lightRange[i] * lightRange[i]), 0.0, 1.0);
                atten = k * k;
            }
            if (lightType[i] == 2) atten *= smoothstep(lightCosOuter[i], lightCosInner[i], dot(-l, lightDir[i]));
        }
        float ndl = dot(n, l);
        if (ndl <= 0.0 || atten <= 0.0) continue;
        if (i == shadowLight && receiveShadow == 1) atten *= shadowFactor();
        vec3 h = normalize(l + v);
        float spec = pow(max(dot(n, h), 0.0), shininess) * (shininess + 8.0) / 25.1327;
        color += (diffColor + specColor * spec) * lightColor[i] * atten * ndl;
    }
    color += emissive;
    if (fogFar > fogNear) {
        float f = clamp((length(viewPos - fragPos) - fogNear) / (fogFar - fogNear), 0.0, 1.0);
        color = mix(color, fogColor, f);
    }
    finalColor = vec4(pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2)), base.a * opacity);
}
)";

struct LitLocs {
    int viewPos, sky, ground, lightCount, lightType, lightPos, lightDir, lightColor, lightRange,
        cosInner, cosOuter, roughness, metalness, emissive, unlit, opacity, receiveShadow,
        shadowLight, shadowMap, shadowBias, shadowTexel, fogColor, fogNear, fogFar, lightVP,
        skinned, bones;
};

struct Gpu {
    bool ready = false;
    Shader lit{}, depth{};
    LitLocs L{};
    int depthSkinned = -1, depthBones = -1;
    ::Material material{};               // raylib material: owns a MAX_MATERIAL_MAPS maps array
    unsigned int shadowFbo = 0, shadowTex = 0;
    int shadowSize = 0;
    std::map<std::string, Texture2D> textures;
};
Gpu gpu;

Texture2D whiteTexture() { return { rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 }; }

void releaseGpu() {
    if (gpu.lit.id) UnloadShader(gpu.lit);
    if (gpu.depth.id) UnloadShader(gpu.depth);
    if (gpu.material.maps) {
        gpu.material.maps[MATERIAL_MAP_DIFFUSE].texture = whiteTexture();   // cached textures are freed below
        UnloadMaterial(gpu.material);
    }
    if (gpu.shadowFbo) rlUnloadFramebuffer(gpu.shadowFbo);   // also frees the attached depth texture
    for (auto& [path, tex] : gpu.textures) UnloadTexture(tex);
    gpu = Gpu{};
}

void ensureGpu() {
    if (gpu.ready) return;
    const std::string head = SKIN_VS_HEAD;
    gpu.lit = LoadShaderFromMemory((head + LIT_VS_BODY).c_str(), LIT_FS);
    gpu.depth = LoadShaderFromMemory((head + DEPTH_VS_BODY).c_str(), DEPTH_FS);
    if (!IsShaderValid(gpu.lit) || !IsShaderValid(gpu.depth)) throw std::runtime_error("g3d: shader compilation failed");
    auto loc = [](const char* name) { return GetShaderLocation(gpu.lit, name); };
    gpu.L = { loc("viewPos"), loc("skyColor"), loc("groundColor"), loc("lightCount"), loc("lightType"),
              loc("lightPos"), loc("lightDir"), loc("lightColor"), loc("lightRange"), loc("lightCosInner"),
              loc("lightCosOuter"), loc("roughness"), loc("metalness"), loc("emissive"), loc("unlit"),
              loc("opacity"), loc("receiveShadow"), loc("shadowLight"), loc("shadowMap"), loc("shadowBias"),
              loc("shadowTexel"), loc("fogColor"), loc("fogNear"), loc("fogFar"), loc("lightVP"),
              loc("skinned"), loc("boneMatrices") };
    gpu.depthSkinned = GetShaderLocation(gpu.depth, "skinned");
    gpu.depthBones = GetShaderLocation(gpu.depth, "boneMatrices");
    gpu.material = LoadMaterialDefault();
    gpu.ready = true;
    onGraphicsShutdown(releaseGpu);
}

bool ensureShadowMap(int size) {
    size = std::clamp(size, 256, 8192);
    if (gpu.shadowFbo && gpu.shadowSize == size) return true;
    if (gpu.shadowFbo) { rlUnloadFramebuffer(gpu.shadowFbo); gpu.shadowFbo = gpu.shadowTex = 0; }
    gpu.shadowFbo = rlLoadFramebuffer();
    gpu.shadowTex = rlLoadTextureDepth(size, size, false);
    rlFramebufferAttach(gpu.shadowFbo, gpu.shadowTex, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    rlDisableFramebuffer();
    if (!rlFramebufferComplete(gpu.shadowFbo)) {
        TraceLog(LOG_WARNING, "g3d: shadow framebuffer incomplete; shadows disabled");
        rlUnloadFramebuffer(gpu.shadowFbo);
        gpu.shadowFbo = gpu.shadowTex = 0;
        return false;
    }
    gpu.shadowSize = size;
    return true;
}

Texture2D texture(const std::string& path) {
    auto it = gpu.textures.find(path);
    if (it != gpu.textures.end()) return it->second;
    Texture2D tex = LoadTexture(path.c_str());
    if (!IsTextureValid(tex)) throw std::runtime_error("g3d: cannot load texture: " + path);
    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    gpu.textures[path] = tex;
    return tex;
}

// sRGB 0..255 -> linear 0..1, times k.
Vector3 linear(Color c, float k = 1) {
    auto f = [](unsigned char v) { return std::pow(v / 255.f, 2.2f); };
    return { f(c.r) * k, f(c.g) * k, f(c.b) * k };
}

// ---- scene traversal -----------------------------------------------------------------
struct Item { Object3D* obj; Matrix world; float viewDepth; };

void animate(std::vector<std::unique_ptr<Object3D>>& list, int t) {
    for (auto& o : list) {
        if (o->up) o->up(t, *o);
        animate(o->children, t);
    }
}

void collect(std::vector<std::unique_ptr<Object3D>>& list, const Matrix& parent, std::vector<Item>& out) {
    for (auto& o : list) {
        if (!o->visible) continue;
        Matrix local = MatrixMultiply(MatrixMultiply(MatrixScale(o->scale.x, o->scale.y, o->scale.z),
                                                     MatrixRotateXYZ(Vector3Scale(o->rotation, DEG2RAD))),
                                      MatrixTranslate(o->position.x, o->position.y, o->position.z));
        Matrix world = MatrixMultiply(local, parent);
        if (o->geometry || o->model) out.push_back({ o.get(), world, 0 });
        collect(o->children, world, out);
    }
}

void setBones(int loc, int skinnedLoc, const Shader& sh, const Model* model, bool skinned) {
    int flag = skinned ? 1 : 0;
    SetShaderValue(sh, skinnedLoc, &flag, SHADER_UNIFORM_INT);
    if (skinned && loc >= 0) {
        rlEnableShader(sh.id);
        rlSetUniformMatrices(loc, model->boneMatrices, (int)std::min<unsigned>(model->skeleton.boneCount, MAX_BONES));
    }
}

// Per-material uniforms of the lit shader.
void setMaterial(const g3d::Material& m) {
    const Shader& sh = gpu.lit;
    int unlit = m.unlit ? 1 : 0, receive = m.receiveShadow ? 1 : 0;
    Vector3 em = linear(m.emissive);
    SetShaderValue(sh, gpu.L.roughness, &m.roughness, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, gpu.L.metalness, &m.metalness, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, gpu.L.emissive, &em, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, gpu.L.unlit, &unlit, SHADER_UNIFORM_INT);
    SetShaderValue(sh, gpu.L.opacity, &m.opacity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, gpu.L.receiveShadow, &receive, SHADER_UNIFORM_INT);
}

// Draws one item with the lit shader (depthPass = false) or the depth shader.
void drawItem(const Item& it, bool depthPass) {
    Object3D& o = *it.obj;
    const Shader& sh = depthPass ? gpu.depth : gpu.lit;
    const int bonesLoc = depthPass ? gpu.depthBones : gpu.L.bones;
    const int skinnedLoc = depthPass ? gpu.depthSkinned : gpu.L.skinned;
    ::Material mat = gpu.material;
    mat.shader = sh;
    if (!depthPass) setMaterial(o.material);
    if (o.material.doubleSided) rlDisableBackfaceCulling(); else rlEnableBackfaceCulling();

    if (o.geometry) {
        mat.maps[MATERIAL_MAP_DIFFUSE].texture = o.material.map.empty() ? whiteTexture() : texture(o.material.map);
        mat.maps[MATERIAL_MAP_DIFFUSE].color = o.material.color;
        setBones(bonesLoc, skinnedLoc, sh, nullptr, false);
        DrawMesh(o.geometry->mesh(), mat, it.world);
        return;
    }
    Model& model = o.model->model();
    o.model->pose(o.animation, o.animationFrame);
    const Matrix world = MatrixMultiply(model.transform, it.world);
    for (int i = 0; i < model.meshCount; i++) {
        const Mesh& mesh = model.meshes[i];
        const ::Material& own = model.materials[model.meshMaterial[i]];
        if (o.modelMaterials) {
            mat.maps[MATERIAL_MAP_DIFFUSE].texture = own.maps[MATERIAL_MAP_DIFFUSE].texture.id
                ? own.maps[MATERIAL_MAP_DIFFUSE].texture : whiteTexture();
            Color oc = own.maps[MATERIAL_MAP_DIFFUSE].color, tint = o.material.color;
            mat.maps[MATERIAL_MAP_DIFFUSE].color = { (unsigned char)(oc.r * tint.r / 255), (unsigned char)(oc.g * tint.g / 255),
                                                     (unsigned char)(oc.b * tint.b / 255), (unsigned char)(oc.a * tint.a / 255) };
        } else {
            mat.maps[MATERIAL_MAP_DIFFUSE].texture = o.material.map.empty() ? whiteTexture() : texture(o.material.map);
            mat.maps[MATERIAL_MAP_DIFFUSE].color = o.material.color;
        }
        bool skinned = mesh.boneIndices && mesh.boneWeights && model.boneMatrices && model.skeleton.boneCount > 0;
        setBones(bonesLoc, skinnedLoc, sh, &model, skinned);
        DrawMesh(mesh, mat, world);
    }
}

Vector3 safeUp(Vector3 dir, Vector3 up) {
    return std::fabs(Vector3DotProduct(Vector3Normalize(dir), Vector3Normalize(up))) > 0.999f ? Vector3{ 0, 0, 1 } : up;
}

// Light view/projection for the shadow pass, loaded into rlgl's matrices.
Matrix loadLightMatrices(const Light& l) {
    Vector3 dir = Vector3Subtract(l.target, l.position);
    float dist = std::max(0.01f, Vector3Length(dir));
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    if (l.type == Light::Type::Directional) {
        float e = l.shadowExtent;
        rlOrtho(-e, e, -e, e, 0.01, dist + 4.0 * e);
    } else {
        float top = 0.05f * std::tan(std::min(89.f, l.outerAngle) * DEG2RAD);
        rlFrustum(-top, top, -top, top, 0.05, l.range > 0 ? l.range : 200.0);
    }
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    // Directional lights are pulled back so the whole shadowed area is in front of them.
    Vector3 eye = l.type == Light::Type::Directional
        ? Vector3Subtract(l.target, Vector3Scale(Vector3Normalize(dir), dist + 2.0f * l.shadowExtent))
        : l.position;
    rlMultMatrixf(MatrixToFloat(MatrixLookAt(eye, l.target, safeUp(dir, { 0, 1, 0 }))));
    return MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
}

void setSceneUniforms(const Scene& s, const Camera& cam, int shadowLight, const Matrix& lightVP) {
    const Shader& sh = gpu.lit;
    const LitLocs& L = gpu.L;
    int count = std::min((int)s.lights.size(), MAX_LIGHTS);
    int type[MAX_LIGHTS] = {};
    Vector3 pos[MAX_LIGHTS] = {}, dir[MAX_LIGHTS] = {}, col[MAX_LIGHTS] = {};
    float range[MAX_LIGHTS] = {}, cin[MAX_LIGHTS] = {}, cosOut[MAX_LIGHTS] = {};
    for (int i = 0; i < count; i++) {
        const Light& l = s.lights[(size_t)i];
        type[i] = (int)l.type;
        pos[i] = l.position;
        Vector3 d = Vector3Subtract(l.target, l.position);
        dir[i] = Vector3Length(d) > 1e-6f ? Vector3Normalize(d) : Vector3{ 0, -1, 0 };
        col[i] = linear(l.color, l.intensity);
        range[i] = l.range;
        float outer = std::clamp(l.outerAngle, 0.1f, 89.9f), inner = std::clamp(l.innerAngle, 0.f, outer - 0.05f);
        cin[i] = std::cos(inner * DEG2RAD);
        cosOut[i] = std::cos(outer * DEG2RAD);
    }
    Vector3 sky = linear(s.skyColor, s.ambient), ground = linear(s.groundColor, s.ambient), fog = linear(s.fogColor);
    SetShaderValue(sh, L.viewPos, &cam.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, L.sky, &sky, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, L.ground, &ground, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, L.lightCount, &count, SHADER_UNIFORM_INT);
    SetShaderValueV(sh, L.lightType, type, SHADER_UNIFORM_INT, MAX_LIGHTS);
    SetShaderValueV(sh, L.lightPos, pos, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
    SetShaderValueV(sh, L.lightDir, dir, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
    SetShaderValueV(sh, L.lightColor, col, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
    SetShaderValueV(sh, L.lightRange, range, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
    SetShaderValueV(sh, L.cosInner, cin, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
    SetShaderValueV(sh, L.cosOuter, cosOut, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
    SetShaderValue(sh, L.fogColor, &fog, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, L.fogNear, &s.fogNear, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, L.fogFar, &s.fogFar, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, L.shadowLight, &shadowLight, SHADER_UNIFORM_INT);
    SetShaderValueMatrix(sh, L.lightVP, lightVP);
    if (shadowLight >= 0) {
        const Light& l = s.lights[(size_t)shadowLight];
        float texel = 1.f / (float)gpu.shadowSize;
        int slot = SHADOW_SLOT;
        SetShaderValue(sh, L.shadowBias, &l.shadowBias, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, L.shadowTexel, &texel, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, L.shadowMap, &slot, SHADER_UNIFORM_INT);
    }
}

void renderBody(Scene& s, Camera& cam, Rectangle vp, const RenderTexture& target, float scale) {
    ensureGpu();
    std::vector<Item> items;
    collect(s.objects, MatrixIdentity(), items);

    // Background, in 2D before the viewport changes.
    if (s.background.a > 0) {
        DrawRectangleRec(vp, s.background);
        rlDrawRenderBatchActive();
    }

    // Shadow pass: the first casting directional or spot light.
    int shadowLight = -1;
    Matrix lightVP = MatrixIdentity();
    for (int i = 0; i < std::min((int)s.lights.size(), MAX_LIGHTS); i++) {
        const Light& l = s.lights[(size_t)i];
        if (l.castShadow && l.type != Light::Type::Point) { shadowLight = i; break; }
    }
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    if (shadowLight >= 0 && ensureShadowMap(s.shadowMapSize)) {
        rlDrawRenderBatchActive();
        rlEnableFramebuffer(gpu.shadowFbo);
        rlViewport(0, 0, gpu.shadowSize, gpu.shadowSize);
        rlClearScreenBuffers();
        rlEnableDepthTest();
        rlEnableDepthMask();
        lightVP = loadLightMatrices(s.lights[(size_t)shadowLight]);
        for (const Item& it : items)
            if (it.obj->material.castShadow && it.obj->material.opacity >= 1.f && !it.obj->material.unlit) drawItem(it, true);
        rlEnableFramebuffer(target.id);
    } else shadowLight = -1;

    // Main pass, inside the viewport (GL's origin is bottom-left).
    const int fbH = target.texture.height;
    const int vx = (int)std::lround(vp.x * scale), vy = (int)std::lround(vp.y * scale);
    const int vw = std::max(1, (int)std::lround(vp.width * scale)), vh = std::max(1, (int)std::lround(vp.height * scale));
    rlViewport(vx, fbH - vy - vh, vw, vh);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    const double aspect = (double)vw / vh;
    if (cam.orthographic) {
        double top = cam.orthoHeight / 2.0;
        rlOrtho(-top * aspect, top * aspect, -top, top, cam.nearPlane, cam.farPlane);
    } else {
        double top = cam.nearPlane * std::tan(cam.fov * 0.5 * DEG2RAD);
        rlFrustum(-top * aspect, top * aspect, -top, top, cam.nearPlane, cam.farPlane);
    }
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    const Matrix view = MatrixLookAt(cam.position, cam.target,
                                     safeUp(Vector3Subtract(cam.target, cam.position), cam.upVector));
    rlMultMatrixf(MatrixToFloat(view));
    rlEnableDepthTest();
    rlEnableDepthMask();

    setSceneUniforms(s, cam, shadowLight, lightVP);
    if (shadowLight >= 0) {
        rlActiveTextureSlot(SHADOW_SLOT);
        rlEnableTexture(gpu.shadowTex);
        rlActiveTextureSlot(0);
    }
    std::vector<Item> clear;
    for (Item& it : items) {
        if (it.obj->material.opacity < 1.f) {
            Vector3 p = Vector3Transform({ 0, 0, 0 }, MatrixMultiply(it.world, view));
            it.viewDepth = -p.z;
            clear.push_back(it);
        } else drawItem(it, false);
    }
    std::sort(clear.begin(), clear.end(), [](const Item& a, const Item& b) { return a.viewDepth > b.viewDepth; });
    rlDisableDepthMask();
    for (const Item& it : clear) drawItem(it, false);
    rlEnableDepthMask();

    if (shadowLight >= 0) {
        rlActiveTextureSlot(SHADOW_SLOT);
        rlDisableTexture();
        rlActiveTextureSlot(0);
    }
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlViewport(0, 0, target.texture.width, fbH);
}

} // namespace

// ------------------------------------------------------------------ geometry
Geometry::~Geometry() = default;   // the GPU mesh is freed by shutdownGraphics, not here

const Mesh& Geometry::mesh() {
    if (!loaded_) {
        mesh_ = make_();
        if (mesh_.vertexCount == 0) throw std::runtime_error("g3d: mesh generation failed");
        loaded_ = true;
        // A geometry destroyed before shutdown leaves its mesh to the GL context's teardown.
        std::weak_ptr<Geometry> self = weak_from_this();
        onGraphicsShutdown([self] {
            if (auto g = self.lock()) { UnloadMesh(g->mesh_); g->mesh_ = Mesh{}; g->loaded_ = false; }
        });
    }
    return mesh_;
}

std::shared_ptr<Geometry> Geometry::box(float w, float h, float d) {
    return std::make_shared<Geometry>([=] { return GenMeshCube(w, h, d); });
}
std::shared_ptr<Geometry> Geometry::sphere(float r, int rings, int slices) {
    return std::make_shared<Geometry>([=] { return GenMeshSphere(r, rings, slices); });
}
std::shared_ptr<Geometry> Geometry::plane(float w, float d, int rx, int rz) {
    return std::make_shared<Geometry>([=] { return GenMeshPlane(w, d, rx, rz); });
}
namespace {
// raylib builds cylinders and cones upwards from y = 0; centre them like the other shapes.
Mesh centeredY(Mesh m) {
    BoundingBox b = GetMeshBoundingBox(m);
    const float dy = -(b.min.y + b.max.y) / 2;
    for (int i = 0; i < m.vertexCount; i++) m.vertices[i * 3 + 1] += dy;
    UpdateMeshBuffer(m, 0, m.vertices, m.vertexCount * 3 * (int)sizeof(float), 0);
    return m;
}

// A (p, q) torus knot swept along its curve, as Three.js's TorusKnotGeometry.
Mesh torusKnot(float radius, float tube, int tubular, int radial, int p, int q) {
    tubular = std::max(3, tubular);
    radial = std::max(3, radial);
    while ((tubular + 1) * (radial + 1) > 65535) tubular /= 2;            // 16-bit indices
    auto curve = [&](float u) {
        const float qp = (float)q / p * u, cs = std::cos(qp);
        return Vector3{ radius * (2 + cs) * 0.5f * std::cos(u), radius * (2 + cs) * 0.5f * std::sin(u),
                        radius * std::sin(qp) * 0.5f };
    };
    Mesh m{};
    m.vertexCount = (tubular + 1) * (radial + 1);
    m.triangleCount = tubular * radial * 2;
    m.vertices = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
    m.normals = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
    m.texcoords = (float*)MemAlloc((unsigned)(m.vertexCount * 2 * sizeof(float)));
    m.indices = (unsigned short*)MemAlloc((unsigned)(m.triangleCount * 3 * sizeof(unsigned short)));
    int v = 0;
    for (int j = 0; j <= tubular; j++) {
        const float u = (float)j / tubular * p * 2 * PI;
        const Vector3 p1 = curve(u), p2 = curve(u + 0.01f);
        const Vector3 t = Vector3Subtract(p2, p1);
        Vector3 n = Vector3Add(p2, p1);
        const Vector3 b = Vector3Normalize(Vector3CrossProduct(t, n));
        n = Vector3Normalize(Vector3CrossProduct(b, t));
        for (int i = 0; i <= radial; i++, v++) {
            const float a = (float)i / radial * 2 * PI, cx = -tube * std::cos(a), cy = tube * std::sin(a);
            const Vector3 pos{ p1.x + cx * n.x + cy * b.x, p1.y + cx * n.y + cy * b.y, p1.z + cx * n.z + cy * b.z };
            const Vector3 nrm = Vector3Normalize(Vector3Subtract(pos, p1));
            m.vertices[v * 3] = pos.x; m.vertices[v * 3 + 1] = pos.y; m.vertices[v * 3 + 2] = pos.z;
            m.normals[v * 3] = nrm.x; m.normals[v * 3 + 1] = nrm.y; m.normals[v * 3 + 2] = nrm.z;
            m.texcoords[v * 2] = (float)j / tubular; m.texcoords[v * 2 + 1] = (float)i / radial;
        }
    }
    int k = 0;
    for (int j = 1; j <= tubular; j++)
        for (int i = 1; i <= radial; i++) {
            const unsigned short a = (unsigned short)((radial + 1) * (j - 1) + (i - 1)), b = (unsigned short)((radial + 1) * j + (i - 1));
            const unsigned short c = (unsigned short)((radial + 1) * j + i), d = (unsigned short)((radial + 1) * (j - 1) + i);
            const unsigned short tri[6] = { a, b, d, b, c, d };
            for (unsigned short x : tri) m.indices[k++] = x;
        }
    UploadMesh(&m, false);
    return m;
}
}

std::shared_ptr<Geometry> Geometry::cylinder(float r, float h, int slices) {
    return std::make_shared<Geometry>([=] { return centeredY(GenMeshCylinder(r, h, slices)); });
}
std::shared_ptr<Geometry> Geometry::cone(float r, float h, int slices) {
    return std::make_shared<Geometry>([=] { return centeredY(GenMeshCone(r, h, slices)); });
}
std::shared_ptr<Geometry> Geometry::torus(float r, float tube, int radSeg, int sides) {
    // raylib's parameters are (tube / major radius, 2 * major radius, tube segments, ring segments).
    return std::make_shared<Geometry>([=] { return GenMeshTorus(tube / r, 2 * r, sides, radSeg); });
}
std::shared_ptr<Geometry> Geometry::knot(float r, float tube, int tubular, int radial, int p, int q) {
    return std::make_shared<Geometry>([=] { return torusKnot(r, tube, tubular, radial, p, q); });
}

// ------------------------------------------------------------------ models
ModelAsset::~ModelAsset() = default;     // see Geometry

void ModelAsset::ensure() {
    if (loaded_) return;
    if (!FileExists(path_.c_str())) throw std::runtime_error("g3d: model file not found: " + path_);
    model_ = LoadModel(path_.c_str());
    if (!IsModelValid(model_) || model_.meshCount == 0) throw std::runtime_error("g3d: cannot load model: " + path_);
    anims_ = LoadModelAnimations(path_.c_str(), &animCount_);
    if (!anims_) animCount_ = 0;
    loaded_ = true;
    std::weak_ptr<ModelAsset> self = weak_from_this();
    onGraphicsShutdown([self] {
        if (auto m = self.lock()) {
            if (m->anims_) UnloadModelAnimations(m->anims_, m->animCount_);
            UnloadModel(m->model_);
            m->model_ = Model{}; m->anims_ = nullptr; m->animCount_ = 0; m->loaded_ = false;
        }
    });
}
Model& ModelAsset::model() { ensure(); return model_; }
int ModelAsset::animationCount() { ensure(); return animCount_; }
int ModelAsset::animationIndex(const std::string& name) {
    ensure();
    for (int i = 0; i < animCount_; i++) if (name == anims_[i].name) return i;
    return -1;
}
int ModelAsset::animationFrames(int index) {
    ensure();
    return index >= 0 && index < animCount_ ? anims_[index].keyframeCount : 0;
}
void ModelAsset::pose(int animation, float frame) {
    ensure();
    if (!model_.boneMatrices) return;
    if (animation >= 0 && animation < animCount_) {
        UpdateModelAnimationBones(model_, anims_[animation], std::max(0.f, frame));
        return;
    }
    for (unsigned b = 0; b < model_.skeleton.boneCount; b++) model_.boneMatrices[b] = MatrixIdentity();
}

ModelPtr loadModel(const std::string& path) { return std::make_shared<ModelAsset>(path); }

// ------------------------------------------------------------------ objects, lights
Object3D* Object3D::add(GeometryPtr g, Material m) {
    Object3D* o = add();
    o->geometry = std::move(g);
    o->material = std::move(m);
    return o;
}
Object3D* Object3D::add(ModelPtr m) {
    Object3D* o = add();
    o->model = std::move(m);
    return o;
}
Object3D* Object3D::add() {
    children.push_back(std::make_unique<Object3D>());
    return children.back().get();
}

Light Light::directional(Vector3 from, Color c, float intensity, bool shadow) {
    Light l;
    l.type = Type::Directional; l.position = from; l.color = c; l.intensity = intensity; l.castShadow = shadow;
    return l;
}
Light Light::point(Vector3 at, Color c, float intensity, float range) {
    Light l;
    l.type = Type::Point; l.position = at; l.color = c; l.intensity = intensity; l.range = range;
    return l;
}
Light Light::spot(Vector3 from, Vector3 to, Color c, float intensity, float outerAngle, bool shadow) {
    Light l;
    l.type = Type::Spot; l.position = from; l.target = to; l.color = c; l.intensity = intensity;
    l.outerAngle = outerAngle; l.innerAngle = outerAngle * 0.75f; l.castShadow = shadow;
    return l;
}

// ------------------------------------------------------------------ scenes
Object3D* Scene::add(GeometryPtr g, Material m) {
    Object3D* o = add();
    o->geometry = std::move(g);
    o->material = std::move(m);
    return o;
}
Object3D* Scene::add(ModelPtr m) {
    Object3D* o = add();
    o->model = std::move(m);
    return o;
}
Object3D* Scene::add() {
    objects.push_back(std::make_unique<Object3D>());
    return objects.back().get();
}

namespace {
Object3D* findIn(std::vector<std::unique_ptr<Object3D>>& list, const std::string& name) {
    for (auto& o : list) {
        if (o->name == name) return o.get();
        if (Object3D* hit = findIn(o->children, name)) return hit;
    }
    return nullptr;
}
}
Object3D* Scene::find(const std::string& name) { return findIn(objects, name); }

void render(Scene& s, int t, const Ctx& c, Rectangle viewport, Camera* camera) {
    Camera& cam = camera ? *camera : s.camera;
    if (s.up) s.up(t, s);
    if (cam.up) cam.up(t, cam);
    for (Light& l : s.lights) if (l.up) l.up(t, l);
    animate(s.objects, t);
    drawOffscreen(c.a, [&](const RenderTexture& target, float scale) {
        Rectangle vp = viewport;
        if (vp.width <= 0 || vp.height <= 0) vp = { 0, 0, target.texture.width / scale, target.texture.height / scale };
        else vp = { c.o.x + vp.x * c.s, c.o.y + vp.y * c.s, vp.width * c.s, vp.height * c.s };
        renderBody(s, cam, vp, target, scale);
    });
}

} // namespace g3d
} // namespace rmtn
