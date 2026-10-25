// scene3d.cpp -- GPU 3D: lazy GPU resources, the lit/shadow/DOF shaders and scene rendering.
#include "rmtn/scene3d.hpp"
#include "rmtn/render.hpp"
#include "rmtn/media.hpp"
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
constexpr int MAX_TILES = 16;            // shadow-atlas tiles per scene
constexpr int MAX_ATLAS = 8192;
constexpr int ENV_SLOT = 13, SHADOW_SLOT = 14, DEPTH_SLOT = 15;   // material maps use units 0..11

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
out vec3 fragPos; out vec2 fragTexCoord; out vec3 fragNormal; out vec4 fragColor;
void main() {
    mat4 skin = skinMatrix();
    vec4 pos = skin * vec4(vertexPosition, 1.0);
    vec4 world = matModel * pos;
    fragPos = world.xyz;
    fragNormal = mat3(matNormal) * (mat3(skin) * vertexNormal);
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
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
in vec3 fragPos; in vec2 fragTexCoord; in vec3 fragNormal; in vec4 fragColor;
uniform sampler2D texture0; uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 skyColor; uniform vec3 groundColor;
uniform int lightCount;
uniform int lightType[8]; uniform vec3 lightPos[8]; uniform vec3 lightDir[8]; uniform vec3 lightColor[8];
uniform float lightRange[8]; uniform float lightCosInner[8]; uniform float lightCosOuter[8];
uniform int lightShadow[8]; uniform float lightBias[8];
uniform float roughness; uniform float metalness; uniform vec3 emissive; uniform int unlit; uniform float opacity;
uniform int receiveShadow;
uniform sampler2D shadowAtlas; uniform float atlasTexel;
uniform mat4 shadowVP[16]; uniform vec4 shadowRect[16]; uniform vec4 shadowParams[16];
uniform sampler2D envMap; uniform int hasEnv; uniform int envLinear; uniform float envMaxLod;
uniform float envIntensity; uniform float envRotation; uniform int skybox;
uniform vec3 fogColor; uniform float fogNear; uniform float fogFar;
uniform int shading; uniform float pxScale;
uniform int toonBands; uniform float toonSoftness; uniform float toonHighlight; uniform float toonHighlightSize;
uniform vec3 rimColor; uniform float rimWidth;
uniform vec3 goochWarm; uniform vec3 goochCool;
uniform vec3 hatchInk; uniform float hatchSpacing; uniform float hatchAngle; uniform float hatchWeight;
out vec4 finalColor;

const float PI = 3.14159265;

// ---- shadows: atlas tiles; params = (near, far, perspective, texel size per unit depth)
float linearDepth(float d, vec4 prm) {
    if (prm.z > 0.5) { float z = d * 2.0 - 1.0; return 2.0 * prm.x * prm.y / (prm.y + prm.x - z * (prm.y - prm.x)); }
    return prm.x + d * (prm.y - prm.x);
}
float shadowAt(int tile, float bias, float ndl) {
    vec4 clip = shadowVP[tile] * vec4(fragPos, 1.0);
    vec3 p = clip.xyz / clip.w * 0.5 + 0.5;
    if (p.z >= 1.0 || p.z <= 0.0 || p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0) return 1.0;
    vec4 prm = shadowParams[tile], r = shadowRect[tile];
    float zf = prm.z > 0.5 ? clip.w : linearDepth(p.z, prm);
    float texelWorld = prm.w * (prm.z > 0.5 ? zf : 1.0);
    float slope = sqrt(max(1.0 - ndl * ndl, 0.0)) / max(ndl, 0.1);
    float b = bias + texelWorld * (1.5 + min(slope, 4.0));
    vec2 lo = r.xy + vec2(1.5 * atlasTexel), hi = r.xy + r.zw - vec2(1.5 * atlasTexel);
    float lit = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++) {
            vec2 uv = clamp(r.xy + p.xy * r.zw + vec2(x, y) * atlasTexel, lo, hi);
            lit += zf - b > linearDepth(texture(shadowAtlas, uv).r, prm) ? 0.0 : 1.0;
        }
    return lit / 9.0;
}
int cubeFace(vec3 d) {
    vec3 a = abs(d);
    if (a.x >= a.y && a.x >= a.z) return d.x > 0.0 ? 0 : 1;
    if (a.y >= a.z) return d.y > 0.0 ? 2 : 3;
    return d.z > 0.0 ? 4 : 5;
}

// ---- environment: equirectangular, mip level by blur
vec3 env(vec3 d, float lod) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5 + envRotation;
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;
    vec3 c = textureLod(envMap, vec2(u, v), lod).rgb;
    return (envLinear == 1 ? c : pow(c, vec3(2.2))) * envIntensity;
}
// Karis' analytic approximation of the split-sum environment BRDF.
vec3 envBRDF(vec3 spec, float rough, float ndv) {
    vec4 r = rough * vec4(-1.0, -0.0275, -0.572, 0.022) + vec4(1.0, 0.0425, 1.04, -0.04);
    float a004 = min(r.x * r.x, exp2(-9.28 * ndv)) * r.x + r.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
    return spec * ab.x + ab.y;
}

vec4 encode(vec3 c, float a) { return vec4(pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2)), a); }

// Light i at this fragment: `l` points to it and `ndl` is n.l; returns the attenuation
// times shadow (0 when the light does not reach a front-facing point).
float lightAt(int i, vec3 n, out vec3 l, out float ndl) {
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
    ndl = dot(n, l);
    if (ndl <= 0.0 || atten <= 0.0) return 0.0;
    if (lightShadow[i] >= 0 && receiveShadow == 1) {
        int tile = lightShadow[i] + (lightType[i] == 1 ? cubeFace(fragPos - lightPos[i]) : 0);
        atten *= shadowAt(tile, lightBias[i], ndl);
    }
    return atten;
}

// ---- stylised shading
// x in 0..1 to `toonBands` flat steps; each step's edge ramps over toonSoftness.
float band(float x) {
    float b = float(max(toonBands, 1));
    float f = clamp(x, 0.0, 1.0) * b;
    float w = max(toonSoftness * b, 1e-3);
    return min((floor(f) + smoothstep(0.0, w, fract(f))) / b, 1.0);
}
vec3 toon(vec3 albedo, vec3 n, vec3 v) {
    vec3 color = albedo * mix(groundColor, skyColor, 0.5);         // flat ambient: the shadow tone
    float edge = max(toonSoftness, 0.004);
    for (int i = 0; i < 8; i++) {
        if (i >= lightCount) break;
        vec3 l; float ndl;
        float a = lightAt(i, n, l, ndl);
        if (a <= 0.0) continue;
        color += albedo * lightColor[i] * band(ndl * a);
        float spot = dot(n, normalize(l + v));
        float cut = 1.0 - toonHighlightSize * toonHighlightSize;
        color += lightColor[i] * toonHighlight * smoothstep(cut - edge * 0.25, cut + edge * 0.25, spot) * step(0.5, a);
    }
    float r = 1.0 - max(dot(n, v), 0.0);
    color += rimColor * smoothstep(1.0 - rimWidth - edge, 1.0 - rimWidth + edge, r);
    return color;
}
vec3 gooch(vec3 albedo, vec3 n, vec3 v) {
    float t = 0.5;
    float hl = 0.0;
    if (lightCount > 0) {
        vec3 l; float ndl;
        float a = lightAt(0, n, l, ndl);
        t = (1.0 + (ndl > 0.0 ? ndl * a : ndl)) * 0.5;               // shadowed light reads as the terminator
        hl = smoothstep(0.93, 0.96, max(dot(reflect(-l, n), v), 0.0)) * step(0.5, a);
    }
    vec3 cool = goochCool * 0.55 + albedo * 0.2, warm = goochWarm * 0.45 + albedo * 0.55;
    return mix(cool, warm, t) + vec3(hl * 0.8);
}
// One family of parallel strokes at `ang`, `strength` 0..1 sets their weight.
float strokes(vec2 p, float ang, float spacing, float strength) {
    if (strength <= 0.0) return 0.0;
    vec2 d = vec2(cos(ang), sin(ang));
    float u = dot(p, vec2(-d.y, d.x)) / spacing;
    float line = floor(u);
    u += 0.1 * sin(dot(p, d) * 0.045 + line * 2.3);                 // each stroke wavers a little
    float dist = abs(fract(u) - 0.5) * spacing;                      // composition px to the stroke centre
    float width = hatchWeight * (0.35 + 1.25 * strength);
    float aa = 0.7 / pxScale;
    return (1.0 - smoothstep(width * 0.5 - aa, width * 0.5 + aa, dist)) * min(strength * 3.0, 1.0);
}
vec3 hatch(vec3 albedo, vec3 n) {
    vec3 lit = mix(groundColor, skyColor, n.y * 0.5 + 0.5);
    for (int i = 0; i < 8; i++) {
        if (i >= lightCount) break;
        vec3 l; float ndl;
        float a = lightAt(i, n, l, ndl);
        lit += lightColor[i] * max(ndl, 0.0) * a;
    }
    float dark = 1.0 - pow(clamp(dot(lit, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0), 1.0 / 2.2);   // perceived tone
    vec2 p = gl_FragCoord.xy / pxScale;
    float ang = radians(hatchAngle);
    float ink = strokes(p, ang, hatchSpacing, clamp((dark - 0.12) / 0.3, 0.0, 1.0));
    ink = max(ink, strokes(p, ang + PI * 0.5, hatchSpacing, clamp((dark - 0.45) / 0.25, 0.0, 1.0)));
    ink = max(ink, strokes(p, ang + PI * 0.25, hatchSpacing * 0.7, clamp((dark - 0.7) / 0.2, 0.0, 1.0)));
    return mix(albedo, hatchInk, ink);
}

void main() {
    if (skybox == 1) { finalColor = encode(env(normalize(fragPos - viewPos), 0.0), 1.0); return; }
    vec4 base = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (unlit == 1) { finalColor = vec4(base.rgb, base.a * opacity); return; }
    vec3 albedo = pow(base.rgb, vec3(2.2));
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 v = normalize(viewPos - fragPos);

    vec3 color;
    if (shading == 1) color = toon(albedo, n, v);
    else if (shading == 2) color = gooch(albedo, n, v);
    else if (shading == 3) color = hatch(albedo, n);
    else {
        float rough = clamp(roughness, 0.0, 1.0);
        vec3 specColor = mix(vec3(0.04), albedo, metalness);
        vec3 diffColor = albedo * (1.0 - metalness);
        if (hasEnv == 1) {
            float ndv = max(dot(n, v), 1e-4);
            vec3 r = reflect(-v, n);
            // Blur by mip level, stopping where the image is still about 16 x 8: smaller mips
            // average the whole sky and ground together.
            float blurLod = max(envMaxLod - 4.0, 0.0);
            color = diffColor * env(n, blurLod) + env(r, rough * blurLod) * envBRDF(specColor, rough, ndv);
        } else color = mix(groundColor, skyColor, n.y * 0.5 + 0.5) * albedo;

        float shininess = exp2(10.0 * (1.0 - rough) + 1.0);
        for (int i = 0; i < 8; i++) {
            if (i >= lightCount) break;
            vec3 l; float ndl;
            float atten = lightAt(i, n, l, ndl);
            if (atten <= 0.0) continue;
            vec3 h = normalize(l + v);
            float spec = pow(max(dot(n, h), 0.0), shininess) * (shininess + 8.0) / 25.1327;
            color += (diffColor + specColor * spec) * lightColor[i] * atten * ndl;
        }
    }
    color += emissive;
    if (fogFar > fogNear) {
        float f = clamp((length(viewPos - fragPos) - fogNear) / (fogFar - fogNear), 0.0, 1.0);
        color = mix(color, fogColor, f);
    }
    finalColor = encode(color, base.a * opacity);
}
)";

// Particles: camera-facing (or oriented) quads from the batch. The shape rides in the
// texture coordinate, u + 2 * shape, so a sorted run of mixed looks is one draw.
const char* PARTICLE_FS = R"(#version 330
in vec2 fragTexCoord; in vec4 fragColor;
out vec4 finalColor;
float star5(vec2 p, float r, float rf) {             // after Inigo Quilez's sdStar5
    const vec2 k1 = vec2(0.809016994375, -0.587785252292), k2 = vec2(-k1.x, k1.y);
    p.x = abs(p.x);
    p -= 2.0 * max(dot(k1, p), 0.0) * k1;
    p -= 2.0 * max(dot(k2, p), 0.0) * k2;
    p.x = abs(p.x);
    p.y -= r;
    vec2 ba = rf * vec2(-k1.y, k1.x) - vec2(0, 1);
    float h = clamp(dot(p, ba) / dot(ba, ba), 0.0, r);
    return length(p - ba * h) * sign(p.y * ba.x - p.x * ba.y);
}
void main() {
    int shape = int(floor(fragTexCoord.x * 0.5));
    vec2 uv = vec2(fragTexCoord.x - 2.0 * float(shape), fragTexCoord.y);
    vec2 p = uv * 2.0 - 1.0;
    float r = length(p), aa = max(fwidth(r), 1e-3), a = 1.0;
    vec3 col = fragColor.rgb;
    if (shape == 0) a = 1.0 - smoothstep(1.0 - aa, 1.0, r);                  // dot
    else if (shape == 1) a = max(0.0, 1.0 - r);                               // glow
    else if (shape == 2) {                                                    // glow with a hot core
        float halo = max(0.0, 1.0 - r), core = max(0.0, 1.0 - r / 0.35);
        vec3 sum = col * halo + mix(col, vec3(1.0), 0.6) * core;
        a = min(1.0, halo + core);
        col = a > 0.0 ? min(sum / a, vec3(1.0)) : col;
    } else if (shape == 3) {                                                  // streak, tail at u = 0
        float w = max(fwidth(p.y), 1e-3);
        a = smoothstep(0.0, 0.9, uv.x) * (1.0 - smoothstep(1.0 - w, 1.0, abs(p.y)));
    } else if (shape == 5) {
        float d = star5(vec2(p.x, -p.y), 1.0, 0.42);
        a = 1.0 - smoothstep(-aa, aa, d);
    } else if (shape == 6) a = 1.0 - smoothstep(0.12 - aa, 0.12 + aa, abs(r - 0.88));   // ring
    if (fragColor.a * a <= 0.002) discard;
    finalColor = vec4(col, fragColor.a * a);
}
)";

// Depth of field: a gather blur over a golden-angle spiral (after Dennis Gustafsson's
// "Bokeh depth of field in a single pass"). Colour is premultiplied, so alpha blurs too.
const char* DOF_FS = R"(#version 330
in vec2 fragTexCoord; in vec4 fragColor;
uniform sampler2D texture0; uniform sampler2D depthTex;
uniform vec2 texel; uniform float zNear; uniform float zFar; uniform int ortho;
uniform float focus; uniform float aperture; uniform float maxBlur; uniform float radStep;
out vec4 finalColor;
float depthAt(vec2 uv) {
    float d = texture(depthTex, uv).r;
    if (ortho == 1) return zNear + d * (zFar - zNear);
    float z = d * 2.0 - 1.0;
    return 2.0 * zNear * zFar / (zFar + zNear - z * (zFar - zNear));
}
float blurSize(float z) { return clamp(aperture * abs(z - focus) / max(z, 1e-4), 0.0, 1.0) * maxBlur; }
void main() {
    float cz = depthAt(fragTexCoord), cs = blurSize(cz);
    vec4 sum = texture(texture0, fragTexCoord);
    float tot = 1.0, radius = radStep, ang = 0.0;
    for (int i = 0; i < 512; i++) {
        if (radius >= maxBlur) break;
        vec2 tc = fragTexCoord + vec2(cos(ang), sin(ang)) * texel * radius;
        vec4 sc = texture(texture0, tc);
        float sz = depthAt(tc), ss = blurSize(sz);
        if (sz > cz) ss = clamp(ss, 0.0, cs * 2.0);       // sharp things in front stay sharp
        float m = smoothstep(radius - 0.5, radius + 0.5, ss);
        sum += mix(sum / tot, sc, m);
        tot += 1.0;
        radius += radStep / radius;
        ang += 2.39996323;
    }
    finalColor = sum / tot * fragColor;
}
)";

// Ink lines, pass 1: per pixel the view-space normal (octahedral, rg), the line style
// (b, 0 = no lines) and the object id (a, 255 = outside the viewport).
const char* GBUF_FS = R"(#version 330
in vec3 fragPos; in vec2 fragTexCoord; in vec3 fragNormal; in vec4 fragColor;
uniform mat4 matView; uniform float objectId; uniform float lineStyle;
out vec4 finalColor;
vec2 octEncode(vec3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    vec2 e = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
    return e * 0.5 + 0.5;
}
void main() {
    vec3 n = normalize(mat3(matView) * normalize(fragNormal));
    if (!gl_FrontFacing) n = -n;
    finalColor = vec4(octEncode(n), lineStyle / 255.0, objectId / 255.0);
}
)";

// Ink lines, pass 2: a pixel is on a line when, within half the line's width, there is
// an object boundary, a depth jump the surface's slope does not explain, or a crease.
// The nearer side owns the edge (its style sets colour and width). Output is straight
// alpha for the usual "over".
const char* LINE_FS = R"(#version 330
in vec2 fragTexCoord; in vec4 fragColor;
uniform sampler2D texture0; uniform sampler2D depthTex;
uniform vec2 texel; uniform float zNear; uniform float zFar; uniform int ortho;
uniform float pixelWorld;              // world size of one target pixel at unit depth (ortho: absolute)
uniform float pxScale;                 // target px per composition px
uniform vec4 styles[64];               // rgb colour, a = width in target px
uniform float maxRadius;               // target px
uniform float creaseCos; uniform float depthThreshold;
uniform float wobble; uniform float wobbleScale; uniform float seed;
out vec4 finalColor;

struct G { int id; int style; vec3 n; float z; };
vec3 octDecode(vec2 e) {
    e = e * 2.0 - 1.0;
    vec3 n = vec3(e, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) n.xy = (1.0 - abs(n.yx)) * vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
    return normalize(n);
}
float linearZ(float d) {
    if (ortho == 1) return zNear + d * (zFar - zNear);
    float z = d * 2.0 - 1.0;
    return 2.0 * zNear * zFar / (zFar + zNear - z * (zFar - zNear));
}
G fetch(vec2 uv) {
    vec4 g = texture(texture0, uv);
    G r;
    r.id = int(g.a * 255.0 + 0.5);
    r.style = int(g.b * 255.0 + 0.5);
    r.n = octDecode(g.rg);
    r.z = r.id == 0 ? 1e30 : linearZ(texture(depthTex, uv).r);
    return r;
}
float hash(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float vnoise(vec2 p, float s) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(vec3(i, s)), hash(vec3(i + vec2(1, 0), s)), f.x),
               mix(hash(vec3(i + vec2(0, 1), s)), hash(vec3(i + vec2(1, 1), s)), f.x), f.y);
}
// The style owning an edge between c and q at distance r (target px), or 0 for none.
int edgeStyle(G c, G q, float r) {
    if (q.id == 255) return 0;
    float z = min(c.z, q.z);
    float pw = ortho == 1 ? pixelWorld : pixelWorld * z;
    float nz = max(abs(c.n.z), 0.05);
    float slope = min(sqrt(max(1.0 - nz * nz, 0.0)) / nz, 20.0);
    float expected = r * pw * (1.0 + 1.5 * slope);                  // depth change the slope explains
    if (c.id != q.id) {
        int nearer = c.z <= q.z ? c.style : q.style, farther = c.z <= q.z ? q.style : c.style;
        // Where two objects touch, neither is in front: the one with lines draws the contact.
        bool touching = abs(c.z - q.z) <= expected + depthThreshold * z;
        return nearer != 0 || !touching ? nearer : farther;
    }
    if (c.id == 0) return 0;
    if (dot(c.n, q.n) < creaseCos) return c.style;
    return abs(c.z - q.z) > expected + depthThreshold * z ? c.style : 0;
}
void main() {
    vec2 uv = fragTexCoord;
    if (wobble > 0.0) {
        vec2 p = gl_FragCoord.xy / pxScale / wobbleScale;
        vec2 d = vec2(vnoise(p, seed), vnoise(p + 17.3, seed + 5.1)) * 2.0 - 1.0;
        uv += d * wobble * pxScale * texel;
    }
    G c = fetch(uv);
    if (c.id == 255) { finalColor = vec4(0.0); return; }
    float best = 0.0;
    vec3 col = vec3(0.0);
    int steps = int(ceil(maxRadius));
    for (int r = 1; r <= 64; r++) {
        if (r > steps || best >= 1.0) break;
        for (int k = 0; k < 8; k++) {
            float a = float(k) * 0.785398;
            vec2 dir = vec2(cos(a), sin(a));
            int s = edgeStyle(c, fetch(uv + dir * float(r) * texel), float(r));
            if (s == 0) continue;
            float cov = clamp(styles[s].a * 0.5 + 1.0 - float(r), 0.0, 1.0);
            if (cov > best) { best = cov; col = styles[s].rgb; }
        }
    }
    finalColor = vec4(col, best);
}
)";

struct LitLocs {
    int viewPos, sky, ground, lightCount, lightType, lightPos, lightDir, lightColor, lightRange,
        cosInner, cosOuter, lightShadow, lightBias, roughness, metalness, emissive, unlit, opacity,
        receiveShadow, shadowAtlas, atlasTexel, shadowVP, shadowRect, shadowParams, envMap, hasEnv,
        envLinear, envMaxLod, envIntensity, envRotation, skybox, fogColor, fogNear, fogFar, skinned, bones,
        shading, pxScale, toonBands, toonSoftness, toonHighlight, toonHighlightSize, rimColor, rimWidth,
        goochWarm, goochCool, hatchInk, hatchSpacing, hatchAngle, hatchWeight;
};
struct DofLocs { int depthTex, texel, zNear, zFar, ortho, focus, aperture, maxBlur, radStep; };
struct GbufLocs { int objectId, lineStyle, skinned, bones; };
struct LineLocs { int depthTex, texel, zNear, zFar, ortho, pixelWorld, pxScale, styles, maxRadius, creaseCos,
                  depthThreshold, wobble, wobbleScale, seed; };
constexpr int MAX_STYLES = 64;

// An offscreen target with a sampleable depth texture (for depth of field).
struct DepthTarget { unsigned int fbo = 0, color = 0, depth = 0; int w = 0, h = 0; };

struct Gpu {
    bool ready = false;
    Shader lit{}, depth{}, dof{}, gbuf{}, line{}, particle{};
    LitLocs L{};
    DofLocs D{};
    GbufLocs G{};
    LineLocs N{};
    int depthSkinned = -1, depthBones = -1;
    ::Material material{};               // raylib material: owns a MAX_MATERIAL_MAPS maps array
    unsigned int atlasFbo = 0, atlasTex = 0;
    int atlasSize = 0;
    DepthTarget sceneTarget, lineTarget;
    Mesh skyCube{};
    std::map<std::string, Texture2D> textures;
    std::map<const Scene*, Texture2D> generated;    // environmentGenerator results
};
Gpu gpu;

Texture2D whiteTexture() { return { rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 }; }

void unloadDepthTarget(DepthTarget& t) {
    if (t.fbo) rlUnloadFramebuffer(t.fbo);      // frees the attached textures too
    t = DepthTarget{};
}

void releaseGpu() {
    if (gpu.lit.id) UnloadShader(gpu.lit);
    if (gpu.depth.id) UnloadShader(gpu.depth);
    if (gpu.dof.id) UnloadShader(gpu.dof);
    if (gpu.gbuf.id) UnloadShader(gpu.gbuf);
    if (gpu.line.id) UnloadShader(gpu.line);
    if (gpu.particle.id) UnloadShader(gpu.particle);
    if (gpu.material.maps) {
        gpu.material.maps[MATERIAL_MAP_DIFFUSE].texture = whiteTexture();   // cached textures are freed below
        UnloadMaterial(gpu.material);
    }
    if (gpu.atlasFbo) rlUnloadFramebuffer(gpu.atlasFbo);
    unloadDepthTarget(gpu.sceneTarget);
    unloadDepthTarget(gpu.lineTarget);
    if (gpu.skyCube.vertexCount) UnloadMesh(gpu.skyCube);
    for (auto& [path, tex] : gpu.textures) UnloadTexture(tex);
    for (auto& [scene, tex] : gpu.generated) UnloadTexture(tex);
    gpu = Gpu{};
}

void ensureGpu() {
    if (gpu.ready) return;
    const std::string head = SKIN_VS_HEAD;
    gpu.lit = LoadShaderFromMemory((head + LIT_VS_BODY).c_str(), LIT_FS);
    gpu.depth = LoadShaderFromMemory((head + DEPTH_VS_BODY).c_str(), DEPTH_FS);
    gpu.dof = LoadShaderFromMemory(nullptr, DOF_FS);
    gpu.gbuf = LoadShaderFromMemory((head + LIT_VS_BODY).c_str(), GBUF_FS);
    gpu.line = LoadShaderFromMemory(nullptr, LINE_FS);
    gpu.particle = LoadShaderFromMemory(nullptr, PARTICLE_FS);
    if (!IsShaderValid(gpu.lit) || !IsShaderValid(gpu.depth) || !IsShaderValid(gpu.dof) ||
        !IsShaderValid(gpu.gbuf) || !IsShaderValid(gpu.line) || !IsShaderValid(gpu.particle))
        throw std::runtime_error("g3d: shader compilation failed");
    auto loc = [](const char* name) { return GetShaderLocation(gpu.lit, name); };
    gpu.L = { loc("viewPos"), loc("skyColor"), loc("groundColor"), loc("lightCount"), loc("lightType"),
              loc("lightPos"), loc("lightDir"), loc("lightColor"), loc("lightRange"), loc("lightCosInner"),
              loc("lightCosOuter"), loc("lightShadow"), loc("lightBias"), loc("roughness"), loc("metalness"),
              loc("emissive"), loc("unlit"), loc("opacity"), loc("receiveShadow"), loc("shadowAtlas"),
              loc("atlasTexel"), loc("shadowVP"), loc("shadowRect"), loc("shadowParams"), loc("envMap"),
              loc("hasEnv"), loc("envLinear"), loc("envMaxLod"), loc("envIntensity"), loc("envRotation"),
              loc("skybox"), loc("fogColor"), loc("fogNear"), loc("fogFar"), loc("skinned"), loc("boneMatrices"),
              loc("shading"), loc("pxScale"), loc("toonBands"), loc("toonSoftness"), loc("toonHighlight"),
              loc("toonHighlightSize"), loc("rimColor"), loc("rimWidth"), loc("goochWarm"), loc("goochCool"),
              loc("hatchInk"), loc("hatchSpacing"), loc("hatchAngle"), loc("hatchWeight") };
    auto gloc = [](const char* name) { return GetShaderLocation(gpu.gbuf, name); };
    gpu.G = { gloc("objectId"), gloc("lineStyle"), gloc("skinned"), gloc("boneMatrices") };
    auto nloc = [](const char* name) { return GetShaderLocation(gpu.line, name); };
    gpu.N = { nloc("depthTex"), nloc("texel"), nloc("zNear"), nloc("zFar"), nloc("ortho"), nloc("pixelWorld"),
              nloc("pxScale"), nloc("styles"), nloc("maxRadius"), nloc("creaseCos"), nloc("depthThreshold"),
              nloc("wobble"), nloc("wobbleScale"), nloc("seed") };
    auto dloc = [](const char* name) { return GetShaderLocation(gpu.dof, name); };
    gpu.D = { dloc("depthTex"), dloc("texel"), dloc("zNear"), dloc("zFar"), dloc("ortho"), dloc("focus"),
              dloc("aperture"), dloc("maxBlur"), dloc("radStep") };
    gpu.depthSkinned = GetShaderLocation(gpu.depth, "skinned");
    gpu.depthBones = GetShaderLocation(gpu.depth, "boneMatrices");
    // Samplers live on fixed units for the whole run.
    int envUnit = ENV_SLOT, shadowUnit = SHADOW_SLOT, depthUnit = DEPTH_SLOT;
    SetShaderValue(gpu.lit, gpu.L.envMap, &envUnit, SHADER_UNIFORM_INT);
    SetShaderValue(gpu.lit, gpu.L.shadowAtlas, &shadowUnit, SHADER_UNIFORM_INT);
    SetShaderValue(gpu.dof, gpu.D.depthTex, &depthUnit, SHADER_UNIFORM_INT);
    SetShaderValue(gpu.line, gpu.N.depthTex, &depthUnit, SHADER_UNIFORM_INT);
    gpu.material = LoadMaterialDefault();
    gpu.ready = true;
    onGraphicsShutdown(releaseGpu);
}

bool ensureAtlas(int size) {
    if (gpu.atlasFbo && gpu.atlasSize == size) return true;
    if (gpu.atlasFbo) { rlUnloadFramebuffer(gpu.atlasFbo); gpu.atlasFbo = gpu.atlasTex = 0; gpu.atlasSize = 0; }
    gpu.atlasFbo = rlLoadFramebuffer();
    gpu.atlasTex = rlLoadTextureDepth(size, size, false);
    rlFramebufferAttach(gpu.atlasFbo, gpu.atlasTex, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    const bool ok = rlFramebufferComplete(gpu.atlasFbo);
    rlDisableFramebuffer();
    if (!ok) {
        TraceLog(LOG_WARNING, "g3d: shadow framebuffer incomplete; shadows disabled");
        rlUnloadFramebuffer(gpu.atlasFbo);
        gpu.atlasFbo = gpu.atlasTex = 0;
        return false;
    }
    gpu.atlasSize = size;
    return true;
}

DepthTarget& depthTarget(DepthTarget& t, int w, int h) {
    if (t.fbo && t.w == w && t.h == h) return t;
    unloadDepthTarget(t);
    t.fbo = rlLoadFramebuffer();
    t.color = rlLoadTexture(nullptr, w, h, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    t.depth = rlLoadTextureDepth(w, h, false);
    rlFramebufferAttach(t.fbo, t.color, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(t.fbo, t.depth, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    const bool ok = rlFramebufferComplete(t.fbo);
    rlDisableFramebuffer();
    if (!ok) { unloadDepthTarget(t); throw std::runtime_error("g3d: offscreen depth target incomplete"); }
    t.w = w; t.h = h;
    return t;
}
DepthTarget& sceneTarget(int w, int h) { return depthTarget(gpu.sceneTarget, w, h); }
// The line pass reads ids and styles: no filtering between texels.
DepthTarget& lineTarget(int w, int h) {
    const bool fresh = !(gpu.lineTarget.fbo && gpu.lineTarget.w == w && gpu.lineTarget.h == h);
    DepthTarget& t = depthTarget(gpu.lineTarget, w, h);
    if (fresh)
        for (unsigned int id : { t.color, t.depth }) {
            rlTextureParameters(id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
            rlTextureParameters(id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
            rlTextureParameters(id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
            rlTextureParameters(id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
        }
    return t;
}

// Mipmapped, trilinear and repeating: right for material maps and environments alike.
void prepareTexture(Texture2D& tex) {
    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
}

Texture2D texture(const std::string& path) {
    auto it = gpu.textures.find(path);
    if (it != gpu.textures.end()) return it->second;
    Image img = media::loadImage(path);      // resolved like other media; JPG and HDR included
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    if (!IsTextureValid(tex)) throw std::runtime_error("g3d: cannot load texture: " + path);
    prepareTexture(tex);
    gpu.textures[path] = tex;
    return tex;
}

Texture2D environmentTexture(const Scene& s) {
    if (!s.environment.empty()) return texture(s.environment);
    if (!s.environmentGenerator) return Texture2D{};
    auto it = gpu.generated.find(&s);
    if (it != gpu.generated.end()) return it->second;
    Image img = s.environmentGenerator();
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    if (!IsTextureValid(tex)) throw std::runtime_error("g3d: environment generator produced no image");
    prepareTexture(tex);
    gpu.generated[&s] = tex;
    return tex;
}

bool isFloatFormat(int format) {
    return format == PIXELFORMAT_UNCOMPRESSED_R32 || format == PIXELFORMAT_UNCOMPRESSED_R32G32B32 ||
           format == PIXELFORMAT_UNCOMPRESSED_R32G32B32A32 || format == PIXELFORMAT_UNCOMPRESSED_R16 ||
           format == PIXELFORMAT_UNCOMPRESSED_R16G16B16 || format == PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
}

// sRGB 0..255 -> linear 0..1, times k.
Vector3 linear(Color c, float k = 1) {
    auto f = [](unsigned char v) { return std::pow(v / 255.f, 2.2f); };
    return { f(c.r) * k, f(c.g) * k, f(c.b) * k };
}

// ---- scene traversal -----------------------------------------------------------------
// A mesh particle is an item of its emitter object with its own transform, fade and tint.
struct Item {
    Object3D* obj; Matrix world; float viewDepth;
    float alpha = 1;
    Color tint = WHITE;
    bool particle = false;
};
float opacityOf(const Item& it) { return it.obj->material.opacity * it.alpha; }
// An emitter without a geometry: its particles are drawn as billboards after the meshes.
struct Emitting { const Object3D* obj; Matrix world; };

void animate(std::vector<std::unique_ptr<Object3D>>& list, int t) {
    for (auto& o : list) {
        if (o->up) o->up(t, *o);
        animate(o->children, t);
    }
}

void collect(std::vector<std::unique_ptr<Object3D>>& list, const Matrix& parent, std::vector<Item>& out,
             std::vector<Emitting>& emitters, double t) {
    for (auto& o : list) {
        if (!o->visible) continue;
        Matrix local = MatrixMultiply(MatrixMultiply(MatrixScale(o->scale.x, o->scale.y, o->scale.z),
                                                     MatrixRotateXYZ(Vector3Scale(o->rotation, DEG2RAD))),
                                      MatrixTranslate(o->position.x, o->position.y, o->position.z));
        Matrix world = MatrixMultiply(local, parent);
        if (o->emitter && o->geometry) {
            fx::each(*o->emitter, t, [&](const fx::Particle3D& p) {
                const float a = p.alpha * p.color.a / 255.f;
                if (a <= 0.002f || p.size <= 0) return;
                const Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(p.size, p.size, p.size),
                                                               MatrixRotate(p.axis, p.rotation * DEG2RAD)),
                                                MatrixTranslate(p.pos.x, p.pos.y, p.pos.z));
                Item it{ o.get(), MatrixMultiply(m, world), 0 };
                it.alpha = a;
                it.tint = p.color;
                it.particle = true;
                out.push_back(it);
            });
        } else if (o->emitter) emitters.push_back({ o.get(), world });
        else if (o->geometry || o->model) out.push_back({ o.get(), world, 0 });
        collect(o->children, world, out, emitters, t);
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

    const int shading = (int)m.shading;
    SetShaderValue(sh, gpu.L.shading, &shading, SHADER_UNIFORM_INT);
    if (m.shading == Shading::Toon) {
        const Material::Toon& t = m.toon;
        const int bands = std::max(1, t.bands);
        const Vector3 rim = linear(t.rim, t.rim.a / 255.f);
        SetShaderValue(sh, gpu.L.toonBands, &bands, SHADER_UNIFORM_INT);
        SetShaderValue(sh, gpu.L.toonSoftness, &t.softness, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, gpu.L.toonHighlight, &t.highlight, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, gpu.L.toonHighlightSize, &t.highlightSize, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, gpu.L.rimColor, &rim, SHADER_UNIFORM_VEC3);
        SetShaderValue(sh, gpu.L.rimWidth, &t.rimWidth, SHADER_UNIFORM_FLOAT);
    } else if (m.shading == Shading::Gooch) {
        const Vector3 warm = linear(m.gooch.warm), cool = linear(m.gooch.cool);
        SetShaderValue(sh, gpu.L.goochWarm, &warm, SHADER_UNIFORM_VEC3);
        SetShaderValue(sh, gpu.L.goochCool, &cool, SHADER_UNIFORM_VEC3);
    } else if (m.shading == Shading::Hatch) {
        const Material::Hatch& h = m.hatch;
        const Vector3 ink = linear(h.ink);
        const float spacing = std::max(1.f, h.spacing);
        SetShaderValue(sh, gpu.L.hatchInk, &ink, SHADER_UNIFORM_VEC3);
        SetShaderValue(sh, gpu.L.hatchSpacing, &spacing, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, gpu.L.hatchAngle, &h.angle, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, gpu.L.hatchWeight, &h.weight, SHADER_UNIFORM_FLOAT);
    }
}

enum class Pass { Lit, Depth, Gbuf };

// Draws one item with the lit, depth (shadow maps) or line g-buffer shader.
void drawItem(const Item& it, Pass pass) {
    Object3D& o = *it.obj;
    const Shader& sh = pass == Pass::Lit ? gpu.lit : pass == Pass::Depth ? gpu.depth : gpu.gbuf;
    const int bonesLoc = pass == Pass::Lit ? gpu.L.bones : pass == Pass::Depth ? gpu.depthBones : gpu.G.bones;
    const int skinnedLoc = pass == Pass::Lit ? gpu.L.skinned : pass == Pass::Depth ? gpu.depthSkinned : gpu.G.skinned;
    ::Material mat = gpu.material;
    mat.shader = sh;
    if (pass == Pass::Lit) {
        if (it.particle) {
            g3d::Material m = o.material;
            m.opacity = opacityOf(it);
            setMaterial(m);
        } else setMaterial(o.material);
    }
    if (o.material.doubleSided) rlDisableBackfaceCulling(); else rlEnableBackfaceCulling();

    if (o.geometry) {
        mat.maps[MATERIAL_MAP_DIFFUSE].texture = o.material.map.empty() ? whiteTexture() : texture(o.material.map);
        const Color c = o.material.color, k = it.tint;
        mat.maps[MATERIAL_MAP_DIFFUSE].color = { (unsigned char)(c.r * k.r / 255), (unsigned char)(c.g * k.g / 255),
                                                 (unsigned char)(c.b * k.b / 255), c.a };
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

// ---- shadows --------------------------------------------------------------------------
struct Tile { Matrix vp; Vector4 rect, params; };
struct Shadows {
    std::vector<Tile> tiles;
    int lightTile[MAX_LIGHTS];           // first tile of each light, -1 = unshadowed
};

// Loads a light-view camera into rlgl and returns its view-projection; `params` gets
// (near, far, perspective, world size of one texel at unit depth).
Matrix loadTileCamera(Vector3 eye, Vector3 target, Vector3 up, bool perspective, float halfAngleOrExtent,
                      float zNear, float zFar, int tileSize, Vector4& params) {
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    if (perspective) {
        float top = zNear * std::tan(halfAngleOrExtent);
        rlFrustum(-top, top, -top, top, zNear, zFar);
        params = { zNear, zFar, 1, 2 * std::tan(halfAngleOrExtent) / tileSize };
    } else {
        float e = halfAngleOrExtent;
        rlOrtho(-e, e, -e, e, zNear, zFar);
        params = { zNear, zFar, 0, 2 * e / tileSize };
    }
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlMultMatrixf(MatrixToFloat(MatrixLookAt(eye, target, up)));
    return MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
}

Shadows renderShadows(const Scene& s, const std::vector<Item>& items) {
    Shadows out;
    std::fill(std::begin(out.lightTile), std::end(out.lightTile), -1);
    const int count = std::min((int)s.lights.size(), MAX_LIGHTS);
    int tilesNeeded = 0;
    for (int i = 0; i < count; i++) {
        const Light& l = s.lights[(size_t)i];
        if (!l.castShadow) continue;
        int n = l.type == Light::Type::Point ? 6 : 1;
        if (tilesNeeded + n > MAX_TILES) break;
        out.lightTile[i] = tilesNeeded;
        tilesNeeded += n;
    }
    if (!tilesNeeded) return out;
    const int grid = (int)std::ceil(std::sqrt((double)tilesNeeded));
    const int tile = std::clamp(s.shadowMapSize, 64, MAX_ATLAS / grid);
    if (!ensureAtlas(grid * tile)) {
        std::fill(std::begin(out.lightTile), std::end(out.lightTile), -1);
        return out;
    }
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(gpu.atlasFbo);
    rlViewport(0, 0, gpu.atlasSize, gpu.atlasSize);
    rlClearScreenBuffers();
    rlEnableDepthTest();
    rlEnableDepthMask();

    auto renderTile = [&](Vector3 eye, Vector3 target, Vector3 up, bool persp, float a, float zn, float zf) {
        const int index = (int)out.tiles.size(), col = index % grid, row = index / grid;
        Tile t;
        t.vp = loadTileCamera(eye, target, up, persp, a, zn, zf, tile, t.params);
        t.rect = { (float)col / grid, (float)row / grid, 1.f / grid, 1.f / grid };
        rlViewport(col * tile, row * tile, tile, tile);
        for (const Item& it : items)
            if (it.obj->material.castShadow && opacityOf(it) >= 1.f && !it.obj->material.unlit) drawItem(it, Pass::Depth);
        out.tiles.push_back(t);
    };
    for (int i = 0; i < count; i++) {
        if (out.lightTile[i] < 0) continue;
        const Light& l = s.lights[(size_t)i];
        Vector3 dir = Vector3Subtract(l.target, l.position);
        const float dist = std::max(0.01f, Vector3Length(dir));
        dir = Vector3Scale(dir, 1 / dist);
        if (l.type == Light::Type::Directional) {
            // Pulled back so everything within shadowExtent of the target is in front of it.
            const float back = dist + 2.0f * l.shadowExtent;
            renderTile(Vector3Subtract(l.target, Vector3Scale(dir, back)), l.target, safeUp(dir, { 0, 1, 0 }),
                       false, l.shadowExtent, 0.01f, back + 2.0f * l.shadowExtent);
        } else if (l.type == Light::Type::Spot) {
            const float half = std::min(85.f, std::max(l.outerAngle, 1.f) + 2.f) * DEG2RAD;
            renderTile(l.position, l.target, safeUp(dir, { 0, 1, 0 }), true, half, 0.05f, l.range > 0 ? l.range : 200.f);
        } else {
            // Cube faces in the shader's cubeFace() order; slightly wider than 90 degrees for PCF.
            static const Vector3 dirs[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
            static const Vector3 ups[6] = { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };
            for (int f = 0; f < 6; f++)
                renderTile(l.position, Vector3Add(l.position, dirs[f]), ups[f], true, 47.f * DEG2RAD, 0.05f,
                           l.range > 0 ? l.range : 50.f);
        }
    }
    rlDrawRenderBatchActive();
    return out;
}

void setSceneUniforms(const Scene& s, const Camera& cam, const Shadows& sh, Texture2D envTex, float scale) {
    const Shader& lit = gpu.lit;
    const LitLocs& L = gpu.L;
    SetShaderValue(lit, L.pxScale, &scale, SHADER_UNIFORM_FLOAT);
    int count = std::min((int)s.lights.size(), MAX_LIGHTS);
    int type[MAX_LIGHTS] = {};
    Vector3 pos[MAX_LIGHTS] = {}, dir[MAX_LIGHTS] = {}, col[MAX_LIGHTS] = {};
    float range[MAX_LIGHTS] = {}, cin[MAX_LIGHTS] = {}, cosOut[MAX_LIGHTS] = {}, bias[MAX_LIGHTS] = {};
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
        bias[i] = l.shadowBias;
    }
    Vector3 sky = linear(s.skyColor, s.ambient), ground = linear(s.groundColor, s.ambient), fog = linear(s.fogColor);
    SetShaderValue(lit, L.viewPos, &cam.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(lit, L.sky, &sky, SHADER_UNIFORM_VEC3);
    SetShaderValue(lit, L.ground, &ground, SHADER_UNIFORM_VEC3);
    SetShaderValue(lit, L.lightCount, &count, SHADER_UNIFORM_INT);
    SetShaderValueV(lit, L.lightType, type, SHADER_UNIFORM_INT, MAX_LIGHTS);
    SetShaderValueV(lit, L.lightPos, pos, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
    SetShaderValueV(lit, L.lightDir, dir, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
    SetShaderValueV(lit, L.lightColor, col, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
    SetShaderValueV(lit, L.lightRange, range, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
    SetShaderValueV(lit, L.cosInner, cin, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
    SetShaderValueV(lit, L.cosOuter, cosOut, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
    SetShaderValueV(lit, L.lightShadow, sh.lightTile, SHADER_UNIFORM_INT, MAX_LIGHTS);
    SetShaderValueV(lit, L.lightBias, bias, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
    SetShaderValue(lit, L.fogColor, &fog, SHADER_UNIFORM_VEC3);
    SetShaderValue(lit, L.fogNear, &s.fogNear, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L.fogFar, &s.fogFar, SHADER_UNIFORM_FLOAT);
    if (!sh.tiles.empty()) {
        Matrix vps[MAX_TILES];
        Vector4 rects[MAX_TILES], params[MAX_TILES];
        for (size_t i = 0; i < sh.tiles.size(); i++) { vps[i] = sh.tiles[i].vp; rects[i] = sh.tiles[i].rect; params[i] = sh.tiles[i].params; }
        rlEnableShader(lit.id);
        rlSetUniformMatrices(L.shadowVP, vps, (int)sh.tiles.size());
        SetShaderValueV(lit, L.shadowRect, rects, SHADER_UNIFORM_VEC4, (int)sh.tiles.size());
        SetShaderValueV(lit, L.shadowParams, params, SHADER_UNIFORM_VEC4, (int)sh.tiles.size());
        float texel = 1.f / (float)gpu.atlasSize;
        SetShaderValue(lit, L.atlasTexel, &texel, SHADER_UNIFORM_FLOAT);
    }
    int hasEnv = envTex.id ? 1 : 0, envLin = hasEnv && isFloatFormat(envTex.format) ? 1 : 0, sky0 = 0;
    float maxLod = hasEnv ? std::floor(std::log2((float)std::max(envTex.width, envTex.height))) : 0;
    float rot = s.environmentRotation / 360.f;
    SetShaderValue(lit, L.hasEnv, &hasEnv, SHADER_UNIFORM_INT);
    SetShaderValue(lit, L.envLinear, &envLin, SHADER_UNIFORM_INT);
    SetShaderValue(lit, L.envMaxLod, &maxLod, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L.envIntensity, &s.environmentIntensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L.envRotation, &rot, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L.skybox, &sky0, SHADER_UNIFORM_INT);
}

void bindUnit(int slot, unsigned int id) {
    rlActiveTextureSlot(slot);
    if (id) rlEnableTexture(id); else rlDisableTexture();
    rlActiveTextureSlot(0);
}

void drawSkybox(const Camera& cam) {
    if (!gpu.skyCube.vertexCount) gpu.skyCube = GenMeshCube(1, 1, 1);
    int one = 1, zero = 0;
    SetShaderValue(gpu.lit, gpu.L.skybox, &one, SHADER_UNIFORM_INT);
    ::Material mat = gpu.material;
    mat.shader = gpu.lit;
    mat.maps[MATERIAL_MAP_DIFFUSE].texture = whiteTexture();
    mat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    setBones(gpu.L.bones, gpu.L.skinned, gpu.lit, nullptr, false);
    const float size = cam.farPlane;                    // corners stay inside the far plane
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    DrawMesh(gpu.skyCube, mat, MatrixMultiply(MatrixScale(size, size, size),
                                              MatrixTranslate(cam.position.x, cam.position.y, cam.position.z)));
    rlEnableDepthMask();
    SetShaderValue(gpu.lit, gpu.L.skybox, &zero, SHADER_UNIFORM_INT);
}

// Blurs `src` by depth into the currently bound target (2D state, composition units).
void compositeDof(const DepthTarget& src, const Camera& cam, float scale) {
    const auto& D = gpu.D;
    const Camera::DepthOfField& d = cam.dof;
    const float maxBlur = std::max(1.f, d.maxBlur * scale);
    const float radStep = std::max(0.5f, maxBlur * maxBlur / 128.f);        // about 64 taps
    const Vector2 texel{ 1.f / src.w, 1.f / src.h };
    const int ortho = cam.orthographic ? 1 : 0;
    SetShaderValue(gpu.dof, D.texel, &texel, SHADER_UNIFORM_VEC2);
    SetShaderValue(gpu.dof, D.zNear, &cam.nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.dof, D.zFar, &cam.farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.dof, D.ortho, &ortho, SHADER_UNIFORM_INT);
    SetShaderValue(gpu.dof, D.focus, &d.focusDistance, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.dof, D.aperture, &d.aperture, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.dof, D.maxBlur, &maxBlur, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.dof, D.radStep, &radStep, SHADER_UNIFORM_FLOAT);
    bindUnit(DEPTH_SLOT, src.depth);
    // Premultiplied "over" the target (which may already hold the scene background).
    rlSetBlendFactorsSeparate(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    BeginShaderMode(gpu.dof);
    const Texture2D color{ src.color, src.w, src.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    DrawTexturePro(color, { 0, 0, (float)src.w, -(float)src.h }, { 0, 0, src.w / scale, src.h / scale }, { 0, 0 }, 0, WHITE);
    EndShaderMode();
    rlDrawRenderBatchActive();
    bindUnit(DEPTH_SLOT, 0);
}

// ---- ink lines --------------------------------------------------------------------------
// Line styles in use: index 0 is "no lines"; the rest are (colour, target-px width) pairs.
struct LineStyles {
    std::vector<Vector4> table{ Vector4{ 0, 0, 0, 0 } };
    float maxWidth = 0;
    int of(const Scene& s, const g3d::Material& m, float scale) {
        if (m.lineWidth <= 0 || s.lines.width <= 0) return 0;
        const Color c = m.lineColor.a ? m.lineColor : s.lines.color;
        const Vector4 v{ c.r / 255.f, c.g / 255.f, c.b / 255.f, s.lines.width * m.lineWidth * scale };
        for (size_t i = 1; i < table.size(); i++)
            if (table[i].x == v.x && table[i].y == v.y && table[i].z == v.z && table[i].w == v.w) return (int)i;
        if ((int)table.size() == MAX_STYLES) return 1;                 // out of styles: share the first
        table.push_back(v);
        maxWidth = std::max(maxWidth, v.w);
        return (int)table.size() - 1;
    }
};

// Pass 1, with the main pass's camera still loaded: ids, styles and normals of the opaque
// items into the line target. Outside the viewport the id is 255, so its edge draws no line.
void renderLineBuffer(const Scene& s, const std::vector<Item>& items, LineStyles& styles, const DepthTarget& lt,
                      int vx, int vy, int vw, int vh, float scale) {
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(lt.fbo);
    rlViewport(0, 0, lt.w, lt.h);
    rlClearColor(0, 0, 0, 255);
    rlClearScreenBuffers();
    rlEnableScissorTest();
    rlScissor(vx, vy, vw, vh);
    rlClearColor(0, 0, 0, 0);
    rlClearScreenBuffers();
    rlDisableScissorTest();
    rlViewport(vx, vy, vw, vh);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlDisableColorBlend();                       // ids and styles are data: write them unblended
    for (size_t i = 0; i < items.size(); i++) {
        const g3d::Material& m = items[i].obj->material;
        if (opacityOf(items[i]) < 1.f) continue;
        const float id = (float)(i % 254 + 1), style = (float)styles.of(s, m, scale);
        SetShaderValue(gpu.gbuf, gpu.G.objectId, &id, SHADER_UNIFORM_FLOAT);
        SetShaderValue(gpu.gbuf, gpu.G.lineStyle, &style, SHADER_UNIFORM_FLOAT);
        drawItem(items[i], Pass::Gbuf);
    }
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
}

// Pass 2, in 2D pixel state over the bound colour target: finds the edges and draws them.
void compositeLines(const Scene& s, const Camera& cam, const DepthTarget& lt, const LineStyles& styles,
                    int vh, float scale, int t) {
    const auto& N = gpu.N;
    const Scene::Lines& L = s.lines;
    const Vector2 texel{ 1.f / lt.w, 1.f / lt.h };
    const int ortho = cam.orthographic ? 1 : 0;
    const float pixelWorld = cam.orthographic ? cam.orthoHeight / vh : 2.f * std::tan(cam.fov * 0.5f * DEG2RAD) / vh;
    const float maxRadius = styles.maxWidth * 0.5f + 1.f;
    const float creaseCos = std::cos(std::clamp(L.creaseAngle, 0.f, 180.f) * DEG2RAD);
    const float wobble = std::max(0.f, L.wobble), wobbleScale = std::max(1.f, L.wobbleScale);
    const float seed = L.boilEvery > 0 ? (float)(t / L.boilEvery) : 0.f;
    SetShaderValue(gpu.line, N.texel, &texel, SHADER_UNIFORM_VEC2);
    SetShaderValue(gpu.line, N.zNear, &cam.nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.line, N.zFar, &cam.farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.line, N.ortho, &ortho, SHADER_UNIFORM_INT);
    SetShaderValue(gpu.line, N.pixelWorld, &pixelWorld, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.line, N.pxScale, &scale, SHADER_UNIFORM_FLOAT);
    SetShaderValueV(gpu.line, N.styles, styles.table.data(), SHADER_UNIFORM_VEC4, (int)styles.table.size());
    SetShaderValue(gpu.line, N.maxRadius, &maxRadius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.line, N.creaseCos, &creaseCos, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.line, N.depthThreshold, &L.depthThreshold, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.line, N.wobble, &wobble, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.line, N.wobbleScale, &wobbleScale, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.line, N.seed, &seed, SHADER_UNIFORM_FLOAT);
    bindUnit(DEPTH_SLOT, lt.depth);
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    BeginShaderMode(gpu.line);
    const Texture2D gb{ lt.color, lt.w, lt.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    DrawTexturePro(gb, { 0, 0, (float)lt.w, -(float)lt.h }, { 0, 0, (float)lt.w, (float)lt.h }, { 0, 0 }, 0, WHITE);
    EndShaderMode();
    rlDrawRenderBatchActive();
    bindUnit(DEPTH_SLOT, 0);
}

// ---- particles ---------------------------------------------------------------------------
// One billboard (or oriented quad) of the particle pass, in world space.
struct Quad { Vector3 c[4]; Color color; int shape; float depth; };

enum Shape { DOT, GLOW, GLOW_HOT, STREAK, FLAT, STAR, RING };

// `v` turned and scaled by `m`, without its translation.
Vector3 direction(Vector3 v, const Matrix& m) {
    return { m.m0 * v.x + m.m4 * v.y + m.m8 * v.z, m.m1 * v.x + m.m5 * v.y + m.m9 * v.z, m.m2 * v.x + m.m6 * v.y + m.m10 * v.z };
}

Color fogged(const Scene& s, Color c, float dist, bool additive) {
    if (s.fogFar <= s.fogNear) return c;
    const float f = std::clamp((dist - s.fogNear) / (s.fogFar - s.fogNear), 0.f, 1.f);
    if (additive) return { c.r, c.g, c.b, (unsigned char)(c.a * (1 - f)) };   // light fades out in fog
    auto lin = [](unsigned char v) { return std::pow(v / 255.f, 2.2f); };
    auto mix = [&](unsigned char a, unsigned char b) {
        return (unsigned char)std::lround(255 * std::pow(lin(a) + (lin(b) - lin(a)) * f, 1 / 2.2f));
    };
    return { mix(c.r, s.fogColor.r), mix(c.g, s.fogColor.g), mix(c.b, s.fogColor.b), c.a };
}

void particleQuads(const Scene& s, const Camera& cam, const Emitting& em, const Matrix& view, double t,
                   std::vector<Quad>& out) {
    const fx::Emitter3D& e = *em.obj->emitter;
    const Matrix& w = em.world;
    const float unitScale = Vector3Length({ w.m0, w.m1, w.m2 });
    const Vector3 right{ view.m0, view.m4, view.m8 }, up{ view.m1, view.m5, view.m9 };
    auto toWorld = [&](Vector3 p) { return Vector3Transform(p, w); };
    auto depthOf = [&](Vector3 p) { return -Vector3Transform(p, view).z; };
    fx::each(e, t, [&](const fx::Particle3D& p) {
        Color col = withAlpha(p.color, p.alpha);
        if (col.a == 0 || p.size <= 0) return;
        const Vector3 c = toWorld(p.pos);
        const float h = p.size * unitScale / 2;
        col = fogged(s, col, Vector3Distance(c, cam.position), e.additive);
        Quad q{};
        q.color = col;
        q.depth = depthOf(c);
        auto billboard = [&](int shape, float rotation) {
            const float ca = std::cos(rotation * DEG2RAD), sa = std::sin(rotation * DEG2RAD);
            const Vector3 r = Vector3Scale(Vector3Add(Vector3Scale(right, ca), Vector3Scale(up, sa)), h);
            const Vector3 u = Vector3Scale(Vector3Subtract(Vector3Scale(up, ca), Vector3Scale(right, sa)), h);
            q.shape = shape;
            q.c[0] = Vector3Subtract(Vector3Subtract(c, r), u);
            q.c[1] = Vector3Subtract(Vector3Add(c, r), u);
            q.c[2] = Vector3Add(Vector3Add(c, r), u);
            q.c[3] = Vector3Add(Vector3Subtract(c, r), u);
            out.push_back(q);
        };
        switch (e.look) {
            case fx::Look::Dot: billboard(DOT, 0); break;
            case fx::Look::Glow: billboard(e.additive ? GLOW_HOT : GLOW, 0); break;
            case fx::Look::Star: billboard(STAR, p.rotation); break;
            case fx::Look::Ring: billboard(RING, 0); break;
            case fx::Look::Streak: {
                const Vector3 tail = toWorld(fx::positionAt(e, p, e.stretch));
                const Vector3 along = Vector3Subtract(c, tail);
                const float len = Vector3Length(along);
                if (len < h * 0.5f) { billboard(DOT, 0); break; }
                const Vector3 dir = Vector3Scale(along, 1 / len);
                const Vector3 side = Vector3Scale(Vector3Normalize(Vector3CrossProduct(dir, Vector3Subtract(c, cam.position))), h);
                const Vector3 head = Vector3Add(c, Vector3Scale(dir, h));   // a rounded-off tip past the particle
                q.shape = STREAK;
                q.c[0] = Vector3Subtract(tail, Vector3Scale(side, 0.3f));
                q.c[1] = Vector3Subtract(head, side);
                q.c[2] = Vector3Add(head, side);
                q.c[3] = Vector3Add(tail, Vector3Scale(side, 0.3f));
                out.push_back(q);
                break;
            }
            case fx::Look::Square: case fx::Look::Confetti: {
                // A paper quad spanned by the particle's axis and a direction turning about it.
                const Vector3 a = p.axis;
                const Vector3 ref = std::fabs(a.y) < 0.9f ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 };
                const Vector3 e1 = Vector3Normalize(Vector3CrossProduct(a, ref)), e2 = Vector3CrossProduct(a, e1);
                const float ca = std::cos(p.rotation * DEG2RAD), sa = std::sin(p.rotation * DEG2RAD);
                const Vector3 b = Vector3Add(Vector3Scale(e1, ca), Vector3Scale(e2, sa));
                const Vector3 n = direction(Vector3CrossProduct(a, b), w);
                const float hw = h, hh = e.look == fx::Look::Confetti ? h * 0.55f : h;
                const Vector3 A = Vector3Scale(Vector3Normalize(direction(a, w)), hh);
                const Vector3 B = Vector3Scale(Vector3Normalize(direction(b, w)), hw);
                // Lit a little from above, the back of the paper darker.
                const Vector3 nn = Vector3Normalize(n);
                const bool back = Vector3DotProduct(nn, Vector3Subtract(cam.position, c)) < 0;
                const float k = (0.72f + 0.28f * std::fabs(nn.y)) * (back && e.look == fx::Look::Confetti ? 0.78f : 1.f);
                q.color = { (unsigned char)(col.r * k), (unsigned char)(col.g * k), (unsigned char)(col.b * k), col.a };
                q.shape = FLAT;
                q.c[0] = Vector3Subtract(Vector3Subtract(c, A), B);
                q.c[1] = Vector3Subtract(Vector3Add(c, A), B);
                q.c[2] = Vector3Add(Vector3Add(c, A), B);
                q.c[3] = Vector3Add(Vector3Subtract(c, A), B);
                out.push_back(q);
                break;
            }
        }
    });
}

void submit(const std::vector<Quad>& quads) {
    static const float U[4] = { 0, 1, 1, 0 }, V[4] = { 0, 0, 1, 1 };
    for (const Quad& q : quads) {
        rlCheckRenderBatchLimit(4);
        rlBegin(RL_QUADS);
        rlColor4ub(q.color.r, q.color.g, q.color.b, q.color.a);
        // streaks run tail (u = 0) to head (u = 1) along corners 0/3 -> 1/2
        for (int i = 0; i < 4; i++) {
            const float u = q.shape == STREAK ? (i == 1 || i == 2 ? 1.f : 0.f) : U[i];
            const float v = q.shape == STREAK ? (i == 0 || i == 1 ? 0.f : 1.f) : V[i];
            rlTexCoord2f(u + 2.f * q.shape, v);
            rlVertex3f(q.c[i].x, q.c[i].y, q.c[i].z);
        }
        rlEnd();
    }
}

// Depth-tested against the meshes but not writing depth: alpha-blended particles back to
// front, then the additive ones, whose order does not matter.
void drawParticles(const Scene& s, const Camera& cam, const std::vector<Emitting>& emitters, const Matrix& view, double t) {
    std::vector<Quad> blended, added;
    for (const Emitting& em : emitters) particleQuads(s, cam, em, view, t, em.obj->emitter->additive ? added : blended);
    if (blended.empty() && added.empty()) return;
    std::sort(blended.begin(), blended.end(), [](const Quad& a, const Quad& b) { return a.depth > b.depth; });
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(rlGetTextureIdDefault());
    BeginShaderMode(gpu.particle);
    submit(blended);
    if (!added.empty()) {
        rlDrawRenderBatchActive();
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        submit(added);
        rlDrawRenderBatchActive();
        restoreBlend();
    }
    EndShaderMode();
    rlSetTexture(0);
    rlEnableDepthMask();
}

void renderBody(Scene& s, Camera& cam, Rectangle vp, const RenderTexture& target, float scale, int t) {
    ensureGpu();
    std::vector<Item> items;
    std::vector<Emitting> emitters;
    const double ft = frameTime(t);              // particles follow motion-blur subframes
    collect(s.objects, MatrixIdentity(), items, emitters, ft);
    const Texture2D envTex = environmentTexture(s);

    // Background, in 2D before the viewport changes.
    if (s.background.a > 0) {
        DrawRectangleRec(vp, s.background);
        rlDrawRenderBatchActive();
    }

    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    const Shadows shadows = renderShadows(s, items);

    // Main pass, into the target or (for depth of field) a target with a depth texture.
    const int fbW = target.texture.width, fbH = target.texture.height;
    const bool dof = cam.dof.aperture > 0 && cam.dof.maxBlur > 0;
    const DepthTarget* dofTarget = nullptr;
    if (dof) {
        dofTarget = &sceneTarget(fbW, fbH);
        rlEnableFramebuffer(dofTarget->fbo);
        rlViewport(0, 0, fbW, fbH);
        rlClearColor(0, 0, 0, 0);
        rlClearScreenBuffers();
    } else rlEnableFramebuffer(target.id);

    // Inside the viewport (GL's origin is bottom-left).
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

    setSceneUniforms(s, cam, shadows, envTex, scale);
    bindUnit(SHADOW_SLOT, shadows.tiles.empty() ? 0 : gpu.atlasTex);
    bindUnit(ENV_SLOT, envTex.id);
    if (envTex.id && s.showEnvironment) drawSkybox(cam);
    std::vector<Item> clear;
    for (Item& it : items) {
        if (opacityOf(it) < 1.f) {
            Vector3 p = Vector3Transform({ 0, 0, 0 }, MatrixMultiply(it.world, view));
            it.viewDepth = -p.z;
            clear.push_back(it);
        } else drawItem(it, Pass::Lit);
    }
    std::sort(clear.begin(), clear.end(), [](const Item& a, const Item& b) { return a.viewDepth > b.viewDepth; });
    rlDisableDepthMask();
    for (const Item& it : clear) drawItem(it, Pass::Lit);
    rlEnableDepthMask();
    if (!emitters.empty()) drawParticles(s, cam, emitters, view, ft);

    bindUnit(SHADOW_SLOT, 0);
    bindUnit(ENV_SLOT, 0);
    LineStyles styles;
    const DepthTarget* lt = nullptr;
    if (s.lines.enabled) {
        lt = &lineTarget(fbW, fbH);
        renderLineBuffer(s, items, styles, *lt, vx, fbH - vy - vh, vw, vh, scale);
    }
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlViewport(0, 0, fbW, fbH);
    if (lt && styles.table.size() > 1) {
        rlEnableFramebuffer(dofTarget ? dofTarget->fbo : target.id);
        compositeLines(s, cam, *lt, styles, vh, scale, t);
    }
    if (dofTarget) {
        rlEnableFramebuffer(target.id);
        rlScalef(scale, scale, 1.f);                    // the 2D state drawOffscreen bound
        compositeDof(*dofTarget, cam, scale);
    }
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
Object3D* Object3D::add(fx::Emitter3D e) {
    Object3D* o = add();
    o->emitter = std::make_shared<const fx::Emitter3D>(std::move(e));
    return o;
}
Object3D* Object3D::add(fx::Emitter3D e, GeometryPtr g, Material m) {
    Object3D* o = add(std::move(g), std::move(m));
    o->emitter = std::make_shared<const fx::Emitter3D>(std::move(e));
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
Object3D* Scene::add(fx::Emitter3D e) {
    Object3D* o = add();
    o->emitter = std::make_shared<const fx::Emitter3D>(std::move(e));
    return o;
}
Object3D* Scene::add(fx::Emitter3D e, GeometryPtr g, Material m) {
    Object3D* o = add(std::move(g), std::move(m));
    o->emitter = std::make_shared<const fx::Emitter3D>(std::move(e));
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

std::function<Image()> studioEnvironment(Color top, Color horizon, Color bottom, int panels) {
    return [=]() {
        const int w = 512, h = 256;
        Image img = GenImageColor(w, h, BLACK);
        Color* px = (Color*)img.data;
        auto lerp = [](Color a, Color b, float k) {
            return Color{ (unsigned char)(a.r + (b.r - a.r) * k), (unsigned char)(a.g + (b.g - a.g) * k),
                          (unsigned char)(a.b + (b.b - a.b) * k), 255 };
        };
        for (int y = 0; y < h; y++) {
            const float elev = std::cos((y + 0.5f) / h * PI);             // +1 straight up, -1 down
            for (int x = 0; x < w; x++) {
                Color c = elev >= 0 ? lerp(horizon, top, std::sqrt(elev)) : lerp(horizon, bottom, std::sqrt(-elev));
                // Soft panels between 20 and 55 degrees up, evenly spaced around.
                const float az = (x + 0.5f) / w * 2 * PI, alt = std::asin(std::clamp(elev, -1.f, 1.f)) * RAD2DEG;
                float glow = 0;
                for (int p = 0; p < panels; p++) {
                    float d = std::fabs(std::remainder(az - (p + 0.25f) * 2 * PI / panels, 2 * PI));
                    float across = std::clamp(1 - (d - 0.18f) / 0.06f, 0.f, 1.f);
                    float upDown = std::clamp(1 - (std::fabs(alt - 37.5f) - 15) / 4, 0.f, 1.f);
                    glow = std::max(glow, across * upDown);
                }
                px[y * w + x] = lerp(c, WHITE, glow);
            }
        }
        return img;
    };
}

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
        renderBody(s, cam, vp, target, scale, t);
    });
}

} // namespace g3d
} // namespace rmtn
