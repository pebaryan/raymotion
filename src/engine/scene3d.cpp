// scene3d.cpp -- GPU 3D: lazy GPU resources, the lit/shadow/DOF shaders and scene rendering.
#include "rmtn/scene3d.hpp"
#include "rmtn/render.hpp"
#include "rmtn/media.hpp"
#include "rmtn/splat.hpp"
#include "rmtn/draw.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <deque>
#include <cmath>
#include <map>
#include <stdexcept>

// Defined and exported by raylib 6.2 (rmodels.c) but missing from its public header:
// poses the bones without CPU skinning, for skinning in our vertex shader.
extern "C" void UpdateModelAnimationBones(Model model, ModelAnimation anim, float frame);

#include "clouds_glsl.hpp"
#include "lightning_glsl.hpp"
#include "precip_glsl.hpp"
#include "wet_glsl.hpp"
#include "wake_glsl.hpp"
#include "ocean.hpp"
#include "air_glsl.hpp"

namespace rmtn {
namespace g3d {
namespace {

constexpr int MAX_LIGHTS = 8;
constexpr int MAX_BONES = 128;
constexpr int MAX_TILES = 16;            // shadow-atlas tiles per scene
constexpr int MAX_ATLAS = 8192;
constexpr int ENV_SLOT = 13, SHADOW_SLOT = 14, DEPTH_SLOT = 15;   // material maps use units 0..11
// Volumetric clouds. The lit shader reads the cloud shadow map from unit 11 (a material map slot the
// engine never fills); the cloud passes, which are not lit, use units 4..8 (raylib's batch owns 0..3).
constexpr int CLOUD_SLOT = 11, CLOUD_SHAPE_SLOT = 4, CLOUD_DETAIL_SLOT = 5, CLOUD_WEATHER_SLOT = 6, CLOUD_LOW_SLOT = 7, CLOUD_BLUE_SLOT = 8;

// ---- shaders ---------------------------------------------------------------------------
// Attribute and uniform names are raylib's defaults, so DrawMesh binds them.
const char* SKIN_VS_HEAD = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexColor;
in vec4 vertexBoneIndices; in vec4 vertexBoneWeights;
uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;
uniform int skinned;                        // 0 none, 1 linear blend, 2 dual quaternion
uniform mat4 boneMatrices[128];
mat4 skinMatrix() {
    if (skinned == 0) return mat4(1.0);
    return boneMatrices[int(vertexBoneIndices.x)] * vertexBoneWeights.x
         + boneMatrices[int(vertexBoneIndices.y)] * vertexBoneWeights.y
         + boneMatrices[int(vertexBoneIndices.z)] * vertexBoneWeights.z
         + boneMatrices[int(vertexBoneIndices.w)] * vertexBoneWeights.w;
}
// Dual-quaternion skinning (Kavan et al. 2007): the bones' rigid motions are blended as
// rotations, so a bent joint keeps its volume instead of collapsing towards the bone.
vec4 quatOf(mat3 m) {                       // m[c][r]; a rotation without scale
    float t = m[0][0] + m[1][1] + m[2][2];
    if (t > 0.0) { float s = sqrt(t + 1.0) * 2.0;
        return vec4((m[1][2] - m[2][1]) / s, (m[2][0] - m[0][2]) / s, (m[0][1] - m[1][0]) / s, 0.25 * s); }
    if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) { float s = sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0;
        return vec4(0.25 * s, (m[1][0] + m[0][1]) / s, (m[2][0] + m[0][2]) / s, (m[1][2] - m[2][1]) / s); }
    if (m[1][1] > m[2][2]) { float s = sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0;
        return vec4((m[1][0] + m[0][1]) / s, 0.25 * s, (m[2][1] + m[1][2]) / s, (m[2][0] - m[0][2]) / s); }
    float s = sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0;
    return vec4((m[2][0] + m[0][2]) / s, (m[2][1] + m[1][2]) / s, 0.25 * s, (m[0][1] - m[1][0]) / s);
}
vec4 qmul(vec4 a, vec4 b) { return vec4(a.w * b.xyz + b.w * a.xyz + cross(a.xyz, b.xyz), a.w * b.w - dot(a.xyz, b.xyz)); }
vec4 dqReal; vec4 dqDual;
void blendDQ() {
    dqReal = vec4(0.0); dqDual = vec4(0.0);
    vec4 first = vec4(0.0);
    for (int i = 0; i < 4; i++) {
        float w = vertexBoneWeights[i];
        if (w <= 0.0) continue;
        mat4 m = boneMatrices[int(vertexBoneIndices[i])];
        vec4 r = quatOf(mat3(m));
        vec4 d = 0.5 * qmul(vec4(m[3].xyz, 0.0), r);
        if (i == 0) first = r;
        if (dot(r, first) < 0.0) { r = -r; d = -d; }          // the short way round
        dqReal += w * r; dqDual += w * d;
    }
    float n = length(dqReal);
    dqReal /= n; dqDual /= n;
}
vec3 dqRotate(vec3 v) { return v + 2.0 * cross(dqReal.xyz, cross(dqReal.xyz, v) + dqReal.w * v); }
vec3 skinPosition(vec3 p) {
    if (skinned != 2) return (skinMatrix() * vec4(p, 1.0)).xyz;
    blendDQ();
    vec3 t = 2.0 * (dqReal.w * dqDual.xyz - dqDual.w * dqReal.xyz + cross(dqReal.xyz, dqDual.xyz));
    return dqRotate(p) + t;
}
vec3 skinNormal(vec3 n) { return skinned == 2 ? dqRotate(n) : mat3(skinMatrix()) * n; }
// Instancing: each body's world matrix arrives as instanceTransform, with its animation
// (two baked frames, the blend, a brightness) in the w of its columns, which an affine matrix
// leaves at 0, 0, 0 and 1. The baked positions and normals are one texture: frame f's vertex v
// is texel f * vatVerts + v of the first half, its normal the same texel of the second.
in mat4 instanceTransform;
uniform int instanced;
uniform sampler2D vat;
uniform int vatWidth; uniform int vatVerts; uniform int vatRows;
mat4 instM; vec4 instAnim; float instFade = 1.0;
// The third column's w is the frame blend plus 32 (Instance::fade + 1), quantised to 1/32.
void setupInstance() {
    instM = instanceTransform;
    instAnim = vec4(instM[0].w, instM[1].w, fract(instM[2].w), instM[3].w);
    // Whole part: fade + 1 in 1/32 steps (0..64), plus 65 per 1/32 of Instance::fadeFrom, which
    // fadedAway() reads back from the fade as 4 per step (fade itself lies in -1..1).
    float code = floor(instM[2].w), from = floor(code / 65.0);
    instFade = (code - 65.0 * from) / 32.0 - 1.0 + 4.0 * from;
    instM[0].w = 0.0; instM[1].w = 0.0; instM[2].w = 0.0; instM[3].w = 1.0;
}
vec3 vatFetch(float frame, int row0) {
    int i = int(frame) * vatVerts + gl_VertexID;
    return texelFetch(vat, ivec2(i % vatWidth, row0 + i / vatWidth), 0).xyz;
}
vec3 instPosition() { return mix(vatFetch(instAnim.x, 0), vatFetch(instAnim.y, 0), instAnim.z); }
vec3 instNormal() { return normalize(mix(vatFetch(instAnim.x, vatRows), vatFetch(instAnim.y, vatRows), instAnim.z)); }
// The body's colour multiplier (tint x shade), 0..254 a channel for 0..2, packed as the
// integer r + 256 g + 65536 b (exact in a float).
vec3 instTint() {
    float v = instAnim.w;
    return vec3(mod(v, 256.0), mod(floor(v / 256.0), 256.0), floor(v / 65536.0)) / 127.0;
}
)";

// ---- grass fields (g3d::Field): every blade is made here from its tile and its number, the
// instance id. The vertex attribute is the blade template's corner: x -1 / 1 across the blade
// (0 at the tip), y 0 at the root to 1 at the tip. Field::bladeRoot mirrors the placement.
const char* FIELD_VS = R"(
uniform int fieldOn;
uniform mat4 fieldWorld;                 // the object's world matrix
uniform vec4 fieldTile;                  // tile corner x, z (object space), tile size, blades in a full tile
uniform ivec2 fieldKey;                  // the tile's column and row
uniform int fieldSeed;
uniform vec4 fieldArea;                  // min x, min z, max x, max z: where the maps lie
uniform vec2 fieldMapFit;                // map uv scale and offset onto texel centres
uniform sampler2D fieldMaps;             // r ground height, g density, b height multiplier, a arrival
uniform vec4 fieldBlade;                 // height, height variation, width, lean
uniform vec4 fieldWind;                  // direction x, z (unit, object space), sway, gust
uniform vec4 fieldGusts;                 // speed, spacing, time (s), colour variation
uniform vec4 fieldFade;                  // fade start, fade end, dryness, grow
uniform vec3 fieldCam;                   // the viewing camera (world), in the shadow passes too
uniform vec3 fieldRoot; uniform vec3 fieldTip; uniform vec3 fieldDry;     // sRGB 0..1
uniform float fieldBands;                // Field::colorBands (0 = smooth)
uniform float fieldWiden;                // blade width multiplier: 1, more in the shadow pass (thinned blades)
uniform int fieldPick;                   // > 0 in a shadow pass: instance i draws blade hash(i) mod fieldPick, a random share of them (the first n would show the sequence's lattice)
uint fieldHash(uint x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
float fieldRand(uint x) { return float(fieldHash(x) >> 8) / 16777216.0; }
// Point k of the R2 sequence (Roberts 2018) in the unit square, shifted per tile: any first n
// points are spread evenly, so a tile can be thinned by drawing fewer.
vec2 fieldSpot(uint k, uint th) {
    uint x = k * 3242174889u + th, z = k * 2447445413u + fieldHash(th);
    return vec2(float(x >> 8), float(z >> 8)) / 16777216.0;
}
bool fieldVertex(vec3 corner, out vec3 pos, out vec3 nrm, out vec4 col) {
    uint k = fieldPick > 0 ? fieldHash(uint(gl_InstanceID) * 2654435769u + 40503u) % uint(fieldPick) : uint(gl_InstanceID);
    uint th = fieldHash(uint(fieldKey.x) * 73856093u ^ uint(fieldKey.y) * 19349663u ^ uint(fieldSeed) * 83492791u);
    vec2 p = fieldTile.xy + fieldSpot(k, th) * fieldTile.z;
    vec2 a = (p - fieldArea.xy) / (fieldArea.zw - fieldArea.xy);
    if (a.x < 0.0 || a.y < 0.0 || a.x > 1.0 || a.y > 1.0) return false;
    vec4 maps = textureLod(fieldMaps, a * fieldMapFit.x + fieldMapFit.y, 0.0);
    uint h = fieldHash(k * 2654435769u ^ th);
    float r1 = fieldRand(h), r2 = fieldRand(h + 1u), r3 = fieldRand(h + 2u), r4 = fieldRand(h + 3u);
    float r5 = fieldRand(h + 4u), r6 = fieldRand(h + 5u), r7 = fieldRand(h + 6u);
    vec3 rootW = (fieldWorld * vec4(p.x, maps.r, p.y, 1.0)).xyz;
    float far = 1.0 - smoothstep(fieldFade.x, fieldFade.y, distance(rootW, fieldCam));
    // Blades are kept by rank: the first `keep` of the tile stay, and those just past it
    // shrink away over a band instead of popping.
    const float BAND = 0.12;
    float keep = maps.g * far, rank = (float(k) + 0.5) / fieldTile.w;
    float s = clamp((keep * (1.0 + BAND) - rank) / BAND, 0.0, 1.0);
    float grown = clamp((fieldFade.w * 1.15 - maps.a) / 0.15, 0.0, 1.0);     // Field::grow past the arrival
    float H = fieldBlade.x * (1.0 + fieldBlade.y * (2.0 * r1 - 1.0)) * maps.b * s * grown * grown * (3.0 - 2.0 * grown);
    if (H <= 1e-4) return false;
    float W = fieldBlade.z * (0.7 + 0.6 * r2) / max(far, 0.33) * fieldWiden;        // fewer blades far off, each wider
    float yaw = r3 * 6.2831853;
    vec3 side = vec3(cos(yaw), 0.0, sin(yaw));
    vec2 face = vec2(-side.z, side.x), wd = fieldWind.xy;
    // The bend, as the tip's offset over the height: a resting lean, a steady sway downwind
    // with a flutter, and gust fronts travelling downwind (a gust reaches a blade further
    // downwind later).
    float front = 0.5 + 0.5 * sin(6.2831853 * (dot(p, wd) - fieldGusts.x * fieldGusts.z) / max(fieldGusts.y, 0.01));
    float flutter = sin(fieldGusts.z * (5.0 + 3.0 * r4) + r5 * 6.2831853);
    vec2 bend = face * fieldBlade.w * (0.4 + 1.2 * r4)
              + wd * (fieldWind.z * (1.0 + 0.4 * flutter) + fieldWind.w * front * front)
              + vec2(-wd.y, wd.x) * (fieldWind.z * 0.3 * flutter);
    float bl = length(bend);
    if (bl > 0.9) { bend *= 0.9 / bl; bl = 0.9; }
    float c = sqrt(1.0 - bl * bl), t = corner.y;
    // Root at 0, tip at (bend * H, c * H): the blade keeps about its length as it bends.
    vec3 centre = vec3(p.x + bend.x * H * t * t, maps.r + H * (t + t * t * (c - 1.0)), p.y + bend.y * H * t * t);
    vec3 tangent = normalize(vec3(2.0 * bend.x * t, 1.0 + 2.0 * t * (c - 1.0), 2.0 * bend.y * t));
    vec3 objPos = centre + side * (0.5 * W * (1.0 - pow(t, 1.5)) * corner.x);
    vec3 n = normalize(cross(side, tangent) + side * (0.4 * corner.x));     // rounded across the blade
    pos = (fieldWorld * vec4(objPos, 1.0)).xyz;
    n = normalize(mat3(fieldWorld) * n);
    if (dot(n, fieldCam - pos) < 0.0) n = -n;                                // lit on the side seen
    nrm = normalize(mix(n, vec3(0.0, 1.0, 0.0), 0.3 + 0.4 * (1.0 - far)));  // far off, shaded like the ground
    float tc = fieldBands > 1.5 ? min(floor(t * fieldBands), fieldBands - 1.0) / (fieldBands - 1.0) : t;   // flat bands
    vec3 base = mix(fieldRoot, fieldTip, tc) * (1.0 + fieldGusts.w * (r6 - 0.5));
    if (r7 < fieldFade.z) base = mix(base, fieldDry * (0.8 + 0.2 * tc), 0.5 + 0.5 * r2);
    col = vec4(base * (fieldBands > 1.5 ? 1.0 : 0.75 + 0.25 * t), 1.0);
    return true;
}
)";


// ---- water: the wave sum is shared by the surface's vertices and every fragment's caustics
const char* WATER_COMMON = R"(
uniform int waterOn; uniform int waterMode;
uniform float waterLevel; uniform float waterEye; uniform float waterTime; uniform vec2 waterCentre;
uniform int waveCount; uniform vec4 waveA[8]; uniform vec4 waveB[8];
// Height of the sea at p and its slope. A wave is (direction.xy, wavenumber, amplitude) and
// (angular speed, phase, sharpness); sharpness 1 is a sine, more narrows the crests. `foot`
// is the size of what is being sampled (a pixel, a blur): waves shorter than a few of it
// fade out, as they would average out in the sample (0 = all of them).
// The slope variance the fade took out (sum of (1 - fade^2) (k a sharpness)^2 / 2) lands in
// waveLost: the shading adds it to the specular roughness so the average is conserved.
float wHash(vec2 p) {
    p = mod(p, 4099.0);
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
float wNoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(wHash(i), wHash(i + vec2(1.0, 0.0)), f.x), mix(wHash(i + vec2(0.0, 1.0)), wHash(i + vec2(1.0, 1.0)), f.x), f.y);
}
float waveLost = 0.0;
float waveHeightLod(vec2 p, float foot, out vec2 g) {
    float h = 0.0; g = vec2(0.0); waveLost = 0.0;
    for (int i = 0; i < 8; i++) {
        if (i >= waveCount) break;
        float fade = foot > 0.0 ? smoothstep(3.0, 12.0, 6.2832 / (waveA[i].z * foot)) : 1.0;
        float ks = waveA[i].z * waveA[i].w * waveB[i].z;
        waveLost += (1.0 - fade * fade) * 0.5 * ks * ks;
        if (fade <= 0.0) continue;
        float s = waveA[i].z * dot(waveA[i].xy, p) - waveB[i].x * waterTime + waveB[i].y;
        float amp = waveA[i].w;
        if (waterMode == 2) {          // a body is small: no single wavelength may stripe it. Each wave's phase wanders and its height comes and goes in patches
            vec2 pk = p * waveA[i].z;
            s += 4.4 * (wNoise(pk * 0.11 + float(i) * 9.1) - 0.5);
            amp *= 0.15 + 1.7 * smoothstep(0.3, 0.7, wNoise(pk * 0.09 + 31.0 + float(i) * 4.3));
        }
        float u = max(0.5 + 0.5 * sin(s), 1e-4), sh = waveB[i].z;
        h += fade * amp * (2.0 * pow(u, sh) - 1.0);
        g += fade * amp * sh * pow(u, sh - 1.0) * cos(s) * waveA[i].z * waveA[i].xy;
    }
    return h;
}
float waveHeight(vec2 p, out vec2 g) { return waveHeightLod(p, 0.0, g); }

// ---- Water::spectrum (W9): the FFT ocean, as up to four cascades of two textures each: A = (Dx, h, Dz, whitecap),
// B = (slope x, slope z) with respect to the cascade's own lookup coordinate. Every cascade's lookup is bent by a slow
// sine (oceanWarp: amplitude, two wavenumbers) so no patch repeats exactly. oceanLost[c * 8 + l] is the mean square
// slope that mip level l has averaged away from level 0 (the shading puts it back into the glint's roughness).
uniform int oceanOn; uniform int oceanCount; uniform int oceanGeo;
uniform float oceanLambda; uniform float oceanCurve; uniform float oceanTail;
uniform vec4 oceanPatch; uniform vec4 oceanWarp[4]; uniform float oceanLost[32];
uniform vec3 oceanRing;                       // the mesh: inner radius, outer radius, radial spacing as a share of the radius
uniform sampler2D oceanA0; uniform sampler2D oceanB0; uniform sampler2D oceanA1; uniform sampler2D oceanB1;
uniform sampler2D oceanA2; uniform sampler2D oceanB2; uniform sampler2D oceanA3; uniform sampler2D oceanB3;
const float OCEAN_N = 128.0;
float oceanLostAt(int c, float lod) {
    float f = clamp(lod, 0.0, 7.0);
    int i = int(floor(f));
    return mix(oceanLost[c * 8 + i], oceanLost[c * 8 + min(i + 1, 7)], f - float(i));
}
void oceanCascade(sampler2D A, sampler2D B, int c, vec2 p, float lod, bool shade, inout vec3 hd, inout vec2 g, inout float foam, inout float lost) {
    vec4 wp = oceanWarp[c];
    float s1 = wp.y * p.y + 1.7 + 2.3 * float(c), s2 = wp.z * p.x + 0.4 + 1.9 * float(c);
    vec2 q = p + wp.x * vec2(sin(s1), sin(s2));
    vec2 uv = q / oceanPatch[c] + 0.5 / OCEAN_N;
    vec4 va = textureLod(A, uv, lod);
    hd += va.xyz;
    if (shade) {
        vec2 s = textureLod(B, uv, lod).xy;
        float al = wp.x * wp.y * cos(s1), be = wp.x * wp.z * cos(s2);
        g += vec2(s.x + be * s.y, al * s.x + s.y);
        foam = max(foam, va.w);
        lost += oceanLostAt(c, lod);
    }
}
// A mip level that matches a sample spacing of `size` metres in the lookup plane (0 = full detail).
float oceanLod(int c, float size) { return size > 0.0 ? max(log2(2.0 * size * OCEAN_N / oceanPatch[c]), 0.0) : 0.0; }
// The vertices: the cascades the mesh can show, each at the level its ring spacing allows.
void oceanGeometry(vec2 p, float spacing, out float h, out vec2 d) {
    vec3 hd = vec3(0.0); vec2 g = vec2(0.0); float foam = 0.0, lost = 0.0;
    if (oceanGeo > 0) oceanCascade(oceanA0, oceanB0, 0, p, oceanLod(0, spacing), false, hd, g, foam, lost);
    if (oceanGeo > 1 && oceanCount > 1) oceanCascade(oceanA1, oceanB1, 1, p, oceanLod(1, spacing), false, hd, g, foam, lost);
    if (oceanGeo > 2 && oceanCount > 2) oceanCascade(oceanA2, oceanB2, 2, p, oceanLod(2, spacing), false, hd, g, foam, lost);
    if (oceanGeo > 3 && oceanCount > 3) oceanCascade(oceanA3, oceanB3, 3, p, oceanLod(3, spacing), false, hd, g, foam, lost);
    h = hd.y; d = hd.xz;
}
// The pixels: every cascade at the level of the pixel's footprint `foot` (metres of the lookup plane; 0 = no band limit).
void oceanFragment(vec2 p, float foot, out float h, out vec2 g, out float foam, out float lost) {
    vec3 hd = vec3(0.0); g = vec2(0.0); foam = 0.0; lost = 0.0;
    oceanCascade(oceanA0, oceanB0, 0, p, oceanLod(0, foot), true, hd, g, foam, lost);
    if (oceanCount > 1) oceanCascade(oceanA1, oceanB1, 1, p, oceanLod(1, foot), true, hd, g, foam, lost);
    if (oceanCount > 2) oceanCascade(oceanA2, oceanB2, 2, p, oceanLod(2, foot), true, hd, g, foam, lost);
    if (oceanCount > 3) oceanCascade(oceanA3, oceanB3, 3, p, oceanLod(3, foot), true, hd, g, foam, lost);
    h = hd.y;
}
)";

const char* WATER_FS = R"(
uniform vec3 waterAbsorb; uniform vec3 waterScatter; uniform float waterCaustics; uniform float waterFoam;
uniform float waterOpen; uniform float waterInvAmp; uniform float waterGlint; uniform float waterBand; uniform float waterFilter;
uniform float oceanStorm; uniform vec2 oceanWind; uniform float oceanSpray; uniform float oceanVis;       // Water::spectrum: wind heading (unit), spray 0..1, haze distance (0: none)
const float WATER_IOR = 1.333;

// Water::reflections and Water::refraction. reflVP is the viewing camera's view-projection: it finds the pixel of
// a world point. The reflection image was drawn with the scene mirrored about the water level, in the same
// view, so a point's mirror image lands on the pixel that shows the point's reflection.
uniform int reflMode; uniform sampler2D reflTex; uniform mat4 reflVP; uniform vec2 reflSize; uniform float reflDistort; uniform float reflDistortFull;   // reflDistortFull: Water::reflectionDistort before the sea's height scales it (wakes and ripples bend a calm mirror at once)
uniform int refrMode; uniform sampler2D refrColor; uniform sampler2D refrDepth; uniform vec4 refrMap; uniform mat4 refrInvVP;
uniform int reflClip;                      // 1 while the mirrored scene is drawn: what lies under the water is cut away

vec2 camUV(vec3 p) {
    vec4 c = reflVP * vec4(p, 1.0);
    return c.xy / max(c.w, 1e-3) * 0.5 + 0.5;
}
vec3 mirrorY(vec3 p) { return vec3(p.x, 2.0 * waterLevel - p.y, p.z); }
// A reflected ray that points into the water is folded up. The plain fold has a crease where it crosses the horizon, which
// on a steep sea draws thin contour rings around every pit; this one is smooth.
vec3 foldUp(vec3 r) { return normalize(vec3(r.x, sqrt(r.y * r.y + 0.0036), r.z)); }
// The reflection image at uv as linear premultiplied rgb and its coverage (1 where something was drawn),
// fading out towards the image's edge, where the sky takes over.
vec4 planarAt(vec2 uv, float lod) {
    vec4 t = textureLod(reflTex, uv, lod);
    vec3 lin = pow(t.rgb / max(t.a, 1e-3), vec3(2.2)) * t.a;
    float edge = smoothstep(0.0, 0.05, min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y)));
    return vec4(lin, t.a) * edge;
}
float refrDepthAt(vec2 uv) {
    ivec2 sz = textureSize(refrDepth, 0);
    return texelFetch(refrDepth, clamp(ivec2((refrMap.xy + uv * refrMap.zw) * vec2(sz)), ivec2(0), sz - 1), 0).r;
}
vec3 refrWorld(vec2 uv, float d) {
    vec4 c = refrInvVP * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    return c.xyz / c.w;
}
// What lies under the surface at this pixel, seen along the refracted ray: linear rgb after the water has
// absorbed it over the real path, and 1 in .a (0 when the ray meets nothing drawn, so it is deep water).
// A screen-space march: the refracted ray from the surface point is stepped (steps that grow by a tenth,
// about 17 m in all), each point is projected and compared with the opaque depth under it; where the ray
// passes behind the scene it is bisected. A scene sample that lies above the surface (a pole standing in the
// water, the bank) is not an occluder of the underwater ray and is stepped over; a ray that leaves the picture
// meets nothing. The path through the water is the length of the ray to the hit.
bool rayBehind(vec3 Q, float thick, out vec2 uv) {
    vec4 c = reflVP * vec4(Q, 1.0);
    uv = c.xy / max(c.w, 1e-3) * 0.5 + 0.5;
    if (c.w < 1e-3 || uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return false;
    float d = refrDepthAt(uv);
    if (d >= 0.99999) return false;
    vec3 P = refrWorld(uv, d);
    float pw = (reflVP * vec4(P, 1.0)).w;
    return c.w > pw && c.w - pw < thick && P.y < fragPos.y - 0.003;       // behind what is drawn, but not far behind it: an occluder in front of the ray is not a hit
}
bool seeLeft = false;                          // set by seeThrough: the refracted ray left the picture
vec4 seeThrough(vec3 n, vec3 v) {
    seeLeft = false;
    vec3 t = refract(-v, n, 1.0 / WATER_IOR);
    float s0 = 0.0, st = 0.04, lo = 0.0, hi = 0.0, thk = 0.1;
    bool hit = false, left = false;
    vec2 uv, exitUV = vec2(0.5);
    for (int i = 0; i < 40; i++) {
        float s1 = s0 + st;
        thk = (1.6 * st * abs(t.y) + 0.05) / max(v.y, 0.1);       // the ray may be this far behind what is drawn (more at a grazing view, where one pixel spans a long line of sight)
        vec4 c = reflVP * vec4(fragPos + t * s1, 1.0);
        vec2 u = c.xy / max(c.w, 1e-3) * 0.5 + 0.5;
        if (c.w < 1e-3 || u.x < 0.0 || u.x > 1.0 || u.y < 0.0 || u.y > 1.0) {
            // The ray leaves the picture before it meets anything: carry on with the colour at the edge it crossed
            // (the last pixel the ray was over), absorbed over the length it travelled, so no edge forms where
            // rays that still hit meet rays that left.
            if (c.w >= 1e-3) { left = true; seeLeft = true; exitUV = clamp(u, vec2(0.002), vec2(0.998)); hi = s1; }
            break;
        }
        if (rayBehind(fragPos + t * s1, thk, uv)) { lo = s0; hi = s1; hit = true; break; }
        s0 = s1; st *= 1.1;
    }
    if (!hit && !left) return vec4(0.0);
    if (hit) {
        for (int i = 0; i < 6; i++) {
            float mid = 0.5 * (lo + hi);
            if (rayBehind(fragPos + t * mid, thk, uv)) hi = mid; else lo = mid;
        }
        uv = camUV(fragPos + t * hi);
    } else {
        uv = exitUV;
        float d = refrDepthAt(uv);
        if (d >= 0.99999) return vec4(0.0);
        vec3 P = refrWorld(uv, d);
        hi = max(hi, (fragPos.y - P.y) / max(-t.y, 0.05));      // the length down to that depth along the ray, as the floor there would have it
        if (P.y >= fragPos.y - 0.003) return vec4(0.0);                // nothing drawn under the surface there: deep water
    }
    vec4 sc = textureLod(refrColor, refrMap.xy + uv * refrMap.zw, 0.0);
    vec3 lin = pow(sc.rgb / max(sc.a, 1e-3), vec3(2.2));
    vec3 tr = exp(-waterAbsorb * hi);
    return vec4(lin * tr + waterScatter * (1.0 - tr), sc.a);
}

// Length of the segment a-b that lies under a horizontal surface at `lvl`.
float waterPath(vec3 a, vec3 b, float lvl) {
    float ya = a.y - lvl, yb = b.y - lvl, len = distance(a, b);
    if (ya <= 0.0 && yb <= 0.0) return len;
    if (ya > 0.0 && yb > 0.0) return 0.0;
    return len * (ya <= 0.0 ? -ya / (yb - ya) : -yb / (ya - yb));
}
// The surface above a point, or a level it cannot be under when well clear of the waves.
float surfaceAt(vec3 p) {
    if (p.y > waterLevel + waterBand) return waterLevel + waterBand;
    vec2 g;
    return waterLevel + waveHeight(p.xz, g);
}
// Where a sun ray (travelling along dir) that crosses the surface at s, bent by the wave
// there, reaches the horizontal plane at planeY.
vec2 causticLand(vec2 s, vec3 dir, float planeY, float foot) {
    vec2 g;
    float h = waveHeightLod(s, foot, g);
    vec3 r = refract(dir, normalize(vec3(-g.x, 1.0, -g.y)), 1.0 / WATER_IOR);
    return s + r.xz * ((waterLevel + h - planeY) / max(-r.y, 0.05));
}
// How much more or less light reaches p than a flat surface would give: the area of a patch
// of surface over the area its refracted rays cover down here. The patch widens with depth,
// as the light's blur does.
float caustic(vec3 p, vec3 dir) {
    float depth = max(waterLevel - p.y, 0.0);
    vec3 r0 = refract(dir, vec3(0.0, 1.0, 0.0), 1.0 / WATER_IOR);
    vec2 s = p.xz - r0.xz * (depth / max(-r0.y, 0.05));
    float e = 0.03 + 0.05 * depth, foot = e * 0.7;
    for (int i = 0; i < 2; i++) s += p.xz - causticLand(s, dir, p.y, foot);
    vec2 h0 = causticLand(s, dir, p.y, foot);
    vec2 a = causticLand(s + vec2(e, 0.0), dir, p.y, foot) - h0;
    vec2 b = causticLand(s + vec2(0.0, e), dir, p.y, foot) - h0;
    return clamp(e * e / max(abs(a.x * b.y - a.y * b.x), 1e-5), 0.0, 6.0);
}
// What light i is worth at this fragment once it has come through the water: absorbed over
// its path down from the surface, and focused by the waves.
vec3 waterLight(int i) {
    if (waterOn == 0 || lightType[i] != 0 || lightDir[i].y > -0.02) return vec3(1.0);
    float lvl = surfaceAt(fragPos);
    if (fragPos.y >= lvl) return vec3(1.0);
    vec3 r = refract(lightDir[i], vec3(0.0, 1.0, 0.0), 1.0 / WATER_IOR);
    vec3 t = exp(-waterAbsorb * ((lvl - fragPos.y) / max(-r.y, 0.05)));
    float c = waterCaustics > 0.0 ? mix(1.0, caustic(fragPos, lightDir[i]), waterCaustics) : 1.0;
    return t * c;
}
// The water's own fog: what the part of the way to this fragment that is under the surface
// does to its colour.
vec3 waterTint(vec3 color) {
    if (waterOn == 0) return color;
    if (refrMode >= 1 && viewPos.y >= waterEye) return color;     // the surface looks through it: refracted, absorbed there
    float lvl = surfaceAt(fragPos);
    float d = waterPath(viewPos, fragPos, viewPos.y < waterEye || fragPos.y < lvl ? lvl : waterLevel);
    if (d <= 0.0) return color;
    vec3 t = exp(-waterAbsorb * d);
    return color * t + waterScatter * (1.0 - t);
}

float hash21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x), mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x), f.y);
}
// A steep sea reflects a tiny patch of the sky image, magnified many times over, so the cloud noise in it shows up as nested
// contour rings round every pit. A rough sea mirrors a blurred sky anyway (its slope variance spreads every reflection), so
// for the sea the sky is read at a mip level that grows with that variance (oceanSkyLod, from Water::shadingSlopeVariance).
uniform float oceanSkyLod;
vec3 skyAt(vec3 d) {
    if (hasEnv == 1) {
        if (oceanOn == 1) {
            float u = atan(d.z, d.x) / (2.0 * PI) + 0.5 + envRotation;
            float v = acos(clamp(d.y, -1.0, 1.0)) / PI;
            vec3 c = textureLod(envMap, vec2(u, v), min(oceanSkyLod, envMaxLod)).rgb;
            return (envLinear == 1 ? c : pow(max(c, vec3(0.0)), vec3(2.2))) * envIntensity * backdropScale;
        }
        return env(d, 0.0) * backdropScale;
    }
    return mix(groundColor, skyColor, clamp(d.y * 0.5 + 0.5, 0.0, 1.0));
}

)";

const char* WATER_BODY_FS = R"(
// ---- water bodies (Scene::waterBodies, waterMode 2): shore and river flow
// bodyShore: x soft-edge depth (m), y foam width (m), z foam strength. bodyFlow: x flow period (s), y ripple slope,
// z ripple length (m), w 1 for a river. A river's vertices carry (speed, heading, width) in the normal and
// (u across, arc length) in the texture coordinate.
uniform vec4 bodyShore; uniform vec4 bodyFlow; uniform int bodyDebug;      // bodyDebug 2: the surface draws its slope as colour (for tests)
float hashB(vec2 p) {
    p = mod(p, 4099.0);
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
// Value noise and its derivative (quintic): (value, d/dx, d/dy).
vec3 noiseD(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0), du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
    float a = hashB(i), b = hashB(i + vec2(1.0, 0.0)), c = hashB(i + vec2(0.0, 1.0)), d = hashB(i + vec2(1.0, 1.0));
    float k1 = b - a, k2 = c - a, k4 = a - b - c + d;
    return vec3(a + k1 * u.x + k2 * u.y + k4 * u.x * u.y, du * vec2(k1 + k4 * u.y, k2 + k4 * u.x));
}
// The slope (d/ds along the river, d/dc across it) of three octaves of ripples at river coordinates q (metres),
// each fading out when its length nears the pixel footprint; `lost` gets the slope variance the fade took out.
vec2 rippleLayer(vec2 q, float foot, inout float lost) {
    vec2 g = vec2(0.0);
    for (int o = 0; o < 3; o++) {
        float L = bodyFlow.z * exp2(-float(o));
        float fade = smoothstep(2.0, 6.0, L / max(foot, 1e-4));
        float amp = bodyFlow.y * pow(0.7, float(o));
        vec3 nd = noiseD(vec2(q.x / 2.2, q.y) / L + vec2(float(o) * 17.3, float(o) * 5.1));
        g += fade * amp * vec2(nd.y / 2.2, nd.z);
        lost += (1.0 - fade * fade) * 0.5 * amp * amp * 0.6;
    }
    return g;
}
// A river's ripple slope in world x, z by the two-phase flow map: two layers of the same ripples, each carried
// along the channel at the local speed through a cycle of bodyFlow.x seconds and half a cycle apart, cross-faded by
// a triangle weight that is zero where a layer's cycle restarts (so its jump never shows). The weights sum to 1
// and the blend is divided by sqrt(w0^2 + w1^2), so the ripples keep their strength at the middle of the fade.
vec2 riverSlope(float foot, inout float lost) {
    float T = max(bodyFlow.x, 0.2);
    float u = fragTexCoord.x, s = fragTexCoord.y;
    float prof = 1.0 - 0.35 * (2.0 * u - 1.0) * (2.0 * u - 1.0);
    float speed = fragNormal.x * prof;
    float c = (u - 0.5) * fragNormal.z;
    vec2 acc = vec2(0.0), lostAcc = vec2(0.0);
    float w2 = 0.0;
    for (int i = 0; i < 2; i++) {
        float ph = waterTime / T + 0.5 * float(i);
        float cyc = floor(ph), f = ph - cyc;
        float w = 1.0 - abs(2.0 * f - 1.0);
        vec2 jit = vec2(hashB(vec2(cyc * 1.37, float(i) * 5.1 + 1.0)), hashB(vec2(float(i) * 3.3 + 2.0, cyc * 0.71))) * 97.0;
        float l = 0.0;
        acc += w * rippleLayer(vec2(s - speed * f * T, c) + jit, foot, l);
        lostAcc.x += w * w * l;
        w2 += w * w;
    }
    lost += lostAcc.x / max(w2, 1e-3);
    acc *= inversesqrt(max(w2, 1e-3));
    vec2 dir = vec2(cos(fragNormal.y), sin(fragNormal.y));
    return dir * acc.x + vec2(-dir.y, dir.x) * acc.y;
}
// Distance gap between the nearest and second-nearest cell point (Worley F2 - F1), in cell units: small along the
// cell edges, so a threshold on it draws a net of thin filaments.
float worleyEdge(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float f1 = 8.0, f2 = 8.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 c = vec2(float(x), float(y));
            vec2 o = vec2(hashB(i + c), hashB(i + c + 71.7));
            float d = length(c + o - f);
            if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) f2 = d;
        }
    return f2 - f1;
}
// Lace foam on the shallows, 0 to 1: a net of thin filaments (the cell edges of a Worley field, which drifts and
// shifts with time) inside a band that is fw deep, broken into patches by noise. `e` is the body's wave energy
// (0 calm, 1 rough): a calm shore keeps only a few thin broken threads at the edge, a rough one a thicker net
// reaching up the bank. The filament width is faded to its average coverage when it is finer than the pixel.
float shoreFoam(float depth, float fw, float e, vec2 p, float foot, float t) {
    float sw = 0.5 + 0.5 * sin(t * 1.3 + vnoise(p * 0.4) * 9.0);
    float band = 1.0 - smoothstep(0.0, fw, depth - fw * 0.25 * sw);
    if (band <= 0.0) return 0.0;
    float L = 0.26;
    float thick = L * mix(0.07, 0.16, e);                                   // filament width in metres
    vec2 wp = p + (vec2(vnoise(p * 2.3 + 5.0), vnoise(p * 2.3 + 19.0)) - 0.5) * 0.6;       // bend the net so it is not a honeycomb
    thick *= 0.55 + 0.9 * vnoise(p * 6.0 + 3.0);
    float d = worleyEdge(wp / L + vec2(t * 0.05, -t * 0.03)) * L;
    float line = 1.0 - smoothstep(0.5 * thick, thick, d);
    float avg = clamp(thick * 3.0 / L, 0.0, 1.0);
    line = mix(avg, line, smoothstep(0.8, 2.2, thick / max(foot, 1e-4)));
    float patch = smoothstep(0.42, 0.62, vnoise(p * 1.7 + vec2(t * 0.08, 3.0)) + 0.3 * e - 0.1 * (1.0 - band));
    return line * patch * band;
}
)";

const char* WATER_SURFACE_FS = R"(
// The surface itself, as a straight colour and an alpha to blend over what is behind it.
vec4 waterSurface() {
    vec2 g;
    // Derivatives first (uniform flow). Waves a few pixels long or less fade out of the normal; the slope
    // variance they took away (waveLost) widens the glint below, so a far chop is a rough sheen, not a pattern.
    float foot = waterFilter > 0.5 ? max(length(dFdx(fragPos.xz)), length(dFdy(fragPos.xz))) : 0.0;
    bool ocean = oceanOn == 1 && waterMode == 1;
    float h, lost, oceanFoam = 0.0;
    if (ocean) {                                   // the spectrum: textures at the mip level of the footprint in the lookup plane
        float footP = waterFilter > 0.5 ? max(length(dFdx(oceanP)), length(dFdy(oceanP))) : 0.0;
        oceanFragment(oceanP, footP, h, g, oceanFoam, lost);
        lost += oceanTail;                         // the spectrum above the finest grid: roughness the grids cannot hold
        g -= 2.0 * oceanCurve * fragPos.xz;        // the planet's curvature tilts the surface: h = -d^2 / 2R
    } else {
        h = waveHeightLod(fragPos.xz, foot, g);
        lost = waveLost;
    }
    float dist = length(viewPos - fragPos);
    if (rippleRate > 0.0) g += rippleSlope(fragPos.xz, max(length(dFdx(fragPos.xz)), length(dFdy(fragPos.xz))));    // rain rings
    float wakeFoam = 0.0, wakeMag = 0.0;
    if (wakeCount > 0 || rippleCount > 0) {                    // boats' wakes and dropped stones (Scene::wakes, Scene::ripples)
        float fp = max(length(dFdx(fragPos.xz)), length(dFdy(fragPos.xz)));
        vec2 g0 = g;
        if (wakeCount > 0) wakeField(fragPos.xz, fp, g, wakeFoam);
        if (rippleCount > 0) g += rippleField(fragPos.xz, fp);
        wakeMag = length(g - g0);
    }
    bool isBody = waterMode == 2;
    if (isBody && bodyFlow.w > 0.5) g += riverSlope(max(length(dFdx(fragPos.xz)), length(dFdy(fragPos.xz))), lost);
    // A body reads the opaque scene behind its surface: how deep the water is above what lies under it (the
    // surface's height over where the view ray meets the scene), and that scene's colour there.
    float bedDepth = 1.0e3, edgeFade = 1.0;
    vec4 bedSeen = vec4(0.0);                         // straight view of the bed after absorption; a = 1 when there is one
    if (isBody) {
        vec2 uvS = camUV(fragPos);
        float dz = refrDepthAt(uvS);
        if (dz < 0.99999) {
            vec3 P = refrWorld(uvS, dz);
            bedDepth = max(fragPos.y - P.y, 0.0);
            vec4 sc = textureLod(refrColor, refrMap.xy + uvS * refrMap.zw, 0.0);
            if (sc.a > 0.5) {
                vec3 tr = exp(-waterAbsorb * distance(fragPos, P));
                bedSeen = vec4(pow(sc.rgb / sc.a, vec3(2.2)) * tr + waterScatter * (1.0 - tr), 1.0);
            }
        }
        edgeFade = smoothstep(0.0, max(bodyShore.x, 1e-3), bedDepth);
        if (bodyFlow.w > 0.5) edgeFade *= smoothstep(0.0, 0.06, min(fragTexCoord.x, 1.0 - fragTexCoord.x));
    }
    if (bodyDebug == 2) return vec4(pow(clamp(vec3(0.5 + 2.0 * g.x, 0.5 + 2.0 * g.y, 0.5), 0.0, 1.0), vec3(2.2)), 1.0);
    vec3 n = normalize(vec3(-g.x, 1.0, -g.y));
    vec3 v = normalize(viewPos - fragPos);
    if (viewPos.y < waterEye) {                      // from below: a window onto the sky, ringed by mirror
        vec3 nd = -n;
        vec3 t = refract(-v, nd, WATER_IOR);
        vec3 mirror = waterScatter * 0.8, c = mirror;
        if (dot(t, t) > 1e-6) {
            float cosT = abs(dot(t, nd));
            c = mix(skyAt(t), mirror, 0.02 + 0.98 * pow(1.0 - cosT, 5.0));
        }
        vec3 tr = exp(-waterAbsorb * dist);
        return vec4(c * tr + waterScatter * (1.0 - tr), 1.0);
    }
    float ndv = max(dot(n, v), 1e-3);
    // What the fade took out (lost slope variance) is put back in the average, not dropped: the Fresnel term and
    // the reflected sky are averaged over normals tilted towards and away from the viewer by the lost slope's
    // standard deviation (3-point Gauss-Hermite). Flattening a chop must not change how bright it is.
    float sig = sqrt(4.0 * lost);
    vec3 hv = normalize(vec3(v.x, 0.0, v.z) + vec3(1e-5, 0.0, 0.0));
    float F = 0.0;
    vec3 skyRefl = vec3(0.0);
    // The planar reflection: where the mirror image of what the tilted surface turns the ray towards lies on
    // the screen. A reflected ray's displacement is the pixel of the mirrored point at the tilted direction
    // against the one at the flat direction (a point `reach` metres out), scaled by reflDistort and faded
    // with distance. The normals are the Gauss-Hermite ones of the Fresnel average: the lost slope spreads
    // the three taps, and widens the mip they read.
    vec2 uvq[3] = vec2[3](vec2(0.0), vec2(0.0), vec2(0.0));
    float reflLod = 0.0;
    if (reflMode == 1) {
        vec3 base = vec3(fragPos.x, waterLevel, fragPos.z);
        float reach = max(30.0, 1.5 * dist), k = mix(reflDistort, reflDistortFull, smoothstep(0.01, 0.04, wakeMag)) / (1.0 + dist / 250.0);
        vec3 rf = reflect(-v, vec3(0.0, 1.0, 0.0));
        vec2 own = camUV(base), flatUV = camUV(mirrorY(base + rf * reach));
        for (int q = 0; q < 3; q++) {
            float z = q == 0 ? -1.2247 : (q == 1 ? 0.0 : 1.2247);
            vec3 nq = normalize(n + hv * (sig * z));
            vec3 rq = reflect(-v, nq);
            rq = ocean ? foldUp(rq) : vec3(rq.x, abs(rq.y), rq.z);
            uvq[q] = own + (camUV(mirrorY(base + rq * reach)) - flatUV) * k;
        }
        vec2 spread = (uvq[2] - uvq[0]) * 0.5 * reflSize;
        reflLod = max(log2(max(length(spread), 1.0)) - 1.0, 0.0);
        // Where the bending squeezes the image into fewer pixels (the pixel-to-pixel step of the sampled position is more
        // than a texel) the mip it needs is a coarser one: standard mip selection from the footprint, which is what
        // keeps thin cloud outlines from breaking into rings of aliased contours.
        vec2 fx = dFdx(uvq[1]) * reflSize, fy = dFdy(uvq[1]) * reflSize;
        reflLod = max(reflLod, log2(max(max(length(fx), length(fy)), 1.0)));
        // And a surface that throws the image about by many pixels is a rough mirror: the farther the bend takes the sample,
        // the softer it reads (a quarter of the bend, in texels, as blur), so a heavy chop smears the reflected cloud edges
        // into a glow instead of leaving thin rings of them.
        reflLod = max(reflLod, log2(1.0 + 0.25 * length((uvq[1] - own) * reflSize)));
    }
    for (int q = 0; q < 3; q++) {
        float z = q == 0 ? -1.2247 : (q == 1 ? 0.0 : 1.2247), wq = q == 1 ? 0.6667 : 0.1667;
        vec3 nq = normalize(n + hv * (sig * z));
        float c = clamp(dot(nq, v), 1e-3, 1.0), Fq = 0.02 + 0.98 * pow(1.0 - c, 5.0);
        vec3 rq = reflect(-v, nq);
        float ry = rq.y;
        rq = ocean ? foldUp(rq) : vec3(rq.x, abs(rq.y), rq.z);
        F += wq * Fq;
        vec3 sq = skyAt(rq);
        if (reflMode == 1) {
            vec4 pq = planarAt(uvq[q], reflLod);
            sq = pq.rgb + (1.0 - pq.a) * sq;
        }
        if (ocean) sq = mix(sq, waterScatter * 1.4, 0.7 * smoothstep(0.06, -0.12, ry));      // pointing into the next wave: its face, not the sky
        skyRefl += wq * Fq * sq;
    }
    vec3 r = reflect(-v, n);
    r = ocean ? foldUp(r) : vec3(r.x, abs(r.y), r.z);
    float crest = clamp(h * waterInvAmp, -1.0, 1.0);
    vec3 body = waterScatter * (1.0 + 0.5 * max(crest, 0.0));
    float bodyA = waterOpen > 0.0 ? 1.0 - exp(-waterOpen * dot(waterAbsorb, vec3(0.3333)) / max(ndv, 0.25)) : 0.0;
    float glintExp = 2.0 / (2.0 / 902.0 + lost) - 2.0;     // Beckmann: roughness variances add
    vec3 glint = vec3(0.0), foamLight = mix(groundColor, skyColor, 0.5);
    for (int i = 0; i < 8; i++) {
        if (i >= lightCount) break;
        vec3 l; float ndl;
        float a = lightAt(i, n, l, ndl);
        if (a <= 0.0) continue;
        float sp = pow(max(dot(n, normalize(l + v)), 0.0), glintExp) * ((glintExp + 8.0) / 25.1327);
        glint += lightColor[i] * (a * sp * F * 0.25 * waterGlint);
        foamLight += lightColor[i] * (a * ndl);
    }
    float foamA;
    if (ocean) {
        // Whitecaps from the cascade's map (the share of foam, already band-limited by its mip level), broken up into lace
        // by noise that fades to its mean as it gets finer than a pixel; then spray streaks along the wind.
        float nf1 = 1.0 - smoothstep(0.1, 0.4, foot), nf2 = 1.0 - smoothstep(0.03, 0.12, foot);
        float n = 0.5 + nf1 * (vnoise(fragPos.xz * 1.3 + waterTime * 0.07) - 0.5) * 0.9 + nf2 * (vnoise(fragPos.xz * 5.3 - waterTime * 0.11) - 0.5) * 0.7;
        float nf3 = 1.0 - smoothstep(0.012, 0.05, foot);
        n += nf3 * (vnoise(fragPos.xz * 15.0 + waterTime * 0.2) - 0.5) * 0.6;
        // a sharp edge broken into lace: the map's share, pushed through a step whose position the noise moves
        // The map repeats every patch, and a patch holds only a few whitecaps, so a lattice of the same dash would show from
        // above: a slow mask (scales that are not whole patches) takes about half of them out, the rest are made up in the map.
        float msc = 1.0;
        float fm = oceanFoam * smoothstep(0.42, 0.58, vnoise(oceanP * 0.035 * msc) * 0.65 + vnoise(oceanP * 0.083 * msc + 7.3) * 0.35);
        float soft = clamp(foot * 1.2, 0.0, 0.7);                              // from afar the edge is a gradient, not a cut-out
        float fx = fm * (0.35 + 1.3 * n) + 0.35 * (fm - 0.5) * nf1, fw = fwidth(fx) * 1.5;   // edge width follows the pixel: anti-aliased
        if (oceanStorm > 0.0) fx *= mix(1.0, 0.3 + 1.4 * (vnoise(oceanP * 1.3 + 3.7) * 0.6 + vnoise(oceanP * 3.1 + 1.1) * 0.4), oceanStorm * (1.0 - smoothstep(0.2, 1.0, foot)));   // a gale: the whitecaps break into small patches (the mean is kept)
        foamA = waterFoam * mix(0.9, 0.55, soft) * smoothstep(0.2 - 0.15 * soft - fw, 0.5 + 0.6 * soft + fw, fx);
        if (oceanSpray > 0.0 || oceanStorm > 0.0) {
            vec2 wd = oceanWind, cw = vec2(-wd.y, wd.x);
            float along = dot(fragPos.xz, wd) - waterTime * 3.0, across = dot(fragPos.xz, cw);
            across += (vnoise(vec2(along * 0.03, across * 0.02)) - 0.5) * 9.0 + (vnoise(vec2(along * 0.11 + 9.0, across * 0.05)) - 0.5) * 3.0;     // the streaks wander sideways, slowly: not ruled lines
            float sfade = 1.0 - smoothstep(0.35, 1.4, foot);
            float st = vnoise(vec2(across * 2.2, along * 0.05)) * 0.65 + vnoise(vec2(across * 5.5, along * 0.14)) * 0.35;
            float streak = smoothstep(0.5, 0.9, st) * smoothstep(-0.4, 0.5, crest);
            streak = mix(0.18, streak, sfade);
            foamA = max(foamA, waterFoam * oceanSpray * 0.6 * streak);
            if (oceanStorm > 0.0) {                                   // a gale: foam lies in long, narrow, wavering filaments along the wind, down the troughs
                float ac = across;
                float ts = vnoise(vec2(ac * 3.0, along * 0.18)) * 0.55 + vnoise(vec2(ac * 7.5, along * 0.45 + 3.1)) * 0.35 + vnoise(vec2(ac * 13.0, along * 0.6)) * 0.1;
                float wid = 0.6 + 0.2 * (vnoise(vec2(along * 0.02, across * 0.011 + 5.0)) - 0.5);                  // their width varies
                float gapn = vnoise(vec2(along * 0.06 + 2.0, ac * 0.3)) * 0.7 + vnoise(vec2(along * 0.2, ac * 0.9 + 4.0)) * 0.3;   // and they break
                float ew = max(0.9, 3.0 * fwidth(ts)), gw = max(0.6, 3.0 * fwidth(gapn));   // soft edges, wider than a pixel
                float tstreak = smoothstep(wid, wid + ew, ts) * smoothstep(0.34, 0.34 + gw, gapn) * (1.0 - smoothstep(-0.1, 0.45, crest));
                foamA = max(foamA, waterFoam * oceanStorm * 0.4 * mix(0.2, tstreak, sfade * sfade));
            }
        }
    } else {
        float fn = vnoise(fragPos.xz * 2.2 + waterTime * 0.15) * 0.6 + vnoise(fragPos.xz * 7.0) * 0.4;
        foamA = waterFoam * smoothstep(0.55, 0.85, crest + (fn - 0.5) * 0.5);
    }
    foamA = max(foamA, wakeFoam);
    if (bodyDebug == 3) return vec4(vec3(foamA), 1.0);                      // the foam alone (tests)
    if (bodyDebug == 4) {                                                   // the surface height (m), in two bytes: coarse, and the metre's fraction
        float a = (fragPos.y + 8.0) / 16.0;
        return vec4(a, fract(a * 16.0), 0.0, 1.0);
    }
    vec3 premul = skyRefl + (1.0 - F) * bodyA * body + glint;
    float a = clamp(F + (1.0 - F) * bodyA + dot(glint, vec3(0.3333)), 0.0, 1.0);
    if (refrMode == 1 || isBody) {                          // looks through: an opaque surface that shows what is under it itself
        vec4 st = refrMode == 1 ? seeThrough(n, v) : bedSeen;
        if (isBody && (st.a < 0.5 || seeLeft)) st = bedSeen;                   // the ray found nothing on screen (the bed is hidden behind a deck, say): the bed straight under the pixel
        vec3 under = mix(mix(body, st.rgb, st.a), body, bodyA);
        premul = skyRefl + (1.0 - F) * under + glint;
        a = 1.0;
    }
    if (isBody) {                                         // the shore: foam in the shallows, a soft edge into the bank
        float e = bodyShore.w;                             // wave energy: 0 calm, 1 rough (the foam grows with it)
        float shore = bodyShore.z > 0.0 ? bodyShore.z * shoreFoam(bedDepth, bodyShore.y, e, fragPos.xz, foot, waterTime) : 0.0;
        float ramp = smoothstep(0.0, 0.08 * bodyShore.y + 0.5 * foot, bedDepth);
        float fa = max(foamA * 0.8, shore * (0.3 + 0.55 * e)) * ramp;       // foam is never opaque on a calm shore
        vec3 foamCol = vec3(clamp(dot(foamLight, vec3(0.3333)), 0.6, 1.0)) * vec3(1.0, 0.995, 0.98);      // white, lit
        float aw = edgeFade;                               // the water over what is behind, then the foam over the water
        vec3 colB = (foamCol * fa + (1.0 - fa) * premul * aw) / max(1.0 - (1.0 - fa) * (1.0 - aw), 1e-3);
        float aOut = 1.0 - (1.0 - fa) * (1.0 - aw);
        float f2 = fogAmount(fragPos);
        if (f2 > 0.0) colB = mix(colB, fogTint(fragPos), f2);
        return vec4(colB, aOut);
    }
    premul = mix(premul, vec3(0.95) * foamLight, foamA);
    a = mix(a, 1.0, foamA);
    vec3 col = premul / max(a, 1e-3);
    if (ocean && oceanVis > 0.0) {                         // aerial perspective: the far sea becomes the sky's own horizon colour
        float k = 1.0 - exp(-dist / oceanVis);
        col = mix(col, skyAt(normalize(vec3(-v.x, 0.0, -v.z) + vec3(1e-5, 0.0, 0.0))), k);
        a = mix(a, 1.0, k);
    }
    float f = fogAmount(fragPos);
    if (f > 0.0) { col = mix(col, fogTint(fragPos), f); a = mix(a, 1.0, f); }
    if (ocean) col += (fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * (1.5 / 255.0);   // dither: smooth gradients of the dark sea must not show 8-bit contour lines
    return vec4(col, a);
}
)";


// ---- Gaussian splats: one instanced quad per splat, sized and oriented by its projected covariance
const char* SPLAT_VS = R"(#version 330
in vec2 corner;                       // (-1..1)
in float splatIndex;                  // the splat for this instance, in drawing order
uniform sampler2D splatData;          // 6 texels a splat: see uploadSplats
uniform mat4 viewM; uniform mat4 worldM; uniform mat4 projM;
uniform vec2 viewport; uniform vec3 camObj;
uniform float scaleMul; uniform float opacityMul; uniform int shDegree;
uniform vec4 clipPlane;               // world: xyz = unit normal, w = offset; w > 1e8 = none
uniform float clipBand; uniform float scatter;
out vec2 vOff; out vec4 vColor;
const int W = 2048;
vec4 fetch(int i, int k) { int t = i * 6 + k; return texelFetch(splatData, ivec2(t % W, t / W), 0); }
void cull() { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); vOff = vec2(0.0); vColor = vec4(0.0); }
void main() {
    int i = int(splatIndex + 0.5);
    vec4 t0 = fetch(i, 0), t1 = fetch(i, 1), t2 = fetch(i, 2), t3 = fetch(i, 3), t4 = fetch(i, 4), t5 = fetch(i, 5);
    vec4 wp = worldM * vec4(t0.xyz, 1.0);
    float op = t0.w * opacityMul;
    if (clipPlane.w < 1e8) {
        // A front sweeps through: what it has passed is gone, and what is in its band is scattered and fading.
        float keep = clamp(-(dot(wp.xyz, clipPlane.xyz) - clipPlane.w) / max(clipBand, 1e-4), 0.0, 1.0);
        if (keep <= 0.0) { cull(); return; }
        float h = t2.z;
        vec3 away = normalize(vec3(fract(h * 7.13) - 0.5, fract(h * 3.77) - 0.5, fract(h * 11.9) - 0.5) + clipPlane.xyz * 0.9 + vec3(1e-3));
        wp.xyz += away * ((1.0 - keep) * scatter);
        op *= keep;
    }
    vec4 vp = viewM * wp;
    float zc = -vp.z;
    if (zc < 0.05 || op < 0.002) { cull(); return; }
    vec4 clip = projM * vp;
    vec3 ndc = clip.xyz / clip.w;
    mat3 Vr = mat3(viewM) * mat3(worldM);
    mat3 S = mat3(t1.x, t1.y, t1.z, t1.y, t1.w, t2.x, t1.z, t2.x, t2.y) * (scaleMul * scaleMul);
    mat3 Sv = Vr * S * transpose(Vr);
    float fx = projM[0][0] * viewport.x * 0.5, fy = projM[1][1] * viewport.y * 0.5;
    vec3 ra = vec3(fx / zc, 0.0, fx * vp.x / (zc * zc)), rb = vec3(0.0, fy / zc, fy * vp.y / (zc * zc));
    float c00 = dot(ra, Sv * ra) + 0.3, c01 = dot(ra, Sv * rb), c11 = dot(rb, Sv * rb) + 0.3;
    float mid = 0.5 * (c00 + c11), rad = length(vec2(0.5 * (c00 - c11), c01));
    float l1 = mid + rad, l2 = max(mid - rad, 0.1);
    vec2 v = vec2(c01, l1 - c00);
    if (dot(v, v) < 1e-12) v = c00 >= c11 ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec2 v1 = normalize(v), v2 = vec2(-v1.y, v1.x);
    float r1 = min(3.0 * sqrt(l1), 1500.0), r2 = min(3.0 * sqrt(l2), 1500.0);
    vec2 px = ndc.xy * viewport * 0.5;
    if (abs(px.x) > viewport.x * 0.5 + r1 || abs(px.y) > viewport.y * 0.5 + r1) { cull(); return; }
    vec2 offPx = corner.x * r1 * v1 + corner.y * r2 * v2;
    gl_Position = vec4(ndc.xy + offPx * 2.0 / viewport, ndc.z, 1.0);
    vOff = corner * 3.0;
    vec3 col = t3.rgb;
    if (shDegree >= 1) {
        vec3 d = normalize(t0.xyz - camObj);
        const float C1 = 0.4886025119;
        vec3 basis = vec3(-C1 * d.y, C1 * d.z, -C1 * d.x);
        col += vec3(dot(vec3(t3.w, t4.x, t4.y), basis), dot(vec3(t4.z, t4.w, t5.x), basis), dot(vec3(t5.y, t5.z, t2.w), basis));
    }
    vColor = vec4(max(col, vec3(0.0)), op);
}
)";

const char* SPLAT_FS = R"(#version 330
in vec2 vOff; in vec4 vColor;
out vec4 finalColor;
void main() {
    float a = vColor.a * exp(-0.5 * dot(vOff, vOff));
    if (a < 1.0 / 255.0) discard;
    finalColor = vec4(clamp(vColor.rgb, 0.0, 1.0), a);
}
)";

const char* LIT_VS_BODY = R"(
out vec3 fragPos; out vec2 fragTexCoord; out vec3 fragNormal; out vec4 fragColor; flat out float fragFade;
out vec2 oceanP;
void main() {
    fragTexCoord = vertexTexCoord;
    fragFade = 1.0;
    oceanP = vec2(0.0);
    if (waterMode == 1 && oceanOn == 1) {     // the spectrum sea: rings out to the horizon (y = the ring's place 0..1), displaced by the cascades
        float r = oceanRing.x * pow(oceanRing.y / oceanRing.x, vertexPosition.y);
        vec2 p = vertexPosition.xz * r + waterCentre, d;
        float h;
        oceanGeometry(p, r * oceanRing.z, h, d);
        vec2 xz = p + oceanLambda * d;
        vec4 world = vec4(xz.x, waterLevel + h - oceanCurve * dot(xz, xz), xz.y, 1.0);
        oceanP = p;
        fragPos = world.xyz;
        fragNormal = vec3(0.0, 1.0, 0.0);
        fragColor = vec4(1.0);
        gl_Position = mvp * world;
        return;
    }
    if (waterMode == 1) {                     // the sea's surface: a mesh round the camera, rolled by the waves
        vec2 p = vertexPosition.xz + waterCentre, g;
        vec4 world = vec4(p.x, waterLevel + waveHeight(p, g), p.y, 1.0);
        fragPos = world.xyz;
        fragNormal = vec3(0.0, 1.0, 0.0);
        fragColor = vec4(1.0);
        gl_Position = mvp * world;
        return;
    }
    if (waterMode == 2) {                     // a body's surface: flat at its level plus the river's fall, in world coordinates
        vec4 world = vec4(vertexPosition.x, waterLevel + vertexPosition.y, vertexPosition.z, 1.0);
        fragPos = world.xyz;
        fragNormal = vertexNormal;            // a river's (speed, heading, width)
        fragColor = vec4(1.0);
        gl_Position = mvp * world;
        return;
    }
    if (fieldOn == 1) {
        vec3 p, n; vec4 c;
        fragTexCoord = vec2(vertexPosition.x * 0.5 + 0.5, vertexPosition.y);
        if (!fieldVertex(vertexPosition, p, n, c)) {
            fragPos = vec3(0.0); fragNormal = vec3(0.0, 1.0, 0.0); fragColor = vec4(0.0);
            gl_Position = vec4(2.0, 2.0, 2.0, 1.0);                           // outside the clip volume
            return;
        }
        fragPos = p; fragNormal = n; fragColor = c;
        gl_Position = mvp * vec4(p, 1.0);
        return;
    }
    if (instanced != 0) {
        setupInstance();
        vec4 world = instM * vec4(instPosition(), 1.0);
        fragPos = world.xyz;
        fragNormal = mat3(instM) * instNormal();
        fragColor = vertexColor * vec4(instTint(), 1.0);
        fragFade = instFade;
        gl_Position = mvp * world;
        return;
    }
    vec4 pos = vec4(skinPosition(vertexPosition), 1.0);
    vec4 world = matModel * pos;
    fragPos = world.xyz;
    fragNormal = mat3(matNormal) * skinNormal(vertexNormal);
    fragColor = vertexColor;
    gl_Position = mvp * pos;
}
)";

const char* DEPTH_VS_BODY = R"(
out vec2 fragTexCoord; out float fragAlpha; flat out float fragFade;
void main() {
    fragTexCoord = vertexTexCoord;
    fragFade = 1.0;
    fragAlpha = vertexColor.a;
    if (fieldOn == 1) {
        vec3 p, n; vec4 c;
        fragTexCoord = vec2(vertexPosition.x * 0.5 + 0.5, vertexPosition.y);
        fragAlpha = 1.0;
        gl_Position = fieldVertex(vertexPosition, p, n, c) ? mvp * vec4(p, 1.0) : vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }
    if (instanced != 0) {
        setupInstance();
        fragFade = instFade;
        gl_Position = mvp * (instM * vec4(instPosition(), 1.0));
        return;
    }
    gl_Position = mvp * vec4(skinPosition(vertexPosition), 1.0);
}
)";

// Material::alphaCutoff, shared by the lit, depth and g-buffer programs. Alpha grows with the
// mip level, so a card that is mostly holes keeps its coverage far away (after Ben Golus,
// "Anti-aliased Alpha Test", 2017).
const char* CUTOUT_FS = R"(
uniform float alphaCutoff;
// The uv's screen derivatives, taken first thing in main: after a discard (a cut-out's
// edge, a screen-door fade) derivatives and implicit-lod texture reads are undefined, and
// drivers differ there (Mesa's llvmpipe shaded impostor pictures darker than NVIDIA did).
vec2 uvDx, uvDy;
bool cutAway(float a, vec2 uv) {
    if (alphaCutoff <= 0.0) return false;
    vec2 size = vec2(textureSize(texture0, 0));
    vec2 d = max(abs(uvDx * size), abs(uvDy * size));
    float mip = max(log2(max(max(d.x, d.y), 1e-6)), 0.0);
    return a * (1.0 + 0.25 * mip) < alphaCutoff;
}
// Instance::fade as a screen door: fade f > 0 keeps the pixels whose 4x4 Bayer level is
// below f, f < 0 those at or above 1 + f, so a body at u and its stand-in at -u tile exactly.
// A fade above 1 carries a lower bound too: fade + 4 * k keeps the pixels from k / 32 up.
bool fadedAway(float f) {
    if (f == 1.0) return false;
    ivec2 q = ivec2(gl_FragCoord.xy) & 3;
    const float bayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
    float d = (bayer[q.y * 4 + q.x] + 0.5) / 16.0;
    float k = floor((f + 2.0) / 4.0);
    if (k > 0.0) {
        f -= 4.0 * k;
        if (d < k / 32.0) return true;
    }
    return f >= 0.0 ? d >= f : d < 1.0 + f;
}
)";

const char* DEPTH_FS = R"(#version 330
in vec2 fragTexCoord; in float fragAlpha; flat in float fragFade;
uniform sampler2D texture0; uniform vec4 colDiffuse;
out vec4 finalColor;
//@CUTOUT_FS@
void main() {
    uvDx = dFdx(fragTexCoord); uvDy = dFdy(fragTexCoord);
    if (fadedAway(fragFade)) discard;
    if (cutAway(textureGrad(texture0, fragTexCoord, uvDx, uvDy).a * colDiffuse.a * fragAlpha, fragTexCoord)) discard;
    finalColor = vec4(1.0);
}
)";

// Colours arrive as sRGB (texture, colDiffuse, vertex colour) or already linear (scene
// uniforms, converted on the CPU); lighting is linear and the result is re-encoded.
const char* LIT_FS = R"(#version 330
in vec3 fragPos; in vec2 fragTexCoord; in vec3 fragNormal; in vec4 fragColor; flat in float fragFade;
in vec2 oceanP;
uniform sampler2D texture0; uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 skyColor; uniform vec3 groundColor;
uniform int lightCount;
uniform int lightType[8]; uniform vec3 lightPos[8]; uniform vec3 lightDir[8]; uniform vec3 lightColor[8];
uniform float lightRange[8]; uniform float lightCosInner[8]; uniform float lightCosOuter[8];
uniform int lightShadow[8]; uniform float lightBias[8];
uniform float roughness; uniform float metalness; uniform float reflection; uniform vec3 emissive; uniform int unlit; uniform float opacity;
uniform int receiveShadow;
uniform int fieldOn;                                   // drawing a g3d::Field
uniform sampler2D shadowAtlas; uniform float atlasTexel;
uniform mat4 shadowVP[16]; uniform vec4 shadowRect[16]; uniform vec4 shadowParams[16];
uniform sampler2D envMap; uniform int hasEnv; uniform int envLinear; uniform float envMaxLod;
uniform float envIntensity; uniform float envRotation; uniform int skybox;
uniform vec3 fogColor; uniform float fogNear; uniform float fogFar;
uniform int fogMode; uniform float fogDensity; uniform float fogHeight; uniform float fogFalloff; uniform int fogSky;
uniform float backdropScale;
uniform int cloudShadowOn; uniform int cloudShadowLight; uniform sampler2D cloudShadowTex;   // Scene::clouds' ground shadow
uniform vec4 cloudShadowMap; uniform vec3 cloudShadowSun; uniform float cloudShadowFloor;
uniform vec4 clipPlane; uniform vec4 clipEdge;        // Material::clip: xyz normal, w offset; rgb colour (linear), a width
uniform int shading; uniform float pxScale;
uniform int toonBands; uniform float toonSoftness; uniform float toonHighlight; uniform float toonHighlightSize;
uniform vec3 rimColor; uniform float rimWidth;
uniform vec3 goochWarm; uniform vec3 goochCool;
uniform vec3 hatchInk; uniform float hatchSpacing; uniform float hatchAngle; uniform float hatchWeight;
uniform float translucency; uniform vec3 translucencyColor;
uniform sampler2D texture1; uniform int hasRoughnessMap;        // glTF metallic-roughness: g, b
uniform sampler2D texture2; uniform int hasNormalMap; uniform float normalScale;
out vec4 finalColor;
//@WATER_COMMON@
//@CUTOUT_FS@

// A tangent-space normal map without stored tangents: the frame is rebuilt from the screen
// derivatives of position and uv (Schüler, "Normal Mapping Without Precomputed Tangents", 2013).
vec2 texDx, texDy;                                                 // uvDx, uvDy for material reads (impostors: finer)
const float PICTURE_SHARPEN = 0.35;                                  // 2^-mips
vec3 posDx, posDy;                                                 // fragPos's, taken with uvDx and uvDy
vec3 mapNormal(vec3 n, vec3 p, vec2 uv) {                          // p, uv: fragPos, fragTexCoord
    vec3 dp1 = posDx, dp2 = posDy;
    vec2 duv1 = uvDx, duv2 = uvDy;
    vec3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
    vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;
    float m = max(dot(t, t), dot(b, b));
    if (m < 1e-20) return n;                                       // no uv change: nothing to map
    float inv = inversesqrt(m);
    vec3 s = textureGrad(texture2, uv, texDx, texDy).xyz * 2.0 - 1.0;
    s.xy *= normalScale;
    return normalize(mat3(t * inv, b * inv, n) * s);
}

)" R"(
const float PI = 3.14159265;
// An impostor texel's normal is the mean of the leaves it stands for, its length how alike they face.
// Lit, they are a cone about the mean (half-angle acos of the length), not one normal.
float spreadCos = 1.0;
// The cone's mean of max(n.l, 0), c = cos of the mean with l: n.l = A + B cos(phi).
float coneNdl(float c, float cosPsi) {
    float sinPsi = sqrt(max(1.0 - cosPsi * cosPsi, 0.0));
    float A = cosPsi * c, B = sinPsi * sqrt(max(1.0 - c * c, 0.0));
    if (B < 1e-4) return max(A, 0.0);
    if (A >= B) return A;
    if (A <= -B) return 0.0;
    float phi0 = acos(-A / B);
    return (A * phi0 + sqrt(B * B - A * A)) / 3.14159265;
}

// ---- shadows: atlas tiles; params = (near, far, perspective, texel size per unit depth)
float linearDepth(float d, vec4 prm) {
    if (prm.z > 0.5) { float z = d * 2.0 - 1.0; return 2.0 * prm.x * prm.y / (prm.y + prm.x - z * (prm.y - prm.x)); }
    return prm.x + d * (prm.y - prm.x);
}
// `ng` is the geometric normal on the light's side. The point is looked up a little way
// along it (normal offset): at a grazing light the depth under the 3 x 3 kernel changes by
// more than any constant bias allows, and lifting the point by ~1.5 texels times the sine of
// the light's angle clears every sample the kernel reads (no acne bands under a low sun).
float shadowAt(int tile, float bias, float ndl, vec3 ng) {
    vec4 prm = shadowParams[tile], r = shadowRect[tile];
    float sinL = sqrt(max(1.0 - ndl * ndl, 0.0));
    float texelHere = prm.w * (prm.z > 0.5 ? (shadowVP[tile] * vec4(fragPos, 1.0)).w : 1.0);
    vec3 q = fragPos + ng * texelHere * 1.5 * sinL;
    vec4 clip = shadowVP[tile] * vec4(q, 1.0);
    vec3 p = clip.xyz / clip.w * 0.5 + 0.5;
    if (p.z >= 1.0 || p.z <= 0.0 || p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0) return 1.0;
    float zf = prm.z > 0.5 ? clip.w : linearDepth(p.z, prm);
    float texelWorld = prm.w * (prm.z > 0.5 ? zf : 1.0);
    float slope = sinL / max(ndl, 0.1);
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
// ---- fog: linear by distance, or exponential in optical depth (optionally thinning with
// height above fogHeight: the density integrated along the ray in closed form)
float fogAmount(vec3 p) {
    vec3 ray = p - viewPos;
    float d = length(ray);
    if (fogMode == 0) return fogFar > fogNear ? clamp((d - fogNear) / (fogFar - fogNear), 0.0, 1.0) : 0.0;
    if (fogDensity <= 0.0) return 0.0;
    float od = fogDensity * d;
    if (fogFalloff > 0.0) {
        float a = fogFalloff * ray.y;
        float g = abs(a) > 1e-4 ? (1.0 - exp(-a)) / a : 1.0 - 0.5 * a;
        od *= exp(-fogFalloff * (viewPos.y - fogHeight)) * g;
    }
    return fogMode == 1 ? 1.0 - exp(-od) : 1.0 - exp(-od * od);
}
// With the spectrum sea the sky is filled in down to the sea horizon: a direction below the geometric horizon
// (the sliver between it and the dipped sea horizon, and the sea's distance haze) looks at the sky's horizon.
vec3 seaSkyDir(vec3 d) {
    if (oceanOn == 1 && d.y < 0.0) return vec3(d.x, 0.0, d.z) / max(length(d.xz), 1e-4);
    return d;
}
vec3 fogTint(vec3 p) {
    if (fogSky == 1 && hasEnv == 1) return env(seaSkyDir(normalize(p - viewPos)), max(envMaxLod - 7.0, 0.0)) * backdropScale;
    return fogColor;
}
// Karis' analytic approximation of the split-sum environment BRDF.
vec3 envBRDF(vec3 spec, float rough, float ndv) {
    vec4 r = rough * vec4(-1.0, -0.0275, -0.572, 0.022) + vec4(1.0, 0.0425, 1.04, -0.04);
    float a004 = min(r.x * r.x, exp2(-9.28 * ndv)) * r.x + r.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
    return spec * ab.x + ab.y;
}

vec4 encode(vec3 c, float a) { return vec4(pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2)), a); }

// How much of the sun gets through the clouds to p: the shadow map is the transmittance on the plane
// y = cloudShadowMap.w, so p is carried along the sun's rays to that plane first (a hill is shadowed
// where its ray crosses a cloud, not where its foot does). Lit outside the map, fading in at its edge.
float cloudShadowAt(vec3 p) {
    float s = (cloudShadowMap.w - p.y) / cloudShadowSun.y;
    vec2 uv = (p.xz + cloudShadowSun.xz * s - cloudShadowMap.xy) * cloudShadowMap.z + 0.5;
    float edge = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
    if (edge <= 0.0) return 1.0;
    float t = textureLod(cloudShadowTex, uv, 0.0).r;
    return mix(1.0, mix(cloudShadowFloor, 1.0, t), smoothstep(0.0, 0.08, edge));
}
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
    if (spreadCos < 0.999) ndl = coneNdl(clamp(ndl, -1.0, 1.0), spreadCos);
    if (ndl <= 0.0 || atten <= 0.0) return 0.0;
    if (lightShadow[i] >= 0 && receiveShadow == 1) {
        int tile = lightShadow[i] + (lightType[i] == 1 ? cubeFace(fragPos - lightPos[i]) : 0);
        vec3 ng = normalize(fragNormal);
        atten *= shadowAt(tile, lightBias[i], ndl, dot(ng, l) < 0.0 ? -ng : ng);
    }
    if (cloudShadowOn == 1 && i == cloudShadowLight) atten *= cloudShadowAt(fragPos);
    return atten;
}

//@RIPPLE_FS@
//@WATER_FS@
//@WATER_BODY_FS@
//@WAKE_FS@
//@WATER_SURFACE_FS@
//@WET_FS@
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
        vec3 lc = lightColor[i] * waterLight(i);
        color += albedo * lc * band(ndl * a);
        float spot = dot(n, normalize(l + v));
        float cut = 1.0 - toonHighlightSize * toonHighlightSize;
        color += lc * toonHighlight * smoothstep(cut - edge * 0.25, cut + edge * 0.25, spot) * step(0.5, a);
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
    uvDx = dFdx(fragTexCoord); uvDy = dFdy(fragTexCoord);           // before any discard (see uvDx)
    posDx = dFdx(fragPos); posDy = dFdy(fragPos);
    texDx = uvDx; texDy = uvDy;
    if (hasNormalMap == 2) { texDx *= PICTURE_SHARPEN; texDy *= PICTURE_SHARPEN; }
    if (skybox == 1) {
        if (waterOn == 1 && viewPos.y < waterEye) { finalColor = encode(waterScatter, 1.0); return; }
        finalColor = encode(env(seaSkyDir(normalize(fragPos - viewPos)), 0.0) * backdropScale, 1.0);
        return;
    }
    if (waterMode >= 1) { vec4 w = waterSurface(); finalColor = encode(w.rgb, w.a); return; }
    if (reflClip == 1 && fragPos.y < waterLevel) discard;     // the mirrored scene: nothing under the water
    float cutDist = clipPlane.w - dot(fragPos, clipPlane.xyz);     // < 0: beyond the cut
    if (cutDist < 0.0) discard;
    if (fadedAway(fragFade)) discard;
    vec4 base = textureGrad(texture0, fragTexCoord, texDx, texDy) * colDiffuse * fragColor;
    if (alphaCutoff > 0.0) {
        float cutA = hasNormalMap == 2 ? textureGrad(texture0, fragTexCoord, uvDx, uvDy).a * colDiffuse.a * fragColor.a : base.a;
        if (cutAway(cutA, fragTexCoord)) discard;
        base.a = 1.0;                                              // what a cut-out keeps is solid
    }
    if (unlit == 1) { finalColor = vec4(base.rgb, base.a * opacity); return; }
    vec3 albedo = pow(base.rgb, vec3(2.2));
    vec3 n = normalize(fragNormal);
    if ((reflClip == 1 ? gl_FrontFacing : !gl_FrontFacing) && fieldOn == 0) n = -n;   // a field's blades face the camera already; mirrored, the winding is too
    vec3 v = normalize(viewPos - fragPos);
    vec3 ng = n;
    if (hasNormalMap >= 1 && shading == 0) n = mapNormal(n, fragPos, fragTexCoord);
    if (ssrPass == 1 && !(wetOn == 1 && wetMat == 1)) discard;
    if (wetOn == 1 && wetMat == 1) {
        weatherSurface(ng, n, albedo, max(length(posDx.xz), length(posDy.xz)));
        if (ssrPass == 1) {                                         // the second draw: the scene's reflection over flat wet ground
            if (gFilm <= 0.003 || ng.y < 0.9) discard;
            vec3 rr = reflect(-v, n);
            if (rr.y <= 0.02) discard;
            vec4 hit = ssrTrace(fragPos + ng * 0.01, rr);
            float a = wetFresnel(max(dot(n, v), 1e-3), gPuddle) * gFilm * (1.0 - gSnow) * hit.a * mix(0.45, 1.0, gPuddle);
            if (a <= 0.002) discard;
            finalColor = vec4(hit.rgb, clamp(a, 0.0, 1.0));
            return;
        }
        if (wetDebug == 1 && gPuddle > 0.001) { finalColor = vec4(0.5 + n.x, 0.5 + n.z, 0.5, 1.0); return; }
    }

    vec3 color;
    if (shading == 1) color = toon(albedo, n, v);
    else if (shading == 2) color = gooch(albedo, n, v);
    else if (shading == 3) color = hatch(albedo, n);
    else {
        float rough = roughness, metal = metalness;
        if (hasRoughnessMap == 1) {
            vec3 mr = textureGrad(texture1, fragTexCoord, uvDx, uvDy).rgb;
            rough *= mr.g; metal *= mr.b;
        }
        rough = clamp(rough, 0.0, 1.0);
        if (wetOn == 1 && wetMat == 1) {                            // wet ground is smoother, standing water a mirror, snow matte and not metal
            rough = mix(rough * (1.0 - 0.65 * gWet), 0.05, gPuddle);
            rough = mix(rough, 0.9, gSnow);
            metal *= 1.0 - gSnow;
        }
        vec3 specColor = mix(vec3(0.04), albedo, metal);
        vec3 diffColor = albedo * (1.0 - metal);
        float sunVis = 1.0;                                         // baked self-shadow, and the spread of the normals a texel stands for
        if (hasNormalMap == 2) { vec4 bk = textureGrad(texture2, fragTexCoord, texDx, texDy); sunVis = bk.a * (255.0 / 127.0); spreadCos = clamp(length(bk.xyz * 2.0 - 1.0), 0.0, 1.0); }
        if (hasEnv == 1) {
            float ndv = max(dot(n, v), 1e-4);
            vec3 r = reflect(-v, n);
            // Blur by mip level, stopping where the image is still about 16 x 8: smaller mips
            // average the whole sky and ground together.
            float blurLod = max(envMaxLod - 4.0, 0.0);
            color = diffColor * env(n, blurLod) + env(r, rough * blurLod) * envBRDF(specColor, rough, ndv) * reflection;
        } else color = mix(groundColor, skyColor, n.y * spreadCos * 0.5 + 0.5) * albedo;

        float shininess = exp2(10.0 * (1.0 - rough) + 1.0);
        shininess *= spreadCos / (spreadCos + shininess * (1.0 - spreadCos));      // Toksvig: the normals a texel stands for widen its lobe
        for (int i = 0; i < 8; i++) {
            if (i >= lightCount) break;
            vec3 l; float ndl;
            float atten = lightAt(i, n, l, ndl) * (lightShadow[i] >= 0 ? sunVis : 1.0);   // the baked shadow stands in for the shadow map the full tree would have
            if (atten <= 0.0) {
                // Translucency: the light falls on the far side; it comes through diffused,
                // and more of it when looking towards the light (forward scattering).
                if (translucency <= 0.0) continue;
                vec3 lb; float ndlb;
                float back = lightAt(i, -n, lb, ndlb);
                if (back <= 0.0) continue;
                float forward = pow(clamp(dot(v, -lb), 0.0, 1.0), 6.0);
                color += diffColor * translucencyColor * lightColor[i] * waterLight(i) * back
                       * translucency * (lightShadow[i] >= 0 ? sunVis : 1.0) * (0.6 * ndlb + 1.4 * forward);
                continue;
            }
            vec3 h = normalize(l + v);
            float spec = pow(max(dot(n, h), 0.0), shininess) * (shininess + 8.0) / 25.1327;
            color += (diffColor + specColor * spec) * lightColor[i] * waterLight(i) * atten * ndl;
        }
        if (wetOn == 1 && wetMat == 1 && gFilm > 0.0) {             // the film of water: a Fresnel mirror of the sky
            float c = max(dot(n, v), 1e-3);
            float blur = max(envMaxLod - 4.0, 0.0) * 0.75 * (1.0 - gPuddle);
            color += wetSky(reflect(-v, n), blur) * (wetFresnel(c, gPuddle) * gFilm * (1.0 - gSnow));
        }
    }
    color += emissive;
    if (clipEdge.a > 0.0) color += clipEdge.rgb * (1.0 - smoothstep(0.0, clipEdge.a, cutDist));
    color = waterTint(color);
    float f = waterOn == 1 && viewPos.y < waterEye ? 0.0 : fogAmount(fragPos);
    if (f > 0.0) color = mix(color, fogTint(fragPos), f);
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
    else if (shape == 7) {                                                    // a line's stroke: soft only across
        float w = max(fwidth(p.y), 1e-3);
        a = 1.0 - smoothstep(1.0 - w, 1.0, abs(p.y));
    } else if (shape == 8) {                                                  // an arrowhead, base at u = 0
        float d = abs(p.y) - (1.0 - uv.x), w = max(fwidth(d), 1e-3);
        a = 1.0 - smoothstep(-w, 0.0, d);
    }
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
uniform float tanTilt; uniform float tanHalfFov; uniform vec2 viewY;   // viewY: the viewport's bottom and height in uv
out vec4 finalColor;
float depthAt(vec2 uv) {
    float d = texture(depthTex, uv).r;
    if (ortho == 1) return zNear + d * (zFar - zNear);
    float z = d * 2.0 - 1.0;
    return 2.0 * zNear * zFar / (zFar + zNear - z * (zFar - zNear));
}
// The distance in focus along the ray through uv: where it meets the tilted focal plane.
float focusAt(vec2 uv) {
    if (ortho == 1 || tanTilt == 0.0) return focus;
    float y = ((uv.y - viewY.x) / viewY.y * 2.0 - 1.0) * tanHalfFov;
    return focus / max(1.0 - tanTilt * y, 0.05);
}
float blurSize(float z, vec2 uv) { float f = focusAt(uv); return clamp(aperture * abs(z - f) / max(z, 1e-4), 0.0, 1.0) * maxBlur; }
void main() {
    float cz = depthAt(fragTexCoord), cs = blurSize(cz, fragTexCoord);
    vec4 sum = texture(texture0, fragTexCoord);
    float tot = 1.0, radius = radStep, ang = 0.0;
    for (int i = 0; i < 512; i++) {
        if (radius >= maxBlur) break;
        vec2 tc = clamp(fragTexCoord + vec2(cos(ang), sin(ang)) * texel * radius, 0.5 * texel, 1.0 - 0.5 * texel);
        vec4 sc = texture(texture0, tc);
        float sz = depthAt(tc), ss = blurSize(sz, tc);
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
in vec3 fragPos; in vec2 fragTexCoord; in vec3 fragNormal; in vec4 fragColor; flat in float fragFade;
uniform mat4 matView; uniform float objectId; uniform float lineStyle;
uniform sampler2D texture0; uniform vec4 colDiffuse;
out vec4 finalColor;
//@CUTOUT_FS@
vec2 octEncode(vec3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    vec2 e = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
    return e * 0.5 + 0.5;
}
void main() {
    uvDx = dFdx(fragTexCoord); uvDy = dFdy(fragTexCoord);
    if (fadedAway(fragFade)) discard;
    if (cutAway(textureGrad(texture0, fragTexCoord, uvDx, uvDy).a * colDiffuse.a * fragColor.a, fragTexCoord)) discard;
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

// The sky, drawn into an equirectangular image (u: azimuth as env() reads it, v: from the
// zenith at the bottom row of the target, which is texture row 0). Linear colours in,
// sRGB out, the same encoding as an environment image.
const char* SKY_FS = R"(#version 330
out vec4 finalColor;
uniform vec2 size;
uniform vec3 zenith; uniform vec3 horizon; uniform vec3 ground; uniform float curve;
uniform vec3 sunDir; uniform vec3 sunColor; uniform float sunCos; uniform float sunGlow;
uniform vec3 haze; uniform float hazeAmount;
uniform float stars; uniform float clouds; uniform vec3 cloudColor; uniform vec3 cloudShade;
uniform float cloudScale; uniform vec2 cloudOffset; uniform float frame; uniform float seed;
const float PI = 3.14159265;
float hash(vec3 p) { p = fract(p * 0.3183099 + vec3(0.71, 0.113, 0.419) + seed * 0.0137); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }
float vnoise(vec2 x) {
    vec2 i = floor(x), f = fract(x), u = f * f * (3.0 - 2.0 * f);
    float a = hash(vec3(i, 0.0)), b = hash(vec3(i + vec2(1, 0), 0.0)), c = hash(vec3(i + vec2(0, 1), 0.0)), d = hash(vec3(i + vec2(1, 1), 0.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}
float fbm(vec2 x) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { v += a * vnoise(x); x = x * 2.03 + vec2(17.1, 9.3); a *= 0.5; }
    return v;
}
void main() {
    vec2 uv = gl_FragCoord.xy / size;
    float phi = (uv.x - 0.5) * 2.0 * PI, theta = uv.y * PI;
    vec3 d = vec3(cos(phi) * sin(theta), cos(theta), sin(phi) * sin(theta));
    vec3 s = normalize(sunDir);
    float h = d.y, toSun = dot(d, s);
    // The gradient, and the horizon warmed under the sun (more as the sun gets low).
    float low = 1.0 - clamp(s.y * 2.5, 0.0, 1.0);
    float side = pow(max(dot(normalize(vec3(d.x, 0.0, d.z) + 1e-5), normalize(vec3(s.x, 0.0, s.z) + 1e-5)), 0.0), 3.0);
    vec3 hor = mix(horizon, haze, hazeAmount * side * (0.35 + 0.65 * low));
    vec3 c = h >= 0.0 ? mix(hor, zenith, pow(h, curve)) : mix(hor, ground, clamp(pow(-h * 6.0, 0.6), 0.0, 1.0));
    c += haze * hazeAmount * 0.6 * pow(max(toSun, 0.0), 6.0) * pow(1.0 - abs(h), 4.0);
    // Stars: one per cell of a cube-ish grid on the sphere, twinkling; gone near the horizon.
    if (stars > 0.0 && h > 0.0) {
        vec3 g = d * 220.0, cell = floor(g);
        float r = hash(cell);
        vec3 at = cell + 0.5 + (vec3(hash(cell + 3.1), hash(cell + 7.7), hash(cell + 1.3)) - 0.5) * 0.7;
        float dist = length(g - at);
        float on = step(1.0 - 0.18 * stars, r) * smoothstep(0.32, 0.0, dist);
        float tw = 0.7 + 0.3 * sin(frame * (0.05 + 0.1 * hash(cell + 9.0)) + r * 40.0);
        c += vec3(0.9, 0.93, 1.0) * on * tw * stars * smoothstep(0.0, 0.25, h) * (0.4 + 0.6 * hash(cell + 5.0));
    }
    // Clouds on a flat layer overhead, lit from the sun's side, thinning to the horizon.
    if (clouds > 0.0 && h > 0.0) {
        vec2 q = d.xz / (h + 0.12) * 1.6 * cloudScale + cloudOffset * 60.0;
        float n = fbm(q);
        float cov = smoothstep(1.0 - clouds * 0.9 - 0.12, 1.0 - clouds * 0.9 + 0.18, n);
        float lit = clamp(0.5 + 0.8 * (fbm(q + s.xz * 0.6) - n) * -4.0 + 0.3 * toSun, 0.0, 1.0);
        vec3 cc = mix(cloudShade, cloudColor, lit) * (0.55 + 0.45 * max(s.y + 0.25, 0.0));
        cc += sunColor * sunGlow * 0.5 * pow(max(toSun, 0.0), 12.0) * (1.0 - cov * 0.5);
        c = mix(c, cc, cov * smoothstep(0.0, 0.18, h));
    }
    // The sun's glow, then the disc (hidden below the horizon).
    c += sunColor * sunGlow * (0.25 * pow(max(toSun, 0.0), 48.0) + 0.6 * pow(max(toSun, 0.0), 600.0));
    if (sunCos < 1.0) c = mix(c, sunColor * 1.4, smoothstep(sunCos - 0.00002, sunCos + 0.00002, toSun) * smoothstep(-0.01, 0.01, h));
    finalColor = vec4(pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);
}
)";

struct SkyLocs { int size, zenith, horizon, ground, curve, sunDir, sunColor, sunCos, sunGlow, haze, hazeAmount, stars,
                 clouds, cloudColor, cloudShade, cloudScale, cloudOffset, frame, seed; };

struct LitLocs {
    int viewPos, sky, ground, lightCount, lightType, lightPos, lightDir, lightColor, lightRange,
        cosInner, cosOuter, lightShadow, lightBias, roughness, metalness, emissive, unlit, opacity,
        receiveShadow, shadowAtlas, atlasTexel, shadowVP, shadowRect, shadowParams, envMap, hasEnv,
        envLinear, envMaxLod, envIntensity, envRotation, skybox, fogColor, fogNear, fogFar, fogMode, fogDensity,
        fogHeight, fogFalloff, fogSky, backdropScale, skinned, bones,
        shading, pxScale, toonBands, toonSoftness, toonHighlight, toonHighlightSize, rimColor, rimWidth,
        goochWarm, goochCool, hatchInk, hatchSpacing, hatchAngle, hatchWeight, reflection,
        translucency, translucencyColor, hasRoughnessMap, hasNormalMap, normalScale;
};
struct WaterLocs { int on, mode, level, eye, time, centre, count, waveA, waveB, absorb, scatter, caustics, foam,
                   open, invAmp, glint, band, filter,
                   reflMode, reflTex, reflVP, reflSize, reflDistort, refrMode, refrColor, refrDepth, refrMap, refrInvVP, reflClip; };
constexpr int REFL_SLOT = 8, REFR_COLOR_SLOT = 9, REFR_DEPTH_SLOT = 10;    // the water's textures (the lit shader reads maps 0..2)
constexpr int WET_OCC_SLOT = 7;          // the rain's depth map from above, for the ground's shelter test
struct ClipLocs { int plane, edge; };
struct SplatLocs { int data, view, world, proj, viewport, camObj, scaleMul, opacityMul, shDegree, clipPlane, clipBand, scatter, corner, index; };
constexpr int SPLAT_SLOT = 13;           // a splat cloud's texture
struct DofLocs { int depthTex, texel, zNear, zFar, ortho, focus, aperture, maxBlur, radStep, tanTilt, tanHalfFov, viewY; };
struct GbufLocs { int objectId, lineStyle, skinned, bones; };
// The instancing uniforms of one program (lit, depth or g-buffer): the program is the same
// one, drawn through a copy of its Shader whose model-matrix location is the attribute.
struct InstLocs { int instanced = -1, vat = -1, width = -1, verts = -1, rows = -1; };
constexpr int VAT_SLOT = 12;             // the baked-animation texture (material maps use 0..11)
// A field's uniforms in one program (see FIELD_VS); its maps share the VAT_SLOT unit.
struct FieldLocs { int on = -1, world = -1, tile = -1, key = -1, seed = -1, area = -1, mapFit = -1, maps = -1,
                   blade = -1, wind = -1, gusts = -1, fade = -1, cam = -1, root = -1, tip = -1, dry = -1, bands = -1, widen = -1, pick = -1; };
constexpr int FIELD_MAX_SEGMENTS = 8;
struct LineLocs { int depthTex, texel, zNear, zFar, ortho, pixelWorld, pxScale, styles, maxRadius, creaseCos,
                  depthThreshold, wobble, wobbleScale, seed; };
constexpr int MAX_STYLES = 64;

// An offscreen target with a sampleable depth texture (for depth of field).
struct DepthTarget { unsigned int fbo = 0, color = 0, depth = 0; int w = 0, h = 0; };

// Volumetric clouds (Scene::clouds): shaders and baked noise made lazily at the first cloud frame.
struct CloudGpu {
    Shader bake{}, march{}, shadow{}, up{};
    RenderTexture shape{}, detail{};            // the noise volumes as tiled atlases
    Texture2D weather{};                        // the coverage map (equalised, tiling)
    Texture2D blue{};                           // a 64 x 64 blue-noise tile (void and cluster)
    bool ready = false;
    unsigned lowFbo = 0, lowTex = 0, fullFbo = 0, fullTex = 0;   // the march's image and the upsampled one (RGBA16F)
    int lowW = 0, lowH = 0, fullW = 0, fullH = 0;
    RenderTexture shadowRT{};                   // the ground shadow map: the sun's transmittance
    int shadowRes = 0;
    // This frame's shadow, for the lit shader (set by renderCloudShadow, off otherwise).
    bool shadowOn = false;
    Vector4 shadowMap{ 0, 0, 0, 0 };            // centre x, z, 1 / extent, the plane's y
    Vector3 shadowSun{ 0, 1, 0 };
    int shadowLight = -1;
    float shadowFloor = 0.1f;
};
struct CloudLitLocs { int on = -1, light = -1, tex = -1, map = -1, sun = -1, floor = -1; };

// Light in the air (Scene::volumetric, Scene::aerial): the march's shader and image, and the composite shader.
struct AirGpu {
    Shader march{}, comp{};
    unsigned lowFbo = 0, lowTex = 0;
    int lowW = 0, lowH = 0;
    float lastMs = 0;
};

struct Gpu {
    bool ready = false;
    CloudGpu cl;
    AirGpu air;
    CloudLitLocs cll;
    Shader lit{}, depth{}, dof{}, gbuf{}, line{}, particle{}, sky{}, bolt{}, rainbow{};
    LitLocs L{};
    WaterLocs W{};
    ClipLocs C{};
    Shader splat{};
    SplatLocs P{};
    struct SplatGpu {
        unsigned int tex = 0, vao = 0, cornerVbo = 0, indexVbo = 0;
        size_t capacity = 0, drawn = 0;
        std::shared_ptr<const splat::Cloud> keep;
        float lastView[16] = {};         // what the order was sorted for
        bool sorted = false;
    };
    std::map<const splat::Cloud*, SplatGpu> splats;
    std::unique_ptr<Object3D> waterObj;  // the sea's surface, drawn with the transparent objects
    std::unique_ptr<Object3D> oceanObj;  // and the spectrum sea's (Water::spectrum): more rings, out to the horizon
    struct OceanGpu { unsigned tex[4][2] = {}; uint64_t id = 0; int cascades = 0; } ocean;   // the cascades' textures, and the frame they hold
    struct BodyMesh { std::unique_ptr<Object3D> obj; int used = 0; };
    std::map<uint64_t, BodyMesh> bodyMeshes;    // the meshes of Scene::waterBodies, by shape (waterBodyKey)
    SkyLocs S{};
    RenderTexture skyTarget{};          // the procedural sky, remade each frame
    DofLocs D{};
    GbufLocs G{};
    LineLocs N{};
    int depthSkinned = -1, depthBones = -1;
    int cutoff[3] = { -1, -1, -1 };      // alphaCutoff in the lit, depth and g-buffer programs
    int wetMat = -1;                     // the lit program's wetMat: whether this material takes the ground's weather
    Shader inst[3]{};                    // lit, depth, gbuf, for DrawMeshInstanced
    std::array<int, 32> instLocs[3]{};
    InstLocs il[3];
    int instState[3] = { -1, -1, -1 };   // the `instanced` uniform last set per program
    struct Vat { unsigned int tex = 0; int width = 0, rows = 0; std::shared_ptr<AnimatedMesh> keep; };
    std::map<const AnimatedMesh*, Vat> vat;
    FieldLocs F[3];
    // Blade templates for 1..FIELD_MAX_SEGMENTS segments, one after another in one buffer.
    unsigned int fieldVao = 0, fieldVbo = 0;
    int fieldFirst[FIELD_MAX_SEGMENTS + 1] = {}, fieldCount[FIELD_MAX_SEGMENTS + 1] = {};
    int fieldState[3] = { -1, -1, -1 };  // the `fieldOn` uniform last set per program
    struct FieldMaps {
        unsigned int tex = 0;
        int revision = 0, res = 0;
        std::weak_ptr<Field> keep;              // held weakly: a field that is gone is dropped (pruneFields)
        std::vector<float> tileDensity, tileLow, tileHigh, tileTall;    // per tile: max mask, ground range, max tall
        std::vector<float> tileArrival;                                   // and the earliest arrival
    };
    std::map<const Field*, FieldMaps> fields;
    // GPU rain and snow (Scene::precipitation): the program, a bare VAO (the corners come from gl_VertexID) and the
    // depth map seen from above along the fall.
    struct PrecipGpu { Shader sh{}; unsigned int vao = 0; std::map<std::string, int> loc; DepthTarget occ; long long drops = 0, splashes = 0; } pr;
    ::Material material{};               // raylib material: owns a MAX_MATERIAL_MAPS maps array
    unsigned int atlasFbo = 0, atlasTex = 0;
    int atlasSize = 0;
    DepthTarget sceneTarget, lineTarget;
    DepthTarget reflTarget, refrTarget;  // the water's mirrored scene, and the opaque scene copied for the water to look through
    Mesh skyCube{};
    std::map<std::string, Texture2D> textures;
    std::map<const Scene*, Texture2D> generated;    // environmentGenerator results
    struct Dynamic { DynamicTexturePtr keep; Texture2D tex{}; RenderTexture canvas{}; long long made = LLONG_MIN; };
    std::map<const DynamicTexture*, Dynamic> dynamic;   // Material::dynamicMap textures
    int frame = 0;                                      // the frame being drawn (for dynamic maps)
    Vector3 viewCam{ 0, 0, 0 };                         // the frame's camera position (fields)
    long long fieldBlades = 0;                          // blades submitted by the main pass this frame
    long long fieldShadowBlades = 0;                    // and by the shadow passes
    long long instanceBodies = 0, instanceTriangles = 0;   // instanced bodies (and their triangles) the main pass drew
};
Gpu gpu;
thread_local const Camera* viewing = nullptr;      // drawingCamera()
thread_local const Scene* viewingScene = nullptr;  // drawingScene()
thread_local float viewingHeight = 0;                   // drawingViewportHeight()

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
    if (gpu.sky.id) UnloadShader(gpu.sky);
    if (gpu.bolt.id) UnloadShader(gpu.bolt);
    if (gpu.rainbow.id) UnloadShader(gpu.rainbow);
    if (gpu.air.march.id) UnloadShader(gpu.air.march);
    if (gpu.air.comp.id) UnloadShader(gpu.air.comp);
    if (gpu.air.lowFbo) rlUnloadFramebuffer(gpu.air.lowFbo);
    for (auto& [mesh, v] : gpu.vat)
        if (v.tex) rlUnloadTexture(v.tex);
    for (auto& [field, f] : gpu.fields)
        if (f.tex) rlUnloadTexture(f.tex);
    if (gpu.fieldVao) rlUnloadVertexArray(gpu.fieldVao);
    if (gpu.fieldVbo) rlUnloadVertexBuffer(gpu.fieldVbo);
    if (gpu.pr.sh.id) UnloadShader(gpu.pr.sh);
    if (gpu.pr.vao) rlUnloadVertexArray(gpu.pr.vao);
    unloadDepthTarget(gpu.pr.occ);
    if (gpu.splat.id) UnloadShader(gpu.splat);
    for (auto& [cloud, s] : gpu.splats) {
        if (s.tex) rlUnloadTexture(s.tex);
        if (s.vao) rlUnloadVertexArray(s.vao);
        if (s.cornerVbo) rlUnloadVertexBuffer(s.cornerVbo);
        if (s.indexVbo) rlUnloadVertexBuffer(s.indexVbo);
    }
    if (gpu.skyTarget.id) UnloadRenderTexture(gpu.skyTarget);
    {
        CloudGpu& c = gpu.cl;
        for (Shader* sh : { &c.bake, &c.march, &c.shadow, &c.up }) if (sh->id) UnloadShader(*sh);
        for (RenderTexture* rt : { &c.shape, &c.detail, &c.shadowRT }) if (rt->id) UnloadRenderTexture(*rt);
        if (c.weather.id) UnloadTexture(c.weather);
        if (c.blue.id) UnloadTexture(c.blue);
        if (c.lowFbo) rlUnloadFramebuffer(c.lowFbo);
        if (c.fullFbo) rlUnloadFramebuffer(c.fullFbo);
    }
    if (gpu.material.maps) {
        gpu.material.maps[MATERIAL_MAP_DIFFUSE].texture = whiteTexture();   // cached textures are freed below
        UnloadMaterial(gpu.material);
    }
    if (gpu.atlasFbo) rlUnloadFramebuffer(gpu.atlasFbo);
    unloadDepthTarget(gpu.sceneTarget);
    unloadDepthTarget(gpu.lineTarget);
    unloadDepthTarget(gpu.reflTarget);
    unloadDepthTarget(gpu.refrTarget);
    if (gpu.skyCube.vertexCount) UnloadMesh(gpu.skyCube);
    for (auto& pair : gpu.ocean.tex) for (unsigned id : pair) if (id) rlUnloadTexture(id);
    for (auto& [path, tex] : gpu.textures) UnloadTexture(tex);
    for (auto& [scene, tex] : gpu.generated) UnloadTexture(tex);
    for (auto& [src, d] : gpu.dynamic) {
        if (d.canvas.id) UnloadRenderTexture(d.canvas);
        else if (d.tex.id) UnloadTexture(d.tex);
    }
    gpu = Gpu{};
}

void ensureGpu() {
    if (gpu.ready) return;
    const std::string head = std::string(SKIN_VS_HEAD) + FIELD_VS;
    auto splice = [](std::string src) {
        for (const auto& [marker, text] : { std::pair<const char*, const char*>{ "//@WATER_COMMON@", WATER_COMMON },
                                            { "//@RIPPLE_FS@", RIPPLE_FS }, { "//@WATER_FS@", WATER_FS }, { "//@WATER_BODY_FS@", WATER_BODY_FS }, { "//@WAKE_FS@", WAKE_FS }, { "//@WATER_SURFACE_FS@", WATER_SURFACE_FS }, { "//@WET_FS@", WET_FS }, { "//@CUTOUT_FS@", CUTOUT_FS } }) {
            const size_t at = src.find(marker);
            if (at != std::string::npos) src.replace(at, std::strlen(marker), text);
        }
        return src;
    };
    const std::string litFs = splice(LIT_FS), depthFs = splice(DEPTH_FS), gbufFs = splice(GBUF_FS);
    const std::string litVs = head + WATER_COMMON + LIT_VS_BODY;
    gpu.lit = LoadShaderFromMemory(litVs.c_str(), litFs.c_str());
    gpu.depth = LoadShaderFromMemory((head + DEPTH_VS_BODY).c_str(), depthFs.c_str());
    gpu.dof = LoadShaderFromMemory(nullptr, DOF_FS);
    gpu.gbuf = LoadShaderFromMemory(litVs.c_str(), gbufFs.c_str());
    gpu.line = LoadShaderFromMemory(nullptr, LINE_FS);
    gpu.particle = LoadShaderFromMemory(nullptr, PARTICLE_FS);
    gpu.sky = LoadShaderFromMemory(nullptr, SKY_FS);
    gpu.splat = LoadShaderFromMemory(SPLAT_VS, SPLAT_FS);
    if (!IsShaderValid(gpu.lit) || !IsShaderValid(gpu.depth) || !IsShaderValid(gpu.dof) ||
        !IsShaderValid(gpu.gbuf) || !IsShaderValid(gpu.line) || !IsShaderValid(gpu.particle) || !IsShaderValid(gpu.sky) ||
        !IsShaderValid(gpu.splat))
        throw std::runtime_error("g3d: shader compilation failed");
    auto loc = [](const char* name) { return GetShaderLocation(gpu.lit, name); };
    gpu.L = { loc("viewPos"), loc("skyColor"), loc("groundColor"), loc("lightCount"), loc("lightType"),
              loc("lightPos"), loc("lightDir"), loc("lightColor"), loc("lightRange"), loc("lightCosInner"),
              loc("lightCosOuter"), loc("lightShadow"), loc("lightBias"), loc("roughness"), loc("metalness"),
              loc("emissive"), loc("unlit"), loc("opacity"), loc("receiveShadow"), loc("shadowAtlas"),
              loc("atlasTexel"), loc("shadowVP"), loc("shadowRect"), loc("shadowParams"), loc("envMap"),
              loc("hasEnv"), loc("envLinear"), loc("envMaxLod"), loc("envIntensity"), loc("envRotation"),
              loc("skybox"), loc("fogColor"), loc("fogNear"), loc("fogFar"), loc("fogMode"), loc("fogDensity"),
              loc("fogHeight"), loc("fogFalloff"), loc("fogSky"), loc("backdropScale"), loc("skinned"), loc("boneMatrices"),
              loc("shading"), loc("pxScale"), loc("toonBands"), loc("toonSoftness"), loc("toonHighlight"),
              loc("toonHighlightSize"), loc("rimColor"), loc("rimWidth"), loc("goochWarm"), loc("goochCool"),
              loc("hatchInk"), loc("hatchSpacing"), loc("hatchAngle"), loc("hatchWeight"), loc("reflection"),
              loc("translucency"), loc("translucencyColor"), loc("hasRoughnessMap"), loc("hasNormalMap"),
              loc("normalScale") };
    gpu.cutoff[0] = loc("alphaCutoff");
    gpu.wetMat = loc("wetMat");
    { const int occUnit = WET_OCC_SLOT; SetShaderValue(gpu.lit, loc("wetOcc"), &occUnit, SHADER_UNIFORM_INT); }
    gpu.cutoff[1] = GetShaderLocation(gpu.depth, "alphaCutoff");
    gpu.cutoff[2] = GetShaderLocation(gpu.gbuf, "alphaCutoff");
    gpu.W = { loc("waterOn"), loc("waterMode"), loc("waterLevel"), loc("waterEye"), loc("waterTime"), loc("waterCentre"),
              loc("waveCount"), loc("waveA"), loc("waveB"), loc("waterAbsorb"), loc("waterScatter"), loc("waterCaustics"),
              loc("waterFoam"), loc("waterOpen"), loc("waterInvAmp"), loc("waterGlint"), loc("waterBand"), loc("waterFilter"),
              loc("reflMode"), loc("reflTex"), loc("reflVP"), loc("reflSize"), loc("reflDistort"), loc("refrMode"),
              loc("refrColor"), loc("refrDepth"), loc("refrMap"), loc("refrInvVP"), loc("reflClip") };
    {
        const int reflUnit = REFL_SLOT, colorUnit = REFR_COLOR_SLOT, depthUnit = REFR_DEPTH_SLOT;
        SetShaderValue(gpu.lit, gpu.W.reflTex, &reflUnit, SHADER_UNIFORM_INT);
        SetShaderValue(gpu.lit, gpu.W.refrColor, &colorUnit, SHADER_UNIFORM_INT);
        SetShaderValue(gpu.lit, gpu.W.refrDepth, &depthUnit, SHADER_UNIFORM_INT);
    }
    gpu.C = { loc("clipPlane"), loc("clipEdge") };
    {
        auto sp = [](const char* name) { return GetShaderLocation(gpu.splat, name); };
        gpu.P = { sp("splatData"), sp("viewM"), sp("worldM"), sp("projM"), sp("viewport"), sp("camObj"), sp("scaleMul"),
                  sp("opacityMul"), sp("shDegree"), sp("clipPlane"), sp("clipBand"), sp("scatter"),
                  GetShaderLocationAttrib(gpu.splat, "corner"), GetShaderLocationAttrib(gpu.splat, "splatIndex") };
        const int unit = SPLAT_SLOT;
        SetShaderValue(gpu.splat, gpu.P.data, &unit, SHADER_UNIFORM_INT);
    }
    auto gloc = [](const char* name) { return GetShaderLocation(gpu.gbuf, name); };
    gpu.G = { gloc("objectId"), gloc("lineStyle"), gloc("skinned"), gloc("boneMatrices") };
    {
        const Shader* base[3] = { &gpu.lit, &gpu.depth, &gpu.gbuf };
        for (int i = 0; i < 3; i++) {
            const Shader& b = *base[i];
            std::copy(b.locs, b.locs + 32, gpu.instLocs[i].begin());
            gpu.instLocs[i][SHADER_LOC_MATRIX_MODEL] = rlGetLocationAttrib(b.id, "instanceTransform");
            gpu.inst[i] = b;
            gpu.inst[i].locs = gpu.instLocs[i].data();
            gpu.il[i] = { GetShaderLocation(b, "instanced"), GetShaderLocation(b, "vat"), GetShaderLocation(b, "vatWidth"),
                          GetShaderLocation(b, "vatVerts"), GetShaderLocation(b, "vatRows") };
            auto f = [&b](const char* name) { return GetShaderLocation(b, name); };
            gpu.F[i] = { f("fieldOn"), f("fieldWorld"), f("fieldTile"), f("fieldKey"), f("fieldSeed"), f("fieldArea"),
                         f("fieldMapFit"), f("fieldMaps"), f("fieldBlade"), f("fieldWind"), f("fieldGusts"),
                         f("fieldFade"), f("fieldCam"), f("fieldRoot"), f("fieldTip"), f("fieldDry"), f("fieldBands"), f("fieldWiden"), f("fieldPick") };
        }
    }
    auto nloc = [](const char* name) { return GetShaderLocation(gpu.line, name); };
    gpu.N = { nloc("depthTex"), nloc("texel"), nloc("zNear"), nloc("zFar"), nloc("ortho"), nloc("pixelWorld"),
              nloc("pxScale"), nloc("styles"), nloc("maxRadius"), nloc("creaseCos"), nloc("depthThreshold"),
              nloc("wobble"), nloc("wobbleScale"), nloc("seed") };
    auto dloc = [](const char* name) { return GetShaderLocation(gpu.dof, name); };
    gpu.D = { dloc("depthTex"), dloc("texel"), dloc("zNear"), dloc("zFar"), dloc("ortho"), dloc("focus"),
              dloc("aperture"), dloc("maxBlur"), dloc("radStep"), dloc("tanTilt"), dloc("tanHalfFov"), dloc("viewY") };
    auto sloc = [](const char* name) { return GetShaderLocation(gpu.sky, name); };
    gpu.S = { sloc("size"), sloc("zenith"), sloc("horizon"), sloc("ground"), sloc("curve"), sloc("sunDir"),
              sloc("sunColor"), sloc("sunCos"), sloc("sunGlow"), sloc("haze"), sloc("hazeAmount"), sloc("stars"),
              sloc("clouds"), sloc("cloudColor"), sloc("cloudShade"), sloc("cloudScale"), sloc("cloudOffset"),
              sloc("frame"), sloc("seed") };
    gpu.depthSkinned = GetShaderLocation(gpu.depth, "skinned");
    gpu.depthBones = GetShaderLocation(gpu.depth, "boneMatrices");
    // Samplers live on fixed units for the whole run.
    int envUnit = ENV_SLOT, shadowUnit = SHADOW_SLOT, depthUnit = DEPTH_SLOT;
    SetShaderValue(gpu.lit, gpu.L.envMap, &envUnit, SHADER_UNIFORM_INT);
    SetShaderValue(gpu.lit, gpu.L.shadowAtlas, &shadowUnit, SHADER_UNIFORM_INT);
    SetShaderValue(gpu.dof, gpu.D.depthTex, &depthUnit, SHADER_UNIFORM_INT);
    SetShaderValue(gpu.line, gpu.N.depthTex, &depthUnit, SHADER_UNIFORM_INT);
    gpu.cll = { loc("cloudShadowOn"), loc("cloudShadowLight"), loc("cloudShadowTex"), loc("cloudShadowMap"),
                loc("cloudShadowSun"), loc("cloudShadowFloor") };
    const int cloudUnit = CLOUD_SLOT;
    SetShaderValue(gpu.lit, gpu.cll.tex, &cloudUnit, SHADER_UNIFORM_INT);
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

// Mipmapped, trilinear and repeating: right for material maps (environments clamp in v).
void prepareTexture(Texture2D& tex) {
    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
}

// An equirectangular environment: repeating in u (the azimuth wraps) but clamped in v, or the
// top row would blend with the bottom one at the poles.
void prepareEnvironmentTexture(Texture2D& tex) {
    prepareTexture(tex);
    rlTextureParameters(tex.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
}

// `environment` selects the environment's sampler state. It has its own cache key, since a
// file may serve as a material map too and that must keep repeating.
Texture2D texture(const std::string& path, bool environment = false) {
    const std::string key = environment ? "env:" + path : path;
    auto it = gpu.textures.find(key);
    if (it != gpu.textures.end()) return it->second;
    Image img = media::loadImage(path);      // resolved like other media; JPG and HDR included
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    if (!IsTextureValid(tex)) throw std::runtime_error("g3d: cannot load texture: " + path);
    if (environment) prepareEnvironmentTexture(tex);
    else prepareTexture(tex);
    gpu.textures[key] = tex;
    return tex;
}

// A Material::dynamicMap's texture for the current frame, remade when its turn comes.
Texture2D dynamicTexture(const DynamicTexturePtr& src) {
    Gpu::Dynamic& d = gpu.dynamic[src.get()];
    d.keep = src;                                   // the address stays this source's
    const long long turn = (long long)std::floor((double)gpu.frame / std::max(1, src->every));
    if (src->draw) return d.tex.id ? d.tex : whiteTexture();    // drawn by drawCanvases before the pass
    if (d.made == turn && d.tex.id) return d.tex;
    d.made = turn;
    if (!src->make) return d.tex.id ? d.tex : whiteTexture();
    Image img = src->make(gpu.frame);
    if (!img.data) return d.tex.id ? d.tex : whiteTexture();
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (d.tex.id && d.tex.width == img.width && d.tex.height == img.height) {
        UpdateTexture(d.tex, img.data);
        GenTextureMipmaps(&d.tex);
    } else {
        if (d.tex.id) UnloadTexture(d.tex);
        d.tex = LoadTextureFromImage(img);
        prepareTexture(d.tex);
    }
    UnloadImage(img);
    return d.tex.id ? d.tex : whiteTexture();
}

// Draws the canvas-style dynamic maps (DynamicTexture::draw) of the visible objects for
// frame t. Runs before the scene's own passes, while the 2D canvas target is still bound.
void drawCanvas(const DynamicTexturePtr& src, int t) {
    Gpu::Dynamic& d = gpu.dynamic[src.get()];
    d.keep = src;
    const long long turn = (long long)std::floor((double)t / std::max(1, src->every));
    const int ss = std::clamp(src->supersample, 1, 4);
    const int w = std::max(1, src->width) * ss, h = std::max(1, src->height) * ss;
    if (d.canvas.id && (d.canvas.texture.width != w || d.canvas.texture.height != h)) {
        UnloadRenderTexture(d.canvas);
        d.canvas = RenderTexture{};
    }
    if (d.made == turn && d.canvas.id) return;
    d.made = turn;
    if (!d.canvas.id) {
        d.canvas = LoadRenderTexture(w, h);
        SetTextureWrap(d.canvas.texture, src->repeat ? TEXTURE_WRAP_REPEAT : TEXTURE_WRAP_CLAMP);
    }
    drawToTexture(d.canvas, (float)ss, [&] { src->draw(Ctx{ { 0, 0 }, 1, 0, 1 }, t); });
    d.tex = d.canvas.texture;
    d.tex.mipmaps = 1 + (int)std::floor(std::log2((float)std::max(w, h)));
}

void drawCanvases(std::vector<std::unique_ptr<Object3D>>& list, int t) {
    for (auto& o : list) {
        if (!o->visible || o->culled) continue;
        if (o->material.dynamicMap && o->material.dynamicMap->draw) drawCanvas(o->material.dynamicMap, t);
        drawCanvases(o->children, t);
    }
}

Texture2D mapOf(const g3d::Material& m) {
    if (m.dynamicMap) return dynamicTexture(m.dynamicMap);
    return m.map.empty() ? whiteTexture() : texture(m.map);
}
// Material::dynamicNormalMap or normalMap, else white (unused: hasNormalMap is 0).
Texture2D normalMapOf(const g3d::Material& m) {
    if (m.dynamicNormalMap) return dynamicTexture(m.dynamicNormalMap);
    return m.normalMap.empty() ? whiteTexture() : texture(m.normalMap);
}

Vector3 linear3(Color c) {
    return { std::pow(c.r / 255.f, 2.2f), std::pow(c.g / 255.f, 2.2f), std::pow(c.b / 255.f, 2.2f) };
}

// Where the sun is: towards it from the origin.
Vector3 sunOf(const Scene& s) {
    const Sky& k = s.sky;
    if (k.sunLight >= 0 && k.sunLight < (int)s.lights.size()) {
        const Light& l = s.lights[(size_t)k.sunLight];
        const Vector3 d = l.type == Light::Type::Directional ? Vector3Subtract(l.position, l.target) : l.position;
        if (Vector3Length(d) > 1e-6f) return Vector3Normalize(d);
    }
    return Vector3Length(k.sunDirection) > 1e-6f ? Vector3Normalize(k.sunDirection) : Vector3{ 0, 1, 0 };
}

// The sky into its equirectangular target, mipmapped for the environment's blur. Called
// before anything else in the frame; leaves the caller's framebuffer, viewport and matrices.
Texture2D renderSky(const Scene& s, double frame, unsigned int callerFbo, int fbW, int fbH) {
    Sky k = s.weather.enabled ? inWeather(s.sky, s.weather, frame) : s.sky;     // with the weather worked in
    if (s.clouds.enabled) k.clouds = 0;         // the volumetric layer replaces the flat painted one
    const int w = std::clamp(k.resolution, 32, 4096), h = w / 2;
    if (gpu.skyTarget.texture.width != w) {
        if (gpu.skyTarget.id) UnloadRenderTexture(gpu.skyTarget);
        gpu.skyTarget = LoadRenderTexture(w, h);
    }
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(gpu.skyTarget.id);
    rlViewport(0, 0, w, h);
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    rlOrtho(0, w, h, 0, -1, 1);
    rlMatrixMode(RL_MODELVIEW);
    rlPushMatrix();
    rlLoadIdentity();
    rlDisableColorBlend();
    const auto& S = gpu.S;
    const Vector2 size{ (float)w, (float)h };
    const Vector3 zen = linear3(k.zenith), hor = linear3(k.horizon), gr = linear3(k.ground), sun = sunOf(s),
                  sc = linear3(k.sunColor), hz = linear3(k.haze), cc = linear3(k.cloudColor), cs = linear3(k.cloudShade);
    const float sunCos = k.sunSize > 0 ? std::cos(k.sunSize * DEG2RAD) : 1.f, f = (float)frame, seed = (float)(k.seed % 9973);
    const Vector2 off{ k.cloudDrift.x * f + k.cloudShift.x, k.cloudDrift.y * f + k.cloudShift.y };
    SetShaderValue(gpu.sky, S.size, &size, SHADER_UNIFORM_VEC2);
    SetShaderValue(gpu.sky, S.zenith, &zen, SHADER_UNIFORM_VEC3);
    SetShaderValue(gpu.sky, S.horizon, &hor, SHADER_UNIFORM_VEC3);
    SetShaderValue(gpu.sky, S.ground, &gr, SHADER_UNIFORM_VEC3);
    SetShaderValue(gpu.sky, S.curve, &k.curve, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.sky, S.sunDir, &sun, SHADER_UNIFORM_VEC3);
    SetShaderValue(gpu.sky, S.sunColor, &sc, SHADER_UNIFORM_VEC3);
    SetShaderValue(gpu.sky, S.sunCos, &sunCos, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.sky, S.sunGlow, &k.sunGlow, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.sky, S.haze, &hz, SHADER_UNIFORM_VEC3);
    SetShaderValue(gpu.sky, S.hazeAmount, &k.hazeAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.sky, S.stars, &k.stars, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.sky, S.clouds, &k.clouds, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.sky, S.cloudColor, &cc, SHADER_UNIFORM_VEC3);
    SetShaderValue(gpu.sky, S.cloudShade, &cs, SHADER_UNIFORM_VEC3);
    SetShaderValue(gpu.sky, S.cloudScale, &k.cloudScale, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.sky, S.cloudOffset, &off, SHADER_UNIFORM_VEC2);
    SetShaderValue(gpu.sky, S.frame, &f, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.sky, S.seed, &seed, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(gpu.sky);
    DrawRectangle(0, 0, w, h, WHITE);
    EndShaderMode();
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    // rlgl keeps one stack for both modes: pop in the reverse order of the pushes.
    rlMatrixMode(RL_MODELVIEW);
    rlPopMatrix();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlEnableFramebuffer(callerFbo);
    rlViewport(0, 0, fbW, fbH);
    Texture2D tex = gpu.skyTarget.texture;
    GenTextureMipmaps(&tex);
    gpu.skyTarget.texture.mipmaps = tex.mipmaps;
    SetTextureFilter(gpu.skyTarget.texture, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(gpu.skyTarget.texture, TEXTURE_WRAP_REPEAT);
    rlTextureParameters(gpu.skyTarget.texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);   // not over the poles
    return gpu.skyTarget.texture;
}

// ---- cube-face environments: six faces (or a horizontal cross) resampled to equirectangular.
// The OpenGL cube-map table: for a direction r, the major axis picks the face and
// (sc, tc, ma) give s = (sc / |ma| + 1) / 2, t = (tc / |ma| + 1) / 2 with t counted down the
// image. Face order +x, -x, +y, -y, +z, -z. The engine is right-handed, so cubeToEquirect
// looks a world direction up with x negated, as three.js does (flipEnvMap).
bool isFloatFormat(int format);

struct CubeFace {
    int n = 0;
    bool isFloat = false;
    std::vector<float> px;      // n * n * 4, RGBA; 8-bit faces stay in 0..255
};

CubeFace cubeFaceFromImage(Image img, const std::string& what) {
    if (!img.data) throw std::runtime_error("g3d: cannot load environment face " + what);
    if (img.width != img.height)
        throw std::runtime_error("g3d: environment faces must be square: " + what + " is " +
                                 std::to_string(img.width) + "x" + std::to_string(img.height));
    CubeFace f;
    f.n = img.width;
    f.isFloat = isFloatFormat(img.format);
    ImageFormat(&img, f.isFloat ? PIXELFORMAT_UNCOMPRESSED_R32G32B32A32 : PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    const size_t count = (size_t)f.n * f.n * 4;
    f.px.resize(count);
    if (f.isFloat) std::memcpy(f.px.data(), img.data, count * sizeof(float));
    else for (size_t i = 0; i < count; ++i) f.px[i] = ((const unsigned char*)img.data)[i];
    return f;
}

// The face and its (s, t) in 0..1 for a direction, by the OpenGL table.
int cubeFaceLookup(float rx, float ry, float rz, float& s, float& t) {
    const float ax = std::fabs(rx), ay = std::fabs(ry), az = std::fabs(rz);
    int face;
    float sc, tc, ma;
    if (ax >= ay && ax >= az) {
        face = rx > 0 ? 0 : 1; ma = ax; sc = rx > 0 ? -rz : rz; tc = -ry;
    } else if (ay >= az) {
        face = ry > 0 ? 2 : 3; ma = ay; sc = rx; tc = ry > 0 ? rz : -rz;
    } else {
        face = rz > 0 ? 4 : 5; ma = az; sc = rz > 0 ? rx : -rx; tc = -ry;
    }
    s = (sc / ma + 1.f) * 0.5f;
    t = (tc / ma + 1.f) * 0.5f;
    return face;
}

// Bilinear inside a face (clamped at its border).
void cubeFaceSample(const CubeFace& f, float s, float t, float out[4]) {
    const float fx = std::clamp(s * f.n - 0.5f, 0.f, (float)(f.n - 1));
    const float fy = std::clamp(t * f.n - 0.5f, 0.f, (float)(f.n - 1));
    const int x0 = (int)fx, y0 = (int)fy, x1 = std::min(x0 + 1, f.n - 1), y1 = std::min(y0 + 1, f.n - 1);
    const float ax = fx - x0, ay = fy - y0;
    for (int c = 0; c < 4; ++c) {
        auto at = [&](int x, int y) { return f.px[((size_t)y * f.n + x) * 4 + c]; };
        out[c] = (at(x0, y0) * (1 - ax) + at(x1, y0) * ax) * (1 - ay) + (at(x0, y1) * (1 - ax) + at(x1, y1) * ax) * ay;
    }
}

Image cubeToEquirect(const std::vector<CubeFace>& faces) {
    const int n = faces[0].n;
    const bool isFloat = faces[0].isFloat;
    int w = std::min(4 * n, 4096);
    w -= w % 2;
    const int h = w / 2;
    Image img{};
    img.width = w; img.height = h; img.mipmaps = 1;
    img.format = isFloat ? PIXELFORMAT_UNCOMPRESSED_R32G32B32A32 : PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    img.data = RL_CALLOC((size_t)w * h, isFloat ? 16 : 4);
    for (int y = 0; y < h; ++y) {
        const float theta = (y + 0.5f) / h * PI, sy = std::sin(theta), ry = std::cos(theta);
        for (int x = 0; x < w; ++x) {
            const float phi = ((x + 0.5f) / w - 0.5f) * 2 * PI;     // the shader's u = atan(z, x) / 2pi + 0.5
            float s, t, rgba[4];
            // x is negated: packs are labelled for left-handed engines (see Scene::environmentFaces)
            const int face = cubeFaceLookup(-std::cos(phi) * sy, ry, std::sin(phi) * sy, s, t);
            cubeFaceSample(faces[face], s, t, rgba);
            const size_t i = ((size_t)y * w + x) * 4;
            for (int c = 0; c < 4; ++c) {
                if (isFloat) ((float*)img.data)[i + c] = rgba[c];
                else ((unsigned char*)img.data)[i + c] = (unsigned char)std::clamp((int)(rgba[c] + 0.5f), 0, 255);
            }
        }
    }
    return img;
}

// Loads the six faces, or the one cross, and returns the equirectangular image. Throws
// std::runtime_error naming the problem.
Image loadEnvironmentFaces(const std::vector<std::string>& files) {
    std::vector<CubeFace> faces;
    if (files.size() == 6) {
        for (const auto& f : files) {
            if (f.empty()) throw std::runtime_error("g3d: environment face path is empty");
            faces.push_back(cubeFaceFromImage(media::loadImage(f), f));
        }
    } else if (files.size() == 1) {
        Image img = media::loadImage(files[0]);
        if (!img.data) throw std::runtime_error("g3d: cannot load environment cross " + files[0]);
        const int n = img.width / 4;
        if (n < 1 || img.width != 4 * n || img.height != 3 * n) {
            const std::string size = std::to_string(img.width) + "x" + std::to_string(img.height);
            UnloadImage(img);
            throw std::runtime_error("g3d: environment cross must be a 4x3 grid of square faces: " + files[0] + " is " + size);
        }
        // columns/rows of +x, -x, +y, -y, +z, -z in the horizontal cross
        const int col[6] = { 2, 0, 1, 1, 1, 3 }, row[6] = { 1, 1, 0, 2, 1, 1 };
        for (int i = 0; i < 6; ++i) {
            Image tile = ImageFromImage(img, Rectangle{ (float)(col[i] * n), (float)(row[i] * n), (float)n, (float)n });
            faces.push_back(cubeFaceFromImage(tile, files[0]));
            UnloadImage(tile);
        }
        UnloadImage(img);
    } else {
        throw std::runtime_error("g3d: environmentFaces needs six face files (+x -x +y -y +z -z) or one cross image, got " +
                                 std::to_string(files.size()));
    }
    for (const auto& f : faces)
        if (f.n != faces[0].n || f.isFloat != faces[0].isFloat)
            throw std::runtime_error("g3d: environment faces must all have the same size and kind (8-bit or HDR)");
    return cubeToEquirect(faces);
}

Texture2D environmentTexture(const Scene& s) {
    if (!s.environment.empty()) return texture(s.environment, true);
    if (!s.environmentFaces.empty()) {
        std::string key = "cube:";
        for (const auto& f : s.environmentFaces) key += f + "|";
        auto it = gpu.textures.find(key);
        if (it != gpu.textures.end()) return it->second;
        Image img = loadEnvironmentFaces(s.environmentFaces);
        Texture2D tex = LoadTextureFromImage(img);
        UnloadImage(img);
        if (!IsTextureValid(tex)) throw std::runtime_error("g3d: cannot make the environment from its faces");
        prepareEnvironmentTexture(tex);
        gpu.textures[key] = tex;
        return tex;
    }
    if (!s.environmentGenerator) return Texture2D{};
    auto it = gpu.generated.find(&s);
    if (it != gpu.generated.end()) return it->second;
    Image img = s.environmentGenerator();
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    if (!IsTextureValid(tex)) throw std::runtime_error("g3d: environment generator produced no image");
    prepareEnvironmentTexture(tex);
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
// One tile of a field this frame: which, how many of its blades, at what detail.
// `shadowCount` is how many of its blades go into the shadow maps (0 = none); `seen` is false
// for a tile kept only for the shadow it casts into view.
struct FieldTile { int tx, tz; int count; int segments; int shadowCount; bool seen; };
struct Item {
    Object3D* obj; Matrix world; float viewDepth;
    float alpha = 1;
    Color tint = WHITE;
    bool particle = false;
    const std::vector<Matrix>* inst = nullptr;   // an instanced set: the bodies that can show
    const std::vector<FieldTile>* tiles = nullptr;   // a field: the tiles that can show
};
float opacityOf(const Item& it) { return it.obj->material.opacity * it.alpha; }
// An emitter without a geometry: its particles are drawn as billboards after the meshes.
struct Emitting { const Object3D* obj; Matrix world; };

Matrix localMatrix(const Object3D& o) {
    return MatrixMultiply(MatrixMultiply(MatrixScale(o.scale.x, o.scale.y, o.scale.z),
                                         MatrixRotateXYZ(Vector3Scale(o.rotation, DEG2RAD))),
                          MatrixTranslate(o.position.x, o.position.y, o.position.z));
}

// What culling needs for one frame (Object3D::cullRadius).
struct Cull {
    bool on = false;
    const Camera* cam = nullptr;
    const Camera* mirror = nullptr;      // the water's mirrored camera: an object it sees stays in the frame too
    float aspect = 1, floorY = 0;
    std::vector<Vector3> shadowDirs;     // unit: the way the shadows of each shadowing light run
    struct Footprint { Vector3 target, dir; float extent; };
    std::vector<Footprint> footprints;   // what each shadowing light's map covers: the shadowExtent round its target
    float screenH = 1080;                // canvas px the view covers, top to bottom (surfaces)
    float compH = 1080;                  // composition px the view covers, top to bottom (line widths)
    float rainR = 0;                     // > 0: an object within this many metres (x, z) of the camera stays: it may shelter the rain
};
// A Surface object's world matrix: its own, or square to the camera for a billboard (see below).
Matrix surfaceWorld(const Object3D& o, const Matrix& world, const Camera& cam, float screenH);
// Seen, or casting a shadow that can fall into view. A shadow runs from the sphere along
// each light's direction down to the floor (at most 80 m): the swept sphere is tested as a
// string of spheres no further apart than its radius.
bool inSight(const Cull& k, Vector3 c, float r, const Camera* from = nullptr) {
    if (!from) from = k.cam;
    if (k.rainR > 0) {
        const float dx = c.x - k.cam->position.x, dz = c.z - k.cam->position.z, reach = k.rainR + r;
        if (dx * dx + dz * dz <= reach * reach) return true;
    }
    if (visibleSphere(*from, k.aspect, c, r)) return true;
    for (const Vector3& away : k.shadowDirs) {
        float reach = 80;
        if (away.y < -0.05f) reach = std::min(reach, std::max(0.f, (c.y - k.floorY) / -away.y));
        const float rs = r + 0.3f;
        const int n = std::clamp((int)std::ceil(reach / std::max(rs, 0.5f)), 1, 160);
        for (int i = 1; i <= n; i++)
            if (visibleSphere(*from, k.aspect, Vector3Add(c, Vector3Scale(away, reach * i / n)), rs)) return true;
    }
    return false;
}

void animate(std::vector<std::unique_ptr<Object3D>>& list, int t, const Matrix& parent, const Cull& k) {
    for (auto& o : list) {
        if (o->up) o->up(t, *o);
        o->culled = false;
        Matrix world = parent;
        if (k.on) {
            world = MatrixMultiply(localMatrix(*o), parent);
            if (o->cullRadius > 0) {
                const float sc = std::max({ std::sqrt(world.m0 * world.m0 + world.m1 * world.m1 + world.m2 * world.m2),
                                            std::sqrt(world.m4 * world.m4 + world.m5 * world.m5 + world.m6 * world.m6),
                                            std::sqrt(world.m8 * world.m8 + world.m9 * world.m9 + world.m10 * world.m10) });
                const Vector3 cc = Vector3Transform(o->cullCentre, world);
                if (!inSight(k, cc, o->cullRadius * sc) && !(k.mirror && inSight(k, cc, o->cullRadius * sc, k.mirror))) {
                    o->culled = true;
                    continue;
                }
            }
        }
        animate(o->children, t, world, k);
    }
}

// Within a shadowing light's map: the shadowExtent box round its target, as the circle that holds it.
bool inFootprint(const Cull& k, Vector3 c, float r) {
    for (const Cull::Footprint& f : k.footprints) {
        const Vector3 rel = Vector3Subtract(c, f.target);
        const Vector3 across = Vector3Subtract(rel, Vector3Scale(f.dir, Vector3DotProduct(rel, f.dir)));
        if (Vector3Length(across) <= f.extent * 1.4143f + r) return true;
    }
    return false;
}

// Instance::tint x Instance::shade as the shader's instTint() reads it.
float packTint(Color tint, float shade) {
    auto q = [shade](unsigned char c) {
        const float v = std::isfinite(shade) ? c / 255.f * shade : 1.f;
        return (float)std::lround(std::clamp(v, 0.f, 2.f) * 127.f);   // 127 = exactly 1
    };
    return q(tint.r) + 256.f * q(tint.g) + 65536.f * q(tint.b);
}

// The world matrices of an object's instances that can show, each carrying its animation (two
// baked frames, their blend, a colour) in the w of its columns, where an affine matrix has
// 0, 0, 0 and 1: see the shader's setupInstance.
void gatherInstances(const Object3D& o, const Matrix& world, const Cull& k, std::vector<Matrix>& out) {
    const Instances& in = *o.instances;
    const AnimatedMesh& am = *in.mesh;
    if (am.frames <= 0) return;
    out.reserve(in.list.size());
    for (const Instance& i : in.list) {
        Matrix m;
        if (Vector3LengthSqr(i.aim) > 1e-12f) {
            // Directions to a basis: z along aim, y as near aimUp as it can be, x = y cross z.
            const Vector3 z = Vector3Normalize(i.aim);
            Vector3 y = Vector3Subtract(i.aimUp, Vector3Scale(z, Vector3DotProduct(i.aimUp, z)));
            if (Vector3LengthSqr(y) < 1e-8f) y = Vector3Subtract({ 0, 1, 0 }, Vector3Scale(z, z.y));
            if (Vector3LengthSqr(y) < 1e-8f) y = { 0, 0, 1 };
            y = Vector3Normalize(y);
            const Vector3 x = Vector3CrossProduct(y, z);
            const Matrix basis = { x.x, y.x, z.x, i.position.x,
                                   x.y, y.y, z.y, i.position.y,
                                   x.z, y.z, z.z, i.position.z,
                                   0, 0, 0, 1 };
            m = MatrixMultiply(MatrixMultiply(MatrixScale(i.scale, i.scale, i.scale), basis), world);
        } else {
            const Matrix tilt = MatrixRotateXYZ(Vector3Scale({ i.pitch, 0, i.bank }, DEG2RAD));
            const Matrix turn = MatrixMultiply(MatrixRotateXYZ(Vector3Scale({ 0, i.yaw, 0 }, DEG2RAD)),
                                               MatrixTranslate(i.position.x, i.position.y, i.position.z));
            m = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(i.scale, i.scale, i.scale), tilt), turn), world);
        }
        if (k.on) {
            const float sc = std::max({ std::sqrt(m.m0 * m.m0 + m.m1 * m.m1 + m.m2 * m.m2),
                                        std::sqrt(m.m4 * m.m4 + m.m5 * m.m5 + m.m6 * m.m6),
                                        std::sqrt(m.m8 * m.m8 + m.m9 * m.m9 + m.m10 * m.m10) });
            if (!inSight(k, Vector3Transform(am.centre, m), am.radius * sc)) continue;
            // A copy that only casts is of use where a shadow map covers it.
            if (o.material.shadowOnly && !inFootprint(k, Vector3Transform(am.centre, m), am.radius * sc)) continue;
        }
        const int len = std::max(1, i.clipLength);
        float f = std::fmod(i.frame, (float)len);
        if (f < 0) f += len;
        const int a = std::min((int)f, len - 1);
        const int last = am.frames - 1;
        m.m3 = (float)std::clamp(i.clipStart + a, 0, last);
        m.m7 = (float)std::clamp(i.clipStart + (a + 1) % len, 0, last);
        const float fade = std::isfinite(i.fade) ? std::clamp(i.fade, -1.f, 1.f) : 1.f;
        const float from = std::isfinite(i.fadeFrom) && fade > 0 ? std::clamp(i.fadeFrom, 0.f, 1.f) : 0.f;
        m.m11 = std::min(f - (float)a, 0.999f) + (float)std::lround((fade + 1) * 32) + 65.f * (float)std::lround(from * 32);   // see setupInstance
        m.m15 = packTint(i.tint, i.shade);
        out.push_back(m);
    }
}

// ---- fields --------------------------------------------------------------------------
constexpr float FIELD_BAND = 0.12f;      // FIELD_VS's BAND: blades shrink away over this rank band

float fieldFar(const Field& f, float distance) {   // FIELD_VS's `far`: the fraction kept at a distance
    const float a = f.fadeStart, b = std::max(f.fadeEnd, f.fadeStart + 1e-3f);
    const float x = std::clamp((distance - a) / (b - a), 0.f, 1.f);
    return 1.f - x * x * (3.f - 2.f * x);
}

// Frees the maps of fields that no longer exist (a script remakes its scene every frame).
void pruneFields() {
    for (auto it = gpu.fields.begin(); it != gpu.fields.end();)
        if (it->second.keep.expired()) {
            if (it->second.tex) rlUnloadTexture(it->second.tex);
            it = gpu.fields.erase(it);
        } else ++it;
}

// The field's ground, density and height maps, sampled over its area into one float texture,
// with each tile's largest density, ground range and largest height multiplier (for culling
// and for how many blades to draw). Remade when the field's revision changes.
const Gpu::FieldMaps& fieldMapsOf(const std::shared_ptr<Field>& field) {
    const Field& f = *field;
    auto found = gpu.fields.find(field.get());
    if (found != gpu.fields.end() && found->second.revision == f.revision && found->second.keep.lock() == field) return found->second;
    if (found != gpu.fields.end()) {
        if (found->second.tex) rlUnloadTexture(found->second.tex);
        gpu.fields.erase(found);
    }
    Gpu::FieldMaps m;
    m.keep = field;
    m.revision = f.revision;
    m.res = std::clamp(f.mapResolution, 2, 4096);
    const int res = m.res;
    const float w = f.max.x - f.min.x, d = f.max.y - f.min.y;
    std::vector<float> texels((size_t)res * res * 4);
    auto finite = [](float v, float fallback) { return std::isfinite(v) ? v : fallback; };
    for (int j = 0; j < res; j++)
        for (int i = 0; i < res; i++) {
            const float x = f.min.x + w * i / (res - 1), z = f.min.y + d * j / (res - 1);
            float* t = &texels[((size_t)j * res + i) * 4];
            t[0] = f.ground ? finite(f.ground(x, z), 0.f) : 0.f;
            t[1] = f.mask ? std::clamp(finite(f.mask(x, z), 0.f), 0.f, 1.f) : 1.f;
            t[2] = f.tall ? std::max(finite(f.tall(x, z), 0.f), 0.f) : 1.f;
            t[3] = f.arrival ? std::clamp(finite(f.arrival(x, z), 0.f), 0.f, 1.f) : 0.f;
        }
    const int nx = f.tilesX(), nz = f.tilesZ();
    const float ts = std::max(f.tileSize, 0.05f);
    m.tileDensity.assign((size_t)nx * nz, 0.f);
    m.tileLow.assign((size_t)nx * nz, 1e30f);
    m.tileHigh.assign((size_t)nx * nz, -1e30f);
    m.tileTall.assign((size_t)nx * nz, 0.f);
    m.tileArrival.assign((size_t)nx * nz, 1.f);
    auto texelOf = [&](float v, float lo, float span) {   // nearest sample index, unclamped
        return span > 0 ? (int)std::floor((v - lo) / span * (res - 1)) : 0;
    };
    for (int tz = 0; tz < nz; tz++)
        for (int tx = 0; tx < nx; tx++) {
            // Every sample the tile's blades can read, one texel of margin either side.
            const int i0 = std::clamp(texelOf(f.min.x + tx * ts, f.min.x, w) - 1, 0, res - 1);
            const int i1 = std::clamp(texelOf(f.min.x + (tx + 1) * ts, f.min.x, w) + 2, 0, res - 1);
            const int j0 = std::clamp(texelOf(f.min.y + tz * ts, f.min.y, d) - 1, 0, res - 1);
            const int j1 = std::clamp(texelOf(f.min.y + (tz + 1) * ts, f.min.y, d) + 2, 0, res - 1);
            const size_t ti = (size_t)tz * nx + tx;
            for (int j = j0; j <= j1; j++)
                for (int i = i0; i <= i1; i++) {
                    const float* t = &texels[((size_t)j * res + i) * 4];
                    m.tileLow[ti] = std::min(m.tileLow[ti], t[0]);
                    m.tileHigh[ti] = std::max(m.tileHigh[ti], t[0]);
                    m.tileDensity[ti] = std::max(m.tileDensity[ti], t[1]);
                    m.tileTall[ti] = std::max(m.tileTall[ti], t[2]);
                    m.tileArrival[ti] = std::min(m.tileArrival[ti], t[3]);
                }
        }
    m.tex = rlLoadTexture(texels.data(), res, res, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
    rlTextureParameters(m.tex, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_BILINEAR);
    rlTextureParameters(m.tex, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_BILINEAR);
    rlTextureParameters(m.tex, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
    rlTextureParameters(m.tex, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
    return gpu.fields.emplace(field.get(), std::move(m)).first->second;
}

// The tiles of a field that can show this frame (or cast a shadow into view), each with how
// many of its blades to draw: no more than the densest point of the tile keeps at the
// tile's nearest distance from the camera.
void gatherField(const Object3D& o, const Matrix& world, const Cull& k, std::vector<FieldTile>& out) {
    const Field& f = *o.field;
    if (!(f.max.x > f.min.x && f.max.y > f.min.y) || f.density <= 0) return;
    const Gpu::FieldMaps& maps = fieldMapsOf(o.field);
    const int nx = f.tilesX(), nz = f.tilesZ();
    const float ts = std::max(f.tileSize, 0.05f);
    const uint32_t full = f.bladesPerTile();
    if (!full) return;
    const float sc = std::max({ std::sqrt(world.m0 * world.m0 + world.m1 * world.m1 + world.m2 * world.m2),
                                std::sqrt(world.m4 * world.m4 + world.m5 * world.m5 + world.m6 * world.m6),
                                std::sqrt(world.m8 * world.m8 + world.m9 * world.m9 + world.m10 * world.m10) });
    const float tallest = f.height * (1 + std::fabs(f.heightVariation));
    const int segments = std::clamp(f.segments, 1, FIELD_MAX_SEGMENTS);
    for (int tz = 0; tz < nz; tz++)
        for (int tx = 0; tx < nx; tx++) {
            const size_t ti = (size_t)tz * nx + tx;
            if (maps.tileDensity[ti] <= 0 || maps.tileTall[ti] <= 0) continue;
            if (f.grow * 1.15f <= maps.tileArrival[ti]) continue;             // nothing has come up yet
            const float top = maps.tileHigh[ti] + tallest * maps.tileTall[ti];
            const Vector3 c{ f.min.x + (tx + 0.5f) * ts, 0.5f * (maps.tileLow[ti] + top), f.min.y + (tz + 0.5f) * ts };
            const float halfY = 0.5f * (top - maps.tileLow[ti]) + tallest;            // bending reaches sideways too
            const float r = std::sqrt(0.5f * ts * ts + halfY * halfY) * sc;
            const Vector3 cw = Vector3Transform(c, world);
            const float nearest = std::max(0.f, Vector3Distance(cw, k.cam->position) - r);
            const float keep = maps.tileDensity[ti] * fieldFar(f, nearest);
            if (keep <= 0) continue;
            // Seen; or, only when the grass casts shadows, casting one that can fall into view.
            const bool seen = !k.on || visibleSphere(*k.cam, k.aspect, cw, r);
            if (!seen && !(f.castShadows && inSight(k, cw, r))) continue;
            const int count = (int)std::min<double>(full, std::ceil(full * keep * (1 + FIELD_BAND)));
            if (count <= 0) continue;
            int shadowCount = 0;     // the first blades by rank, as the main pass has them
            if (f.castShadows && nearest < f.shadowDistance && (!k.on || inFootprint(k, cw, r)))
                shadowCount = std::max(1, (int)std::ceil(count * std::clamp(f.shadowDensity, 0.02f, 1.f)));
            out.push_back({ tx, tz, count, nearest < f.detailDistance ? segments : 1, shadowCount, seen });
        }
}

// The blade templates: a strip of quads up the blade, then a triangle to the tip.
void fieldTemplates() {
    if (gpu.fieldVao) return;
    std::vector<float> v;
    auto put = [&v](float x, float y) { v.insert(v.end(), { x, y, 0.f }); };
    for (int s = 1; s <= FIELD_MAX_SEGMENTS; s++) {
        gpu.fieldFirst[s] = (int)(v.size() / 3);
        for (int j = 0; j < s; j++) {
            const float t0 = (float)j / s, t1 = (float)(j + 1) / s;
            if (j < s - 1) { put(-1, t0); put(1, t0); put(1, t1); put(-1, t0); put(1, t1); put(-1, t1); }
            else { put(-1, t0); put(1, t0); put(0, 1); }
        }
        gpu.fieldCount[s] = (int)(v.size() / 3) - gpu.fieldFirst[s];
    }
    gpu.fieldVao = rlLoadVertexArray();
    rlEnableVertexArray(gpu.fieldVao);
    gpu.fieldVbo = rlLoadVertexBuffer(v.data(), (int)(v.size() * sizeof(float)), false);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, 0, 0);      // vertexPosition
    rlEnableVertexAttribute(0);
    rlDisableVertexArray();
}

void collect(std::vector<std::unique_ptr<Object3D>>& list, const Matrix& parent, std::vector<Item>& out,
             std::vector<Emitting>& emitters, double t, const Cull& k, std::deque<std::vector<Matrix>>& store,
             std::deque<std::vector<FieldTile>>& fieldStore) {
    for (auto& o : list) {
        if (!o->visible || o->culled) continue;
        Matrix world = MatrixMultiply(localMatrix(*o), parent);
        if (o->surface) world = surfaceWorld(*o, world, *k.cam, k.screenH);
        if (o->emitter && o->geometry) {
            const bool inWind = viewingScene && viewingScene->weather.enabled && !o->emitter->ownWind
                                && o->emitter->windFollow > 0;                   // the weather's wind, on a copy
            const fx::Emitter3D windy = inWind ? inWeather(*o->emitter, viewingScene->weather, world) : fx::Emitter3D{};
            fx::each(inWind ? windy : *o->emitter, t, [&](const fx::Particle3D& p) {
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
        else if (o->splats && o->splats->cloud) emitters.push_back({ o.get(), world });
        else if (o->lines && !o->lines->paths.empty()) emitters.push_back({ o.get(), world });
        else if (o->instances && o->instances->mesh) {
            store.emplace_back();
            gatherInstances(*o, world, k, store.back());
            if (!store.back().empty()) {
                Item it{ o.get(), world, 0 };
                it.inst = &store.back();
                out.push_back(it);
            }
        } else if (o->field) {
            fieldStore.emplace_back();
            gatherField(*o, world, k, fieldStore.back());
            if (!fieldStore.back().empty()) {
                Item it{ o.get(), world, 0 };
                it.tiles = &fieldStore.back();
                out.push_back(it);
            }
        } else if (o->geometry || o->model) out.push_back({ o.get(), world, 0 });
        collect(o->children, world, out, emitters, t, k, store, fieldStore);
    }
}

// mode: 0 not skinned, 1 linear blend, 2 dual quaternion (the shader's `skinned`).
void setBones(int loc, int skinnedLoc, const Shader& sh, const Model* model, int mode) {
    SetShaderValue(sh, skinnedLoc, &mode, SHADER_UNIFORM_INT);
    if (mode && loc >= 0) {
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
    SetShaderValue(sh, gpu.L.reflection, &m.reflection, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, gpu.L.emissive, &em, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, gpu.L.unlit, &unlit, SHADER_UNIFORM_INT);
    SetShaderValue(sh, gpu.L.opacity, &m.opacity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, gpu.L.receiveShadow, &receive, SHADER_UNIFORM_INT);
    const int weathers = m.weathering && !m.unlit ? 1 : 0;
    SetShaderValue(sh, gpu.wetMat, &weathers, SHADER_UNIFORM_INT);
    const float translucency = std::max(m.translucency, 0.f), normalScale = m.normalScale;
    const Vector3 tc = linear(m.translucencyColor);
    const int hasRough = m.roughnessMap.empty() ? 0 : 1, hasNormal = m.normalMap.empty() && !m.dynamicNormalMap ? 0 : m.normalAlphaShade ? 2 : 1;
    SetShaderValue(sh, gpu.L.translucency, &translucency, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, gpu.L.translucencyColor, &tc, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, gpu.L.hasRoughnessMap, &hasRough, SHADER_UNIFORM_INT);
    SetShaderValue(sh, gpu.L.hasNormalMap, &hasNormal, SHADER_UNIFORM_INT);
    SetShaderValue(sh, gpu.L.normalScale, &normalScale, SHADER_UNIFORM_FLOAT);

    const Vector3 cn = Vector3Length(m.clip.normal) > 1e-6f ? Vector3Normalize(m.clip.normal) : Vector3{ 1, 0, 0 };
    const Vector4 plane{ cn.x, cn.y, cn.z, m.clip.offset };
    const Vector3 edgeRgb = linear(m.clip.edgeColor, m.clip.edgeColor.a / 255.f);
    const Vector4 edge{ edgeRgb.x, edgeRgb.y, edgeRgb.z, m.clip.edgeColor.a > 0 ? std::max(m.clip.edgeWidth, 0.f) : 0.f };
    SetShaderValue(sh, gpu.C.plane, &plane, SHADER_UNIFORM_VEC4);
    SetShaderValue(sh, gpu.C.edge, &edge, SHADER_UNIFORM_VEC4);

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

// The cut-out and the extra texture maps, for any pass. The raylib material's maps array is
// shared by every draw, so each draw sets them all (white where a map is unused).
void setMaps(Pass pass, const g3d::Material& m, ::Material& mat) {
    const int p = (int)pass;
    const Shader& sh = pass == Pass::Lit ? gpu.lit : pass == Pass::Depth ? gpu.depth : gpu.gbuf;
    const float cut = std::max(m.alphaCutoff, 0.f);
    SetShaderValue(sh, gpu.cutoff[p], &cut, SHADER_UNIFORM_FLOAT);
    const bool lit = pass == Pass::Lit;
    mat.maps[MATERIAL_MAP_METALNESS].texture = lit && !m.roughnessMap.empty() ? texture(m.roughnessMap) : whiteTexture();
    mat.maps[MATERIAL_MAP_NORMAL].texture = lit ? normalMapOf(m) : whiteTexture();
}

void bindUnit(int slot, unsigned int id);

// The baked animation of a mesh as one float texture: positions in the top half, normals in
// the bottom, `width` texels to a row. Made on first use, kept until the GPU is released.
const Gpu::Vat& vatOf(const std::shared_ptr<AnimatedMesh>& am) {
    auto found = gpu.vat.find(am.get());
    if (found != gpu.vat.end()) return found->second;
    const size_t n = (size_t)am->frames * (size_t)am->vertices;
    if (n == 0 || am->positions.size() < n * 3 || am->normals.size() < n * 3)
        throw std::runtime_error("g3d: an AnimatedMesh needs frames * vertices positions and normals");
    Gpu::Vat v;
    v.keep = am;
    v.width = 2048;
    v.rows = (int)((n + (size_t)v.width - 1) / (size_t)v.width);
    if (v.rows * 2 > 16384) throw std::runtime_error("g3d: an AnimatedMesh is too large (frames * vertices)");
    std::vector<float> texels((size_t)v.width * (size_t)v.rows * 2 * 3, 0.f);
    std::copy(am->positions.begin(), am->positions.begin() + (std::ptrdiff_t)(n * 3), texels.begin());
    std::copy(am->normals.begin(), am->normals.begin() + (std::ptrdiff_t)(n * 3),
              texels.begin() + (std::ptrdiff_t)((size_t)v.width * (size_t)v.rows * 3));
    v.tex = rlLoadTexture(texels.data(), v.width, v.rows * 2, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32, 1);
    rlTextureParameters(v.tex, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
    rlTextureParameters(v.tex, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
    return gpu.vat.emplace(am.get(), std::move(v)).first->second;
}

// Whether the pass's program reads its vertices as instances; the value is kept in the
// program, so it is only set when it changes.
void setInstanced(Pass pass, bool on) {
    const int p = (int)pass, value = on ? 1 : 0;
    if (gpu.instState[p] == value) return;
    gpu.instState[p] = value;
    const Shader& sh = pass == Pass::Lit ? gpu.lit : pass == Pass::Depth ? gpu.depth : gpu.gbuf;
    SetShaderValue(sh, gpu.il[p].instanced, &value, SHADER_UNIFORM_INT);
}

// Draws an instanced item: every body that can show, in one call.
void drawInstances(const Item& it, Pass pass) {
    Object3D& o = *it.obj;
    const int p = (int)pass;
    const Shader& base = pass == Pass::Lit ? gpu.lit : pass == Pass::Depth ? gpu.depth : gpu.gbuf;
    const AnimatedMesh& am = *o.instances->mesh;
    if (!am.rest) return;
    const Gpu::Vat& vat = vatOf(o.instances->mesh);
    ::Material mat = gpu.material;
    mat.shader = gpu.inst[p];
    if (pass == Pass::Lit) setMaterial(o.material);
    if (o.material.doubleSided) rlDisableBackfaceCulling(); else rlEnableBackfaceCulling();
    mat.maps[MATERIAL_MAP_DIFFUSE].texture = mapOf(o.material);
    mat.maps[MATERIAL_MAP_DIFFUSE].color = o.material.color;
    setMaps(pass, o.material, mat);
    const int skinnedLoc = pass == Pass::Lit ? gpu.L.skinned : pass == Pass::Depth ? gpu.depthSkinned : gpu.G.skinned;
    setBones(pass == Pass::Lit ? gpu.L.bones : pass == Pass::Depth ? gpu.depthBones : gpu.G.bones, skinnedLoc, base, nullptr, 0);
    setInstanced(pass, true);
    const int unit = VAT_SLOT;
    SetShaderValue(base, gpu.il[p].vat, &unit, SHADER_UNIFORM_INT);
    SetShaderValue(base, gpu.il[p].width, &vat.width, SHADER_UNIFORM_INT);
    SetShaderValue(base, gpu.il[p].verts, &am.vertices, SHADER_UNIFORM_INT);
    SetShaderValue(base, gpu.il[p].rows, &vat.rows, SHADER_UNIFORM_INT);
    bindUnit(VAT_SLOT, vat.tex);
    if (pass == Pass::Lit) {
        gpu.instanceBodies += (long long)it.inst->size();
        gpu.instanceTriangles += (long long)it.inst->size() * am.rest->mesh().triangleCount;
    }
    DrawMeshInstanced(am.rest->mesh(), mat, it.inst->data(), (int)it.inst->size());
    bindUnit(VAT_SLOT, 0);
}

// Draws a field: each tile is one instanced draw of a blade template, the blades made in the
// vertex shader. Shadow passes draw only the tiles marked to cast.
void drawField(const Item& it, Pass pass) {
    const Object3D& o = *it.obj;
    const Field& own = *o.field;
    // With the weather enabled the wind settings come from it: a copy for this draw, never the user's field.
    const bool inWind = viewingScene && viewingScene->weather.enabled && !own.ownWind;
    Field resolved;
    if (inWind) resolved = inWeather(own, viewingScene->weather, &it.world);
    const Field& f = inWind ? resolved : own;
    const int p = (int)pass;
    const Shader& sh = pass == Pass::Lit ? gpu.lit : pass == Pass::Depth ? gpu.depth : gpu.gbuf;
    const FieldLocs& L = gpu.F[p];
    auto found = gpu.fields.find(&own);
    if (found == gpu.fields.end()) return;
    const Gpu::FieldMaps& maps = found->second;
    fieldTemplates();
    if (pass == Pass::Lit) setMaterial(o.material);
    setInstanced(pass, false);
    const int skinnedLoc = pass == Pass::Lit ? gpu.L.skinned : pass == Pass::Depth ? gpu.depthSkinned : gpu.G.skinned;
    const int notSkinned = 0;
    SetShaderValue(sh, skinnedLoc, &notSkinned, SHADER_UNIFORM_INT);
    rlDisableBackfaceCulling();

    rlEnableShader(sh.id);
    // What DrawMesh would set, for a mesh already in world space.
    const Matrix view = rlGetMatrixModelview(), proj = rlGetMatrixProjection();
    if (sh.locs[SHADER_LOC_MATRIX_VIEW] >= 0) rlSetUniformMatrix(sh.locs[SHADER_LOC_MATRIX_VIEW], view);
    if (sh.locs[SHADER_LOC_MATRIX_PROJECTION] >= 0) rlSetUniformMatrix(sh.locs[SHADER_LOC_MATRIX_PROJECTION], proj);
    if (sh.locs[SHADER_LOC_MATRIX_MODEL] >= 0) rlSetUniformMatrix(sh.locs[SHADER_LOC_MATRIX_MODEL], MatrixIdentity());
    if (sh.locs[SHADER_LOC_MATRIX_NORMAL] >= 0) rlSetUniformMatrix(sh.locs[SHADER_LOC_MATRIX_NORMAL], MatrixIdentity());
    rlSetUniformMatrix(sh.locs[SHADER_LOC_MATRIX_MVP], MatrixMultiply(view, proj));
    const Color mc = o.material.color;
    const float diffuse[4] = { mc.r / 255.f, mc.g / 255.f, mc.b / 255.f, mc.a / 255.f };
    if (sh.locs[SHADER_LOC_COLOR_DIFFUSE] >= 0) rlSetUniform(sh.locs[SHADER_LOC_COLOR_DIFFUSE], diffuse, SHADER_UNIFORM_VEC4, 1);
    const float cut = std::max(o.material.alphaCutoff, 0.f);
    rlSetUniform(gpu.cutoff[p], &cut, SHADER_UNIFORM_FLOAT, 1);
    const bool lit = pass == Pass::Lit;
    const unsigned int units[3] = {
        mapOf(o.material).id,
        lit && !o.material.roughnessMap.empty() ? texture(o.material.roughnessMap).id : rlGetTextureIdDefault(),
        lit ? normalMapOf(o.material).id : rlGetTextureIdDefault() };
    const int samplerLocs[3] = { sh.locs[SHADER_LOC_MAP_DIFFUSE], sh.locs[SHADER_LOC_MAP_METALNESS], sh.locs[SHADER_LOC_MAP_NORMAL] };
    for (int u = 0; u < 3; u++) {
        rlActiveTextureSlot(u);
        rlEnableTexture(units[u]);
        if (samplerLocs[u] >= 0) rlSetUniform(samplerLocs[u], &u, SHADER_UNIFORM_INT, 1);
    }
    rlActiveTextureSlot(VAT_SLOT);
    rlEnableTexture(maps.tex);
    rlActiveTextureSlot(0);

    auto set4 = [](int loc, float a, float b, float c, float d) { const float v[4] = { a, b, c, d }; rlSetUniform(loc, v, SHADER_UNIFORM_VEC4, 1); };
    auto setColor = [](int loc, Color c) { const float v[3] = { c.r / 255.f, c.g / 255.f, c.b / 255.f }; rlSetUniform(loc, v, SHADER_UNIFORM_VEC3, 1); };
    const int on = 1, mapUnit = VAT_SLOT, seed = (int)f.seed;
    rlSetUniform(L.on, &on, SHADER_UNIFORM_INT, 1);
    rlSetUniform(L.maps, &mapUnit, SHADER_UNIFORM_INT, 1);
    rlSetUniform(L.seed, &seed, SHADER_UNIFORM_INT, 1);
    rlSetUniformMatrix(L.world, it.world);
    set4(L.area, f.min.x, f.min.y, f.max.x, f.max.y);
    const float fit[2] = { (maps.res - 1.f) / maps.res, 0.5f / maps.res };
    rlSetUniform(L.mapFit, fit, SHADER_UNIFORM_VEC2, 1);
    set4(L.blade, std::max(f.height, 0.f), std::clamp(f.heightVariation, 0.f, 0.95f), std::max(f.width, 0.f), f.lean);
    Vector2 wd = f.windDirection;
    const float wl = std::sqrt(wd.x * wd.x + wd.y * wd.y);
    wd = wl > 1e-6f ? Vector2{ wd.x / wl, wd.y / wl } : Vector2{ 1, 0 };
    set4(L.wind, wd.x, wd.y, f.sway, f.gust);
    const float seconds = (float)(frameTime(gpu.frame) / std::max(f.fps, 1.f));
    set4(L.gusts, f.gustSpeed, std::max(f.gustScale, 0.01f), seconds, std::clamp(f.colorVariation, 0.f, 1.f));
    set4(L.fade, f.fadeStart, std::max(f.fadeEnd, f.fadeStart + 1e-3f), std::clamp(f.dryness, 0.f, 1.f),
         std::isfinite(f.grow) ? std::clamp(f.grow, 0.f, 1.f) : 1.f);
    const float cam[3] = { gpu.viewCam.x, gpu.viewCam.y, gpu.viewCam.z };
    rlSetUniform(L.cam, cam, SHADER_UNIFORM_VEC3, 1);
    setColor(L.root, f.root);
    setColor(L.tip, f.tip);
    setColor(L.dry, f.dry);
    const float bands = (float)std::clamp(f.colorBands, 0, 16);
    rlSetUniform(L.bands, &bands, SHADER_UNIFORM_FLOAT, 1);
    // Thinned blades in a shadow map are widened to keep the shadow's weight (at most 3 times).
    const float widen = pass == Pass::Depth ? std::min(3.f, 1.f / std::clamp(f.shadowDensity, 0.02f, 1.f)) : 1.f;
    rlSetUniform(L.widen, &widen, SHADER_UNIFORM_FLOAT, 1);

    const float ts = std::max(f.tileSize, 0.05f);
    const float full = (float)f.bladesPerTile();
    rlEnableVertexArray(gpu.fieldVao);
    for (const FieldTile& t : *it.tiles) {
        const int n = pass == Pass::Depth ? t.shadowCount : t.seen ? t.count : 0;
        if (n <= 0) continue;
        set4(L.tile, f.min.x + t.tx * ts, f.min.y + t.tz * ts, ts, full);
        const int key[2] = { t.tx, t.tz };
        rlSetUniform(L.key, key, SHADER_UNIFORM_IVEC2, 1);
        // A thinned shadow pass draws a random share of the tile's blades, not its first ones.
        const int pick = pass == Pass::Depth && n < t.count ? t.count : 0;
        rlSetUniform(L.pick, &pick, SHADER_UNIFORM_INT, 1);
        rlDrawVertexArrayInstanced(gpu.fieldFirst[t.segments], gpu.fieldCount[t.segments], n);
        if (pass == Pass::Lit) gpu.fieldBlades += n;
        else if (pass == Pass::Depth) gpu.fieldShadowBlades += n;
    }
    rlDisableVertexArray();
    const int off = 0;
    rlSetUniform(L.on, &off, SHADER_UNIFORM_INT, 1);
    for (int u : { 0, 1, 2, VAT_SLOT }) {
        rlActiveTextureSlot(u);
        rlDisableTexture();
    }
    rlActiveTextureSlot(0);
}

// Draws one item with the lit, depth (shadow maps) or line g-buffer shader.
void drawItem(const Item& it, Pass pass) {
    if (it.inst) { drawInstances(it, pass); return; }
    if (it.tiles) { drawField(it, pass); return; }
    setInstanced(pass, false);
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
    setMaps(pass, o.material, mat);

    if (o.geometry) {
        mat.maps[MATERIAL_MAP_DIFFUSE].texture = mapOf(o.material);
        const Color c = o.material.color, k = it.tint;
        mat.maps[MATERIAL_MAP_DIFFUSE].color = { (unsigned char)(c.r * k.r / 255), (unsigned char)(c.g * k.g / 255),
                                                 (unsigned char)(c.b * k.b / 255), c.a };
        setBones(bonesLoc, skinnedLoc, sh, nullptr, 0);
        DrawMesh(o.geometry->mesh(), mat, it.world);
        return;
    }
    Model& model = o.model->model();
    o.model->pose(o.animation, o.animationFrame);
    const Matrix world = MatrixMultiply(model.transform, it.world);
    for (int i = 0; i < model.meshCount; i++) {
        const Mesh& mesh = model.meshes[i];
        const int materialIndex = model.meshMaterial[i];
        const ::Material& own = model.materials[materialIndex];
        if (o.modelMaterials) {
            mat.maps[MATERIAL_MAP_DIFFUSE].texture = own.maps[MATERIAL_MAP_DIFFUSE].texture.id
                ? own.maps[MATERIAL_MAP_DIFFUSE].texture : whiteTexture();
            Color oc = own.maps[MATERIAL_MAP_DIFFUSE].color, tint = o.material.color;
            const Color slotTint = materialIndex >= 0 && materialIndex < (int)o.modelMaterialTints.size()
                ? o.modelMaterialTints[(size_t)materialIndex] : WHITE;
            mat.maps[MATERIAL_MAP_DIFFUSE].color = { (unsigned char)(oc.r * tint.r * slotTint.r / (255 * 255)),
                                                     (unsigned char)(oc.g * tint.g * slotTint.g / (255 * 255)),
                                                     (unsigned char)(oc.b * tint.b * slotTint.b / (255 * 255)),
                                                     (unsigned char)(oc.a * tint.a * slotTint.a / (255 * 255)) };
        } else {
            mat.maps[MATERIAL_MAP_DIFFUSE].texture = mapOf(o.material);
            mat.maps[MATERIAL_MAP_DIFFUSE].color = o.material.color;
        }
        bool skinned = mesh.boneIndices && mesh.boneWeights && model.boneMatrices && model.skeleton.boneCount > 0;
        setBones(bonesLoc, skinnedLoc, sh, &model, skinned ? (o.model->dualQuaternion ? 2 : 1) : 0);
        DrawMesh(mesh, mat, world);
    }
}

Vector3 safeUp(Vector3 dir, Vector3 up) {
    return std::fabs(Vector3DotProduct(Vector3Normalize(dir), Vector3Normalize(up))) > 0.999f ? Vector3{ 0, 0, 1 } : up;
}

// ---- 2D surfaces -----------------------------------------------------------------------
// The rectangle in the surface's own space, metres: x0..x1 left to right, y0..y1 bottom to top.
struct SurfaceRect { float x0, x1, y0, y1; };
SurfaceRect surfaceRect(const Surface& s) {
    const float x0 = -s.anchor.x * s.size.x, y1 = s.anchor.y * s.size.y;
    return { x0, x0 + s.size.x, y1 - s.size.y, y1 };
}
Vector3 column(const Matrix& m, int i) {
    return i == 0 ? Vector3{ m.m0, m.m1, m.m2 } : i == 1 ? Vector3{ m.m4, m.m5, m.m6 } : Vector3{ m.m8, m.m9, m.m10 };
}

Matrix surfaceWorld(const Object3D& o, const Matrix& world, const Camera& cam, float screenH) {
    const Surface& s = *o.surface;
    if (!s.billboard && s.lift == 0) return world;
    const Vector3 look = Vector3Subtract(cam.target, cam.position);
    const Vector3 f = Vector3Normalize(look);
    Vector3 p{ world.m12, world.m13, world.m14 };
    if (s.lift != 0) {                       // along the line of sight, so it stays where it was on screen
        const Vector3 toward = cam.orthographic ? Vector3Negate(f) : Vector3Normalize(Vector3Subtract(cam.position, p));
        p = Vector3Add(p, Vector3Scale(toward, s.lift));
    }
    Matrix m = world;
    if (s.billboard) {
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(f, safeUp(look, cam.upVector)));
        const Vector3 up = Vector3CrossProduct(right, f);
        float k = Vector3Length(column(world, 1));
        if (s.pixelSize > 0) {
            const float depth = std::max(cam.nearPlane, Vector3DotProduct(Vector3Subtract(p, cam.position), f));
            const float metresPerPx = cam.orthographic ? cam.orthoHeight / screenH
                                                       : 2 * depth * std::tan(cam.fov * 0.5f * DEG2RAD) / screenH;
            k = s.pixelSize * s.pixels.y * metresPerPx / std::max(1e-6f, s.size.y);
        }
        m = MatrixIdentity();
        m.m0 = right.x * k; m.m1 = right.y * k; m.m2 = right.z * k;
        m.m4 = up.x * k;    m.m5 = up.y * k;    m.m6 = up.z * k;
        m.m8 = -f.x * k;    m.m9 = -f.y * k;    m.m10 = -f.z * k;
    }
    m.m12 = p.x; m.m13 = p.y; m.m14 = p.z;
    return m;
}

// Screen px per design px where the surface is densest on screen (its nearest edge), or 0 when
// a corner is behind the camera (the caller then takes the most it allows).
float surfaceDensity(const Surface& s, const Matrix& m, const Camera& cam, float w, float h) {
    const SurfaceRect r = surfaceRect(s);
    const Vector3 local[4] = { { r.x0, r.y1, 0 }, { r.x1, r.y1, 0 }, { r.x1, r.y0, 0 }, { r.x0, r.y0, 0 } };
    Vector2 q[4];
    for (int i = 0; i < 4; i++)
        if (!project(cam, Vector3Transform(local[i], m), { 0, 0, w, h }, q[i])) return 0;
    const float across = std::max(Vector2Distance(q[0], q[1]), Vector2Distance(q[3], q[2])) / std::max(1.f, s.pixels.x);
    const float down = std::max(Vector2Distance(q[0], q[3]), Vector2Distance(q[1], q[2])) / std::max(1.f, s.pixels.y);
    return std::max(across, down);
}

// Each visible surface's texture size for this frame, from how large it shows.
void sizeSurfaces(std::vector<std::unique_ptr<Object3D>>& list, const Matrix& parent, const Camera& cam, float w, float h) {
    constexpr float MAX_TEXTURE = 8192;
    for (auto& o : list) {
        if (!o->visible || o->culled) continue;
        Matrix world = MatrixMultiply(localMatrix(*o), parent);
        if (o->surface && o->material.dynamicMap) {
            Surface& s = *o->surface;
            world = surfaceWorld(*o, world, cam, h);
            const float hi = std::max(s.minResolution, s.maxResolution);
            const float density = surfaceDensity(s, world, cam, w, h);
            float res = density > 0 ? density * std::max(0.01f, s.sharpness) : hi;
            res = std::exp2(std::ceil(std::log2(std::max(res, 1e-4f)) * 4 - 1e-3f) / 4);   // quarter octaves, rounded up
            res = std::clamp(res, std::min(s.minResolution, hi), hi);
            const float big = std::max(1.f, std::max(s.pixels.x, s.pixels.y));
            const int ss = res >= 2 ? 1 : 2;     // a denser texture is already supersampled
            res = std::min(res, MAX_TEXTURE / (big * (float)ss));
            s.resolution = res;
            DynamicTexture& tex = *o->material.dynamicMap;
            tex.supersample = ss;
            tex.width = std::max(1, (int)std::ceil(s.pixels.x * res - 1e-3f));
            tex.height = std::max(1, (int)std::ceil(s.pixels.y * res - 1e-3f));
        }
        sizeSurfaces(o->children, world, cam, w, h);
    }
}

// A quad facing +z with the picture's top-left at uv 0,0 (canvas textures run top-down).
GeometryPtr surfaceQuad(const Surface& s) {
    const SurfaceRect r = surfaceRect(s);
    return std::make_shared<Geometry>([r] {
        Mesh m{};
        m.vertexCount = 4;
        m.triangleCount = 2;
        m.vertices = (float*)MemAlloc(12 * sizeof(float));
        m.normals = (float*)MemAlloc(12 * sizeof(float));
        m.texcoords = (float*)MemAlloc(8 * sizeof(float));
        m.indices = (unsigned short*)MemAlloc(6 * sizeof(unsigned short));
        const float v[12] = { r.x0, r.y1, 0, r.x1, r.y1, 0, r.x1, r.y0, 0, r.x0, r.y0, 0 };
        const float uv[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
        const unsigned short idx[6] = { 3, 2, 1, 3, 1, 0 };
        for (int i = 0; i < 12; i++) { m.vertices[i] = v[i]; m.normals[i] = i % 3 == 2 ? 1.f : 0.f; }
        for (int i = 0; i < 8; i++) m.texcoords[i] = uv[i];
        for (int i = 0; i < 6; i++) m.indices[i] = idx[i];
        UploadMesh(&m, false);
        return m;
    });
}

void makeSurface(Object3D& o, std::shared_ptr<Surface> sp) {
    if (!sp) throw std::invalid_argument("g3d: add(Surface) needs a surface");
    auto tex = std::make_shared<DynamicTexture>();
    tex->width = std::max(1, (int)sp->pixels.x);
    tex->height = std::max(1, (int)sp->pixels.y);
    Surface* raw = sp.get();                 // the object owns the surface for as long as the texture
    tex->draw = [raw](const Ctx& c, int t) {
        Surface& s = *raw;
        Ctx d = c;
        d.s = c.s * s.resolution;
        const int lt = t - s.start;
        if (s.background.a > 0) dr::rect(d, 0, 0, s.pixels.x, s.pixels.y, s.background);
        if (s.draw) s.draw(d, lt);
        for (auto& n : s.nodes) stepNode(*n, lt, d);
    };
    o.geometry = surfaceQuad(*sp);
    o.material = Material{};
    o.material.unlit = true;
    o.material.castShadow = false;
    o.material.receiveShadow = false;
    o.material.opacity = sp->background.a < 255 ? 0.999f : 1.f;
    o.material.dynamicMap = std::move(tex);
    o.surface = std::move(sp);
}
} // namespace

bool visibleSphere(const Camera& cam, float aspect, Vector3 c, float r) {
    const Vector3 look = Vector3Subtract(cam.target, cam.position);
    const Vector3 f = Vector3Normalize(look);
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(f, safeUp(look, cam.upVector)));
    const Vector3 up = Vector3CrossProduct(right, f);
    const Vector3 v = Vector3Subtract(c, cam.position);
    const float z = Vector3DotProduct(v, f), x = Vector3DotProduct(v, right), y = Vector3DotProduct(v, up);
    if (z + r < cam.nearPlane || z - r > cam.farPlane) return false;
    constexpr float margin = 1.03f;           // the viewport's aspect is rounded to pixels
    if (cam.orthographic) {
        const float halfH = cam.orthoHeight * 0.5f * margin;
        return std::fabs(x) <= halfH * aspect + r && std::fabs(y) <= halfH + r;
    }
    const float ty = std::tan(cam.fov * 0.5f * DEG2RAD) * margin, tx = ty * aspect;
    // The side planes pass through the eye: a sphere meets a half-space when its centre is
    // within r of the plane, measured along the plane's unit normal.
    if ((std::fabs(x) - z * tx) / std::sqrt(1 + tx * tx) > r) return false;
    if ((std::fabs(y) - z * ty) / std::sqrt(1 + ty * ty) > r) return false;
    return true;
}

namespace {

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
            if (it.obj->material.castShadow && opacityOf(it) >= 1.f && !it.obj->material.unlit &&
                !(it.tiles && !it.obj->field->castShadows))
                drawItem(it, Pass::Depth);
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

// A water's waves as the shader reads them: (direction.xy, wavenumber, amplitude) and
// (angular speed, phase, sharpness), with deep-water dispersion (omega^2 = g k).
struct PackedWaves {
    int count = 0;
    Vector4 a[8] = {}, b[8] = {};
    float amplitude = 0;        // the sum of the amplitudes
};
PackedWaves packWaves(const Water& w) {
    PackedWaves p;
    for (const Water::Wave& v : w.waves) {
        if (p.count == 8) break;
        const float k = 2 * PI / std::max(v.wavelength, 0.05f), d = v.direction * DEG2RAD;
        p.a[p.count] = { std::cos(d), std::sin(d), k, v.amplitude };
        p.b[p.count] = { std::sqrt(9.81f * k) * v.speed * w.speed, 1.7f * (float)p.count + 0.3f, std::max(v.sharpness, 1.f), 0 };
        p.amplitude += std::fabs(v.amplitude);
        p.count++;
    }
    return p;
}

void setWaterLook(const Scene& s, const Camera& cam, const Water& w, float eye, double seconds);
void setWakeUniforms(const Scene& s, double seconds);

// ---- the spectrum sea (Water::spectrum): the cascades as textures on units OCEAN_SLOT.., and the uniforms
void bindUnit(int slot, unsigned int id);
constexpr int OCEAN_SLOT = 16;                         // cascade c: A on 16 + 2c, B on 17 + 2c
constexpr int OCEAN_RINGS = 160, OCEAN_SEGMENTS = 256;  // the mesh
constexpr float OCEAN_R0 = 0.2f;                        // its inner ring (m)

// The horizon's distance for an eye `above` metres over the sea (the tangent point of the sphere), and the sea mesh's
// reach and the far plane that go with it.
float seaHorizon(float above) { const double R = Water::kPlanetRadius, h = std::max(above, 0.5f); return (float)std::sqrt(2 * R * h + h * h); }
float seaReach(const Water& w, float above) { return w.spectrum.curvature ? 1.4f * seaHorizon(above) + 2000.f : 12000.f; }

void uploadOcean(const ocean::Frame& f) {
    Gpu::OceanGpu& o = gpu.ocean;
    if (o.id == f.id) return;
    for (int c = 0; c < f.cascades; c++)
        for (int k = 0; k < 2; k++) {
            unsigned& id = o.tex[c][k];
            const std::vector<float>& data = k == 0 ? f.a[c] : f.b[c];
            if (!id) {
                id = rlLoadTexture(nullptr, ocean::kN, ocean::kN, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
                rlTextureParameters(id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
                rlTextureParameters(id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_REPEAT);
                rlTextureParameters(id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
                rlTextureParameters(id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_LINEAR);
            }
            rlUpdateTexture(id, 0, 0, ocean::kN, ocean::kN, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, data.data());
            int mips = 0;
            rlGenTextureMipmaps(id, ocean::kN, ocean::kN, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, &mips);
        }
    o.id = f.id;
    o.cascades = f.cascades;
}

// How much the sea looks like a gale: Water::Spectrum::storm, or from the wind (0 at 12 m/s, 1 at 20) when it is -1.
float stormOf(const Water& w) {
    if (w.spectrum.storm >= 0) return std::min(w.spectrum.storm, 1.f);
    const float t = std::clamp((w.spectrum.windSpeed - 12.f) / 8.f, 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

void setOceanUniforms(const Water& w, const Camera& cam, double seconds) {
    const Shader& lit = gpu.lit;
    auto at = [&](const char* name) { return GetShaderLocation(lit, name); };
    const int on = w.enabled && w.spectrum.enabled ? 1 : 0;
    SetShaderValue(lit, at("oceanOn"), &on, SHADER_UNIFORM_INT);
    if (!on) return;
    const std::shared_ptr<const ocean::Frame> fr = ocean::frameAt(w, seconds);
    const ocean::Frame& f = *fr;
    uploadOcean(f);
    for (int c = 0; c < ocean::kMaxCascades; c++)
        for (int k = 0; k < 2; k++) {
            const int unit = OCEAN_SLOT + 2 * c + k;
            SetShaderValue(lit, at(TextFormat("ocean%c%d", k == 0 ? 'A' : 'B', c)), &unit, SHADER_UNIFORM_INT);
        }
    Vector4 patch{ 1, 1, 1, 1 }, warp[4] = {};
    float lost[32] = {};
    for (int c = 0; c < f.cascades; c++) {
        (&patch.x)[c] = f.patch[c];
        warp[c] = { f.warp[c].amp, f.warp[c].a, f.warp[c].b, 0 };
        for (int l = 0; l < ocean::kLevels; l++) lost[c * 8 + l] = f.lost[c][l];
    }
    const float curve = w.spectrum.curvature ? 0.5f / Water::kPlanetRadius : 0.f;
    const float above = std::max(cam.position.y - w.level, 0.f), r1 = seaReach(w, above);
    const Vector3 ring{ OCEAN_R0, r1, std::log(r1 / OCEAN_R0) / (OCEAN_RINGS - 1) };
    const float heading = w.spectrum.windDirection * DEG2RAD;
    const Vector2 wind{ std::cos(heading), std::sin(heading) };
    const float U = w.spectrum.windSpeed, t = std::clamp((U - 12.f) / 8.f, 0.f, 1.f);
    const float storm = stormOf(w);
    const float spray = std::clamp(w.spectrum.spray, 0.f, 1.f) * std::max(t * t * (3 - 2 * t), storm);
    const float vis = std::max(w.spectrum.visibility, 0.f) * (1.f - 0.45f * storm);      // spray hangs over a gale
    const int count = f.cascades, geo = f.geometry;
    SetShaderValue(lit, at("oceanCount"), &count, SHADER_UNIFORM_INT);
    SetShaderValue(lit, at("oceanGeo"), &geo, SHADER_UNIFORM_INT);
    SetShaderValue(lit, at("oceanLambda"), &f.lambda, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, at("oceanCurve"), &curve, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, at("oceanTail"), &f.tail, SHADER_UNIFORM_FLOAT);
    const float skyLod = std::clamp(std::log2(1.f + 80.f * (f.resolved + f.tail)), 0.f, 5.f);
    SetShaderValue(lit, at("oceanSkyLod"), &skyLod, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, at("oceanPatch"), &patch, SHADER_UNIFORM_VEC4);
    SetShaderValueV(lit, at("oceanWarp"), warp, SHADER_UNIFORM_VEC4, 4);
    SetShaderValueV(lit, at("oceanLost"), lost, SHADER_UNIFORM_FLOAT, 32);
    SetShaderValue(lit, at("oceanRing"), &ring, SHADER_UNIFORM_VEC3);
    SetShaderValue(lit, at("oceanWind"), &wind, SHADER_UNIFORM_VEC2);
    SetShaderValue(lit, at("oceanSpray"), &spray, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, at("oceanStorm"), &storm, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, at("oceanVis"), &vis, SHADER_UNIFORM_FLOAT);
}

// Binds (or clears) the cascades' textures for the sea's draw.
void bindOceanTextures(bool on) {
    for (int c = 0; c < ocean::kMaxCascades; c++)
        for (int k = 0; k < 2; k++) bindUnit(OCEAN_SLOT + 2 * c + k, on && c < gpu.ocean.cascades ? gpu.ocean.tex[c][k] : 0);
}

// Everything the lit shader needs of the sea: the waves, the water's fog and the light
// that reaches under it.
void setWaterUniforms(const Scene& s, const Camera& cam) {
    const Water w = waterOf(s);                 // with the weather's wind worked in, if it is enabled
    const WaterLocs& K = gpu.W;
    const Shader& lit = gpu.lit;
    const int on = w.enabled ? 1 : 0, mode = 0, none = 0;
    SetShaderValue(lit, K.on, &on, SHADER_UNIFORM_INT);
    SetShaderValue(lit, K.mode, &mode, SHADER_UNIFORM_INT);
    SetShaderValue(lit, K.reflClip, &none, SHADER_UNIFORM_INT);         // the mirrored pass and the water draw switch these on
    SetShaderValue(lit, K.reflMode, &none, SHADER_UNIFORM_INT);
    SetShaderValue(lit, K.refrMode, &none, SHADER_UNIFORM_INT);
    SetShaderValue(lit, GetShaderLocation(lit, "oceanOn"), &none, SHADER_UNIFORM_INT);
    if (!on) return;
    const double seconds = frameTime(gpu.frame) / std::max(w.fps, 1.f);   // frames to seconds
    setOceanUniforms(w, cam, seconds);
    setWaterLook(s, cam, w, w.height(cam.position.x, cam.position.z, seconds), seconds);      // height() includes the level
}

// The water look's uniforms of the lit shader (a sea or a body): `eye` is the surface height the camera
// is compared with to tell above from below.
void setWaterLook(const Scene& s, const Camera& cam, const Water& w, float eye, double seconds) {
    const WaterLocs& K = gpu.W;
    const Shader& lit = gpu.lit;
    const PackedWaves pk = packWaves(w);
    const float time = (float)seconds;
    const Vector2 centre{ cam.position.x, cam.position.z };
    SetShaderValue(lit, K.level, &w.level, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, K.eye, &eye, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, K.time, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, K.centre, &centre, SHADER_UNIFORM_VEC2);
    SetShaderValue(lit, K.count, &pk.count, SHADER_UNIFORM_INT);
    SetShaderValueV(lit, K.waveA, pk.a, SHADER_UNIFORM_VEC4, 8);
    SetShaderValueV(lit, K.waveB, pk.b, SHADER_UNIFORM_VEC4, 8);
    SetShaderValue(lit, K.absorb, &w.absorb, SHADER_UNIFORM_VEC3);
    // The water scatters back the daylight that reaches it, dimmer the deeper the eye is.
    float daylight = s.ambient * 0.5f;
    for (const Light& l : s.lights) {
        if (l.type != Light::Type::Directional) continue;
        const Vector3 d = Vector3Normalize(Vector3Subtract(l.target, l.position));
        const Vector3 c = linear(l.color, l.intensity);
        daylight += (0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z) * std::clamp(-d.y, 0.f, 1.f) * 0.6f;
    }
    const float meanAbsorb = (w.absorb.x + w.absorb.y + w.absorb.z) / 3;
    const float fade = std::exp(-meanAbsorb * std::max(0.f, w.level - cam.position.y) * 0.5f);
    Vector3 scatter = Vector3Scale(linear(w.deep), std::clamp(daylight, 0.03f, 2.f) * fade);
    if (w.enabled && w.spectrum.enabled) {        // a gale scatters back the grey sky: darker and greyer
        const float st = stormOf(w), lum = 0.2126f * scatter.x + 0.7152f * scatter.y + 0.0722f * scatter.z;
        scatter = Vector3Scale(Vector3Lerp(scatter, { lum * 0.9f, lum, lum * 1.05f }, 0.55f * st), 1.f - 0.4f * st);
    }
    SetShaderValue(lit, K.scatter, &scatter, SHADER_UNIFORM_VEC3);
    const float caustics = std::clamp(w.caustics, 0.f, 1.f), foam = std::clamp(w.foam, 0.f, 1.f);
    const float open = std::max(w.openDepth, 0.f), invAmp = 1.f / std::max(pk.amplitude, 1e-3f), band = pk.amplitude + 0.05f;
    SetShaderValue(lit, K.caustics, &caustics, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, K.foam, &foam, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, K.open, &open, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, K.invAmp, &invAmp, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, K.glint, &w.sunGlint, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, K.band, &band, SHADER_UNIFORM_FLOAT);
    const float filterOn = w.filter ? 1.f : 0.f;
    SetShaderValue(lit, K.filter, &filterOn, SHADER_UNIFORM_FLOAT);
    { const int dbg = s.ground.debug; SetShaderValue(lit, GetShaderLocation(lit, "bodyDebug"), &dbg, SHADER_UNIFORM_INT); }
    setWakeUniforms(s, seconds);
}

// Scene::wakes and Scene::ripples as the shader reads them (wake_glsl.hpp): the emission tables of the first four
// wakes and the first sixteen ripples, at `seconds`. With none, only the counts are set (0).
void setWakeUniforms(const Scene& s, double seconds) {
    const Shader& lit = gpu.lit;
    static const int kWakeMax = 4, kRippleMax = 16;
    int wakes = 0, ripples = 0;
    Vector4 a[4] = {}, b[4] = {}, boat[4] = {};
    static std::vector<Vector4> table;
    table.assign((size_t)kWakeMax * Wake::kSamples, Vector4{ 0, 0, 0, 0 });
    for (const Wake& w : s.wakes) {
        if (wakes >= kWakeMax) break;
        if (w.path.size() < 2 || w.speed <= 0) continue;
        w.emissions(seconds, &table[(size_t)wakes * Wake::kSamples]);
        a[wakes] = { w.length, std::max(w.beam, 0.05f), w.speed, w.amplitude() };
        b[wakes] = { std::max(w.life, 1.f), std::clamp(w.foam, 0.f, 1.f), std::max(w.trail, 0.f), 0.f };
        Vector2 p, h;
        boat[wakes] = w.at(seconds, &p, &h) ? Vector4{ p.x, p.y, h.x, h.y } : Vector4{ 0, 0, 0, 0 };
        wakes++;
    }
    Vector4 ra[16] = {}, rb[16] = {};
    for (const Ripple& r : s.ripples) {
        if (ripples >= kRippleMax) break;
        ra[ripples] = { r.x, r.z, (float)(seconds - r.time), r.strength };
        rb[ripples] = { r.size, r.life, 0.f, 0.f };
        ripples++;
    }
    auto at = [&](const char* name) { return GetShaderLocation(lit, name); };
    SetShaderValue(lit, at("wakeCount"), &wakes, SHADER_UNIFORM_INT);
    SetShaderValue(lit, at("rippleCount"), &ripples, SHADER_UNIFORM_INT);
    if (wakes) {
        SetShaderValueV(lit, at("wakeA"), a, SHADER_UNIFORM_VEC4, 4);
        SetShaderValueV(lit, at("wakeB"), b, SHADER_UNIFORM_VEC4, 4);
        SetShaderValueV(lit, at("wakeBoat"), boat, SHADER_UNIFORM_VEC4, 4);
        SetShaderValueV(lit, at("wakeE"), table.data(), SHADER_UNIFORM_VEC4, kWakeMax * Wake::kSamples);
    }
    if (ripples) {
        SetShaderValueV(lit, at("rippleA"), ra, SHADER_UNIFORM_VEC4, 16);
        SetShaderValueV(lit, at("rippleB"), rb, SHADER_UNIFORM_VEC4, 16);
    }
}

// The sea's surface: rings that thicken with distance round a point (the camera, which the
// shader moves it to), so waves are well sampled near the eye and the surface reaches the horizon.
Mesh makeWaterMesh() {
    constexpr int R = 96, S = 192;
    constexpr float r0 = 0.3f, r1 = 1500.f;
    Mesh m = {};
    m.vertexCount = 1 + R * S;
    m.triangleCount = S + (R - 1) * S * 2;
    m.vertices = (float*)MemAlloc((unsigned)m.vertexCount * 3 * sizeof(float));
    m.normals = (float*)MemAlloc((unsigned)m.vertexCount * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc((unsigned)m.vertexCount * 2 * sizeof(float));
    m.indices = (unsigned short*)MemAlloc((unsigned)m.triangleCount * 3 * sizeof(unsigned short));
    for (int v = 0; v < m.vertexCount; v++) {
        m.normals[v * 3 + 1] = 1;
        m.normals[v * 3] = m.normals[v * 3 + 2] = 0;
    }
    for (int i = 0; i < R; i++) {
        const float r = r0 * std::pow(r1 / r0, (float)i / (R - 1));
        for (int j = 0; j < S; j++) {
            const float a = 2 * PI * j / S;
            const int v = 1 + i * S + j;
            m.vertices[v * 3] = r * std::cos(a);
            m.vertices[v * 3 + 1] = 0;
            m.vertices[v * 3 + 2] = r * std::sin(a);
        }
    }
    int k = 0;
    auto tri = [&](int a, int b, int c) {
        m.indices[k++] = (unsigned short)a; m.indices[k++] = (unsigned short)b; m.indices[k++] = (unsigned short)c;
    };
    for (int j = 0; j < S; j++) tri(0, 1 + (j + 1) % S, 1 + j);
    for (int i = 0; i + 1 < R; i++)
        for (int j = 0; j < S; j++) {
            const int a = 1 + i * S + j, b = 1 + i * S + (j + 1) % S, c = a + S, d = b + S;
            tri(a, b, c);
            tri(b, d, c);
        }
    UploadMesh(&m, false);
    return m;
}
// The spectrum sea's mesh: unit rings (cos a, w, sin a) whose radius the vertex shader sets from w (0 = the inner
// ring, 1 = the outer, on a log scale), so the sea reaches as far as the horizon needs and no further.
Mesh makeOceanMesh() {
    constexpr int R = OCEAN_RINGS, S = OCEAN_SEGMENTS;
    Mesh m = {};
    m.vertexCount = 1 + R * S;
    m.triangleCount = S + (R - 1) * S * 2;
    m.vertices = (float*)MemAlloc((unsigned)m.vertexCount * 3 * sizeof(float));
    m.normals = (float*)MemAlloc((unsigned)m.vertexCount * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc((unsigned)m.vertexCount * 2 * sizeof(float));
    m.indices = (unsigned short*)MemAlloc((unsigned)m.triangleCount * 3 * sizeof(unsigned short));
    for (int v = 0; v < m.vertexCount; v++) {
        m.normals[v * 3 + 1] = 1;
        m.normals[v * 3] = m.normals[v * 3 + 2] = 0;
    }
    for (int i = 0; i < R; i++)
        for (int j = 0; j < S; j++) {
            const float a = 2 * PI * j / S;
            const int v = 1 + i * S + j;
            m.vertices[v * 3] = std::cos(a);
            m.vertices[v * 3 + 1] = (float)i / (R - 1);
            m.vertices[v * 3 + 2] = std::sin(a);
        }
    int k = 0;
    auto tri = [&](int a, int b, int c) {
        m.indices[k++] = (unsigned short)a; m.indices[k++] = (unsigned short)b; m.indices[k++] = (unsigned short)c;
    };
    for (int j = 0; j < S; j++) tri(0, 1 + (j + 1) % S, 1 + j);
    for (int i = 0; i + 1 < R; i++)
        for (int j = 0; j < S; j++) {
            const int a = 1 + i * S + j, b = 1 + i * S + (j + 1) % S, c = a + S, d = b + S;
            tri(a, b, c);
            tri(b, d, c);
        }
    UploadMesh(&m, false);
    return m;
}
Object3D& oceanObject() {
    if (!gpu.oceanObj) {
        gpu.oceanObj = std::make_unique<Object3D>();
        gpu.oceanObj->name = "ocean";
        gpu.oceanObj->geometry = std::make_shared<Geometry>([] { return makeOceanMesh(); });
        Material& m = gpu.oceanObj->material;
        m.doubleSided = true;
        m.castShadow = false;
    }
    return *gpu.oceanObj;
}
Object3D& waterObject() {
    if (!gpu.waterObj) {
        gpu.waterObj = std::make_unique<Object3D>();
        gpu.waterObj->name = "water";
        gpu.waterObj->geometry = std::make_shared<Geometry>([] { return makeWaterMesh(); });
        Material& m = gpu.waterObj->material;
        m.doubleSided = true;
        m.castShadow = false;
    }
    return *gpu.waterObj;
}

void setSceneUniforms(const Scene& s, const Camera& cam, const Shadows& sh, Texture2D envTex, float scale) {
    setWaterUniforms(s, cam);
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
    const int fogMode = (int)s.fogMode, fogSky = s.fogFromSky ? 1 : 0;
    SetShaderValue(lit, L.fogMode, &fogMode, SHADER_UNIFORM_INT);
    SetShaderValue(lit, L.fogDensity, &s.fogDensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L.fogHeight, &s.fogHeight, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L.fogFalloff, &s.fogFalloff, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L.fogSky, &fogSky, SHADER_UNIFORM_INT);
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
    // A sky is in world space (no rotation); it lights by `lighting` but is seen (backdrop,
    // fog) as it is.
    float rot = s.sky.enabled ? 0.f : s.environmentRotation / 360.f;
    const float intensity = s.sky.enabled ? s.sky.lighting : s.environmentIntensity;
    const float backdrop = s.sky.enabled ? 1.f / std::max(intensity, 1e-4f) : 1.f;
    SetShaderValue(lit, L.hasEnv, &hasEnv, SHADER_UNIFORM_INT);
    SetShaderValue(lit, L.envLinear, &envLin, SHADER_UNIFORM_INT);
    SetShaderValue(lit, L.envMaxLod, &maxLod, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L.envIntensity, &intensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L.backdropScale, &backdrop, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L.envRotation, &rot, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lit, L.skybox, &sky0, SHADER_UNIFORM_INT);
    const CloudGpu& cg = gpu.cl;
    const int cloudOn = cg.shadowOn ? 1 : 0;
    SetShaderValue(lit, gpu.cll.on, &cloudOn, SHADER_UNIFORM_INT);
    if (cloudOn) {
        SetShaderValue(lit, gpu.cll.light, &cg.shadowLight, SHADER_UNIFORM_INT);
        SetShaderValue(lit, gpu.cll.map, &cg.shadowMap, SHADER_UNIFORM_VEC4);
        SetShaderValue(lit, gpu.cll.sun, &cg.shadowSun, SHADER_UNIFORM_VEC3);
        SetShaderValue(lit, gpu.cll.floor, &cg.shadowFloor, SHADER_UNIFORM_FLOAT);
    }
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
    setBones(gpu.L.bones, gpu.L.skinned, gpu.lit, nullptr, 0);
    setInstanced(Pass::Lit, false);                     // a forest drawn before may have left it on
    const float size = cam.farPlane;                 // corners stay inside the far plane
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    DrawMesh(gpu.skyCube, mat, MatrixMultiply(MatrixScale(size, size, size),
                                              MatrixTranslate(cam.position.x, cam.position.y, cam.position.z)));
    rlEnableDepthMask();
    SetShaderValue(gpu.lit, gpu.L.skybox, &zero, SHADER_UNIFORM_INT);
}

// Blurs `src` by depth into the currently bound target (2D state, composition units).
void compositeDof(const DepthTarget& src, const Camera& cam, float scale, Vector2 viewY) {
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
    const float tanTilt = std::tan(std::clamp(d.tilt, -80.f, 80.f) * DEG2RAD), tanHalf = std::tan(cam.fov * 0.5f * DEG2RAD);
    SetShaderValue(gpu.dof, D.tanTilt, &tanTilt, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.dof, D.tanHalfFov, &tanHalf, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gpu.dof, D.viewY, &viewY, SHADER_UNIFORM_VEC2);
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

enum Shape { DOT, GLOW, GLOW_HOT, STREAK, FLAT, STAR, RING, STROKE, HEAD };

// `v` turned and scaled by `m`, without its translation.
Vector3 direction(Vector3 v, const Matrix& m) {
    return { m.m0 * v.x + m.m4 * v.y + m.m8 * v.z, m.m1 * v.x + m.m5 * v.y + m.m9 * v.z, m.m2 * v.x + m.m6 * v.y + m.m10 * v.z };
}

// The lit shader's fogAmount(), for colours made on the CPU.
float fogAmount(const Scene& s, Vector3 eye, Vector3 p) {
    const Vector3 ray = Vector3Subtract(p, eye);
    const float d = Vector3Length(ray);
    if (s.fogMode == FogMode::Linear)
        return s.fogFar > s.fogNear ? std::clamp((d - s.fogNear) / (s.fogFar - s.fogNear), 0.f, 1.f) : 0.f;
    if (s.fogDensity <= 0) return 0;
    float od = s.fogDensity * d;
    if (s.fogFalloff > 0) {
        const float a = s.fogFalloff * ray.y;
        const float g = std::fabs(a) > 1e-4f ? (1 - std::exp(-a)) / a : 1 - 0.5f * a;
        od *= std::exp(-s.fogFalloff * (eye.y - s.fogHeight)) * g;
    }
    return s.fogMode == FogMode::Exponential ? 1 - std::exp(-od) : 1 - std::exp(-od * od);
}

Color fogged(const Scene& s, Color c, Vector3 eye, Vector3 p, bool additive) {
    const float f = fogAmount(s, eye, p);
    if (f <= 0) return c;
    const Color tint = s.fogFromSky && s.sky.enabled ? (s.weather.enabled ? inWeather(s.sky, s.weather, 0).horizon : s.sky.horizon)
                                                     : s.fogColor;
    if (additive) return { c.r, c.g, c.b, (unsigned char)(c.a * (1 - f)) };   // light fades out in fog
    auto lin = [](unsigned char v) { return std::pow(v / 255.f, 2.2f); };
    auto mix = [&](unsigned char a, unsigned char b) {
        return (unsigned char)std::lround(255 * std::pow(lin(a) + (lin(b) - lin(a)) * f, 1 / 2.2f));
    };
    return { mix(c.r, tint.r), mix(c.g, tint.g), mix(c.b, tint.b), c.a };
}

void particleQuads(const Scene& s, const Camera& cam, const Emitting& em, const Matrix& view, double t,
                   std::vector<Quad>& out) {
    const fx::Emitter3D& own = *em.obj->emitter;
    const bool inWind = s.weather.enabled && !own.ownWind && own.windFollow > 0;     // the weather's wind, on a copy
    const fx::Emitter3D windy = inWind ? inWeather(own, s.weather, em.world) : fx::Emitter3D{};
    const fx::Emitter3D& e = inWind ? windy : own;
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
        col = fogged(s, col, cam.position, c, e.additive);
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

// ---- 3D lines: each stroke a camera-facing ribbon whose half-width is worked out at each end
// from that end's depth (so it is the same number of px all along), round joins and caps as
// discs, arrowheads as their own quads. Clipped to the near plane before anything else.
void lineQuads(const Scene& s, const Camera& cam, const Emitting& em, const Matrix& view, float compH,
               std::vector<Quad>& out) {
    const Lines& L = *em.obj->lines;
    const bool ortho = cam.orthographic;
    const float tanHalf = std::tan(cam.fov * 0.5f * DEG2RAD);
    const Vector3 forward = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    const Vector3 right{ view.m0, view.m4, view.m8 }, up{ view.m1, view.m5, view.m9 };
    auto depthOf = [&](Vector3 p) { return -Vector3Transform(p, view).z; };
    auto perPx = [&](float d) { return ortho ? cam.orthoHeight / compH : 2 * d * tanHalf / compH; };   // metres a px
    const float nearD = ortho ? -1e30f : cam.nearPlane * 1.01f;
    auto toward = [&](Vector3 p) {                           // unit vector from p to the camera
        return ortho ? Vector3Negate(forward) : Vector3Normalize(Vector3Subtract(cam.position, p));
    };
    auto screen = [&](Vector3 p) {                           // composition px from the view's centre
        const Vector3 v = Vector3Transform(p, view);
        const float k = ortho ? compH / cam.orthoHeight : compH * 0.5f / (tanHalf * -v.z);
        return Vector2{ v.x * k, v.y * k };
    };
    for (const Lines::Path& path : L.paths) {
        if (path.points.size() < 2 || path.color.a == 0 || path.width <= 0) continue;
        const float w = path.width;
        auto half = [&](float d) { return L.worldWidth ? w * 0.5f + 0.5f * perPx(d) : (w * 0.5f + 0.5f) * perPx(d); };
        // world points, lifted, closed
        std::vector<Vector3> pts;
        for (const Vector3& p : path.points) {
            Vector3 q = Vector3Transform(p, em.world);
            if (L.lift != 0) q = Vector3Add(q, Vector3Scale(toward(q), L.lift));
            pts.push_back(q);
        }
        if (path.closed) pts.push_back(pts.front());
        // trim to [from, to] of the length
        std::vector<float> acc{ 0 };
        for (size_t i = 1; i < pts.size(); i++) acc.push_back(acc.back() + Vector3Distance(pts[i - 1], pts[i]));
        const float total = acc.back();
        if (total <= 1e-9f) continue;
        const float f0 = std::clamp(std::min(path.from, path.to), 0.f, 1.f) * total;
        const float f1 = std::clamp(std::max(path.from, path.to), 0.f, 1.f) * total;
        if (f1 - f0 <= 1e-9f) continue;
        auto at = [&](float d) {
            size_t i = 1;
            while (i + 1 < acc.size() && acc[i] < d) i++;
            const float seg = acc[i] - acc[i - 1];
            return Vector3Lerp(pts[i - 1], pts[i], seg > 0 ? (d - acc[i - 1]) / seg : 0.f);
        };
        // arrowheads: the stroke stops at the head's base; a head never outgrows the drawn part
        const bool headEnd = path.head == Lines::Head::End || path.head == Lines::Head::Both;
        const bool headStart = path.head == Lines::Head::Start || path.head == Lines::Head::Both;
        const int heads = (int)headEnd + (int)headStart;
        auto headLen = [&](Vector3 tip) {
            const float len = L.worldWidth ? path.headSize * w : path.headSize * w * perPx(std::max(depthOf(tip), 1e-4f));
            return std::min(len, heads ? (f1 - f0) / heads : len);
        };
        float s0 = f0, s1 = f1;
        struct HeadAt { Vector3 tip, base; };
        std::vector<HeadAt> headList;
        if (headEnd) { const Vector3 tip = at(f1); s1 = f1 - headLen(tip); headList.push_back({ tip, at(s1) }); }
        if (headStart) { const Vector3 tip = at(f0); s0 = f0 + headLen(tip); headList.push_back({ tip, at(s0) }); }
        // the drawn polyline
        std::vector<Vector3> line{ at(s0) };
        for (size_t i = 1; i + 1 < acc.size(); i++) if (acc[i] > s0 && acc[i] < s1) line.push_back(pts[i]);
        line.push_back(at(s1));

        const Color col = path.color;
        auto colourAt = [&](Vector3 p) { return fogged(s, col, cam.position, p, false); };
        auto clip = [&](Vector3& a, Vector3& b) {             // to the near plane; false if all behind it
            const float da = depthOf(a), db = depthOf(b);
            if (da < nearD && db < nearD) return false;
            if (da < nearD) a = Vector3Lerp(a, b, (nearD - da) / (db - da));
            else if (db < nearD) b = Vector3Lerp(b, a, (nearD - db) / (da - db));
            return true;
        };
        auto disc = [&](Vector3 p) {
            const float d = depthOf(p);
            if (d < nearD) return;
            const float h = half(d);
            const Vector3 r = Vector3Scale(right, h), u = Vector3Scale(up, h);
            Quad q{};
            q.color = colourAt(p);
            q.shape = DOT;
            q.depth = d;
            q.c[0] = Vector3Subtract(Vector3Subtract(p, r), u);
            q.c[1] = Vector3Subtract(Vector3Add(p, r), u);
            q.c[2] = Vector3Add(Vector3Add(p, r), u);
            q.c[3] = Vector3Add(Vector3Subtract(p, r), u);
            out.push_back(q);
        };
        auto ribbon = [&](Vector3 a, Vector3 b, int shape, float ha, float hb) {
            const Vector3 dir = Vector3Subtract(b, a);
            if (Vector3Length(dir) < 1e-9f) return;
            const Vector3 sideA = Vector3CrossProduct(dir, ortho ? forward : Vector3Subtract(a, cam.position));
            const Vector3 sideB = Vector3CrossProduct(dir, ortho ? forward : Vector3Subtract(b, cam.position));
            if (Vector3Length(sideA) < 1e-12f || Vector3Length(sideB) < 1e-12f) return;   // end-on: the discs cover it
            const Vector3 sa = Vector3Scale(Vector3Normalize(sideA), ha), sb = Vector3Scale(Vector3Normalize(sideB), hb);
            Quad q{};
            q.color = colourAt(Vector3Lerp(a, b, 0.5f));
            q.shape = shape;
            q.depth = 0.5f * (depthOf(a) + depthOf(b));
            q.c[0] = Vector3Subtract(a, sa);
            q.c[1] = Vector3Subtract(b, sb);
            q.c[2] = Vector3Add(b, sb);
            q.c[3] = Vector3Add(a, sa);
            out.push_back(q);
        };
        auto stroke = [&](Vector3 a, Vector3 b) { ribbon(a, b, STROKE, half(depthOf(a)), half(depthOf(b))); };

        if (path.dash > 0) {
            // A dash pattern measured on screen, carried across joints; each dash has round caps.
            const float dash = path.dash, gap = path.gap > 0 ? path.gap : path.dash, period = dash + gap;
            float run = std::fmod(path.dashOffset, period);
            if (run < 0) run += period;
            for (size_t i = 1; i < line.size(); i++) {
                Vector3 a = line[i - 1], b = line[i];
                if (!clip(a, b)) continue;
                const float len = Vector2Distance(screen(a), screen(b));
                const float za = depthOf(a), zb = depthOf(b);
                auto worldAt = [&](float sf) {                // screen fraction -> point (perspective-correct)
                    const float tf = ortho ? sf : sf * za / ((1 - sf) * zb + sf * za);
                    return Vector3Lerp(a, b, tf);
                };
                float pos = 0;
                while (pos < len) {
                    const float phase = std::fmod(run + pos, period);
                    if (phase < dash) {
                        const float end = std::min(len, pos + (dash - phase));
                        const Vector3 p = worldAt(len > 0 ? pos / len : 0), q = worldAt(len > 0 ? end / len : 1);
                        stroke(p, q);
                        disc(p);
                        disc(q);
                        pos = end;
                    } else pos += period - phase;
                }
                run = std::fmod(run + len, period);
            }
        } else {
            for (size_t i = 1; i < line.size(); i++) {
                Vector3 a = line[i - 1], b = line[i];
                if (clip(a, b)) stroke(a, b);
            }
            for (size_t i = 0; i < line.size(); i++) {         // joins, and caps where no head sits
                if ((i == 0 && headStart) || (i + 1 == line.size() && headEnd)) continue;
                disc(line[i]);
            }
        }
        for (const HeadAt& h : headList) {
            Vector3 base = h.base, tip = h.tip;
            if (!clip(base, tip)) continue;
            const float len = L.worldWidth ? path.headSize * w : path.headSize * w * perPx(std::max(depthOf(tip), 1e-4f));
            const float k = Vector3Distance(base, tip) / std::max(len, 1e-9f);     // a shortened head stays in proportion
            auto hw = [&](Vector3 p) {                        // the head's half-width, at p's depth
                const float d = depthOf(p);
                return L.worldWidth ? 0.45f * path.headSize * w * k + 0.5f * perPx(d)
                                    : (0.45f * path.headSize * w * k + 0.5f) * perPx(d);
            };
            ribbon(base, tip, HEAD, hw(base), hw(tip));
        }
    }
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
// ---- splats --------------------------------------------------------------------------
// A cloud's texture: 6 RGBA32F texels a splat, made once. 0: position, opacity. 1: covariance
// xx xy xz yy. 2: covariance yz zz, a per-splat random number, SH blue 3. 3: colour, SH red 1.
// 4: SH red 2, red 3, green 1, green 2. 5: SH green 3, blue 1, blue 2.
Gpu::SplatGpu& splatGpu(const std::shared_ptr<const splat::Cloud>& cloud) {
    auto found = gpu.splats.find(cloud.get());
    if (found != gpu.splats.end()) return found->second;
    const size_t n = cloud->size();
    if (n == 0 || cloud->position.size() < n * 3 || cloud->scale.size() < n * 3 || cloud->rotation.size() < n * 4 ||
        cloud->colour.size() < n * 3)
        throw std::runtime_error("g3d: a splat cloud is empty or its arrays disagree");
    if (n > 5000000) throw std::runtime_error("g3d: a splat cloud is limited to 5 million splats");
    constexpr int WIDTH = 2048;
    const size_t texels = n * 6;
    const int rows = (int)((texels + WIDTH - 1) / WIDTH);
    std::vector<float> data((size_t)WIDTH * (size_t)rows * 4, 0.f);
    const bool sh = cloud->hasSH() && cloud->sh.size() >= n * 9;
    for (size_t i = 0; i < n; i++) {
        float cov[6];
        cloud->covariance(i, cov);
        const unsigned h = (unsigned)i * 2654435761u;
        const float rnd = (float)((h >> 8) & 0xFFFF) / 65536.f;
        const float* p = &cloud->position[i * 3];
        const float* c = &cloud->colour[i * 3];
        const float* k = sh ? &cloud->sh[i * 9] : nullptr;
        auto s = [&](int j) { return k ? k[j] : 0.f; };
        float* t = &data[i * 24];
        const float texel[24] = { p[0], p[1], p[2], cloud->opacity[i],
                                  cov[0], cov[1], cov[2], cov[3],
                                  cov[4], cov[5], rnd, s(8),
                                  c[0], c[1], c[2], s(0),
                                  s(1), s(2), s(3), s(4),
                                  s(5), s(6), s(7), 0.f };
        std::copy(texel, texel + 24, t);
    }
    Gpu::SplatGpu g;
    g.keep = cloud;
    g.tex = rlLoadTexture(data.data(), WIDTH, rows, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
    rlTextureParameters(g.tex, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
    rlTextureParameters(g.tex, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
    // One quad (two triangles) per instance, and the instance order, sized for the whole cloud.
    static const float quad[12] = { -1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1 };
    g.vao = rlLoadVertexArray();
    rlEnableVertexArray(g.vao);
    g.cornerVbo = rlLoadVertexBuffer(quad, sizeof(quad), false);
    rlEnableVertexBuffer(g.cornerVbo);
    rlSetVertexAttribute((unsigned)gpu.P.corner, 2, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute((unsigned)gpu.P.corner);
    g.indexVbo = rlLoadVertexBuffer(nullptr, (int)(n * sizeof(float)), true);
    rlEnableVertexBuffer(g.indexVbo);
    rlSetVertexAttribute((unsigned)gpu.P.index, 1, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute((unsigned)gpu.P.index);
    rlSetVertexAttributeDivisor((unsigned)gpu.P.index, 1);
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    g.capacity = n;
    return gpu.splats.emplace(cloud.get(), std::move(g)).first->second;
}

// Splat indices in drawing order, far to near, those that can show: in front of the camera and
// inside the frustum (with each splat's reach). `mv` is object -> view space (raylib's order).
void sortSplats(const splat::Cloud& c, const Matrix& mv, float tx, float ty, float reach, std::vector<unsigned>& order) {
    const size_t n = c.size();
    std::vector<unsigned> keys;
    std::vector<unsigned> ids;
    keys.reserve(n);
    ids.reserve(n);
    const float sx = std::sqrt(mv.m0 * mv.m0 + mv.m1 * mv.m1 + mv.m2 * mv.m2);          // the object's scale
    for (size_t i = 0; i < n; i++) {
        const float x = c.position[i * 3], y = c.position[i * 3 + 1], z = c.position[i * 3 + 2];
        const float vx = mv.m0 * x + mv.m4 * y + mv.m8 * z + mv.m12;
        const float vy = mv.m1 * x + mv.m5 * y + mv.m9 * z + mv.m13;
        const float vz = mv.m2 * x + mv.m6 * y + mv.m10 * z + mv.m14;
        const float depth = -vz;
        if (depth < 0.05f) continue;
        const float r = 3 * reach * sx * std::max({ c.scale[i * 3], c.scale[i * 3 + 1], c.scale[i * 3 + 2] });
        if (std::fabs(vx) > depth * tx + r || std::fabs(vy) > depth * ty + r) continue;
        unsigned bits;
        std::memcpy(&bits, &depth, 4);
        keys.push_back(~bits);                           // positive floats order as integers; far first
        ids.push_back((unsigned)i);
    }
    // LSD radix sort, three passes of 11 bits.
    std::vector<unsigned> keys2(keys.size()), ids2(ids.size());
    for (int pass = 0; pass < 3; pass++) {
        const int shift = pass * 11;
        std::vector<unsigned> count(2049, 0);
        for (unsigned k : keys) count[((k >> shift) & 2047) + 1]++;
        for (size_t b = 0; b < 2048; b++) count[b + 1] += count[b];
        for (size_t j = 0; j < keys.size(); j++) {
            const unsigned slot = count[(keys[j] >> shift) & 2047]++;
            keys2[slot] = keys[j];
            ids2[slot] = ids[j];
        }
        keys.swap(keys2);
        ids.swap(ids2);
    }
    order.swap(ids);
}

void drawSplats(const Scene& s, const Camera& cam, const std::vector<Emitting>& objs, const Matrix& view, const Matrix& proj,
                int vw, int vh) {
    struct Job { const Emitting* em; float depth; };
    std::vector<Job> jobs;
    const float aspect = (float)vw / (float)vh;
    for (const Emitting& em : objs) {
        if (!em.obj->splats || !em.obj->splats->cloud) continue;
        const splat::Cloud& c = *em.obj->splats->cloud;
        if (c.size() == 0) continue;
        const Vector3 centre = Vector3Transform(c.centre, em.world);
        const float scale = std::max({ Vector3Length({ em.world.m0, em.world.m1, em.world.m2 }), Vector3Length({ em.world.m4, em.world.m5, em.world.m6 }),
                                       Vector3Length({ em.world.m8, em.world.m9, em.world.m10 }) });
        const float reach = std::max(em.obj->splats->look.scale, 1.f) * (em.obj->splats->look.scatter > 0 ? 1.f : 1.f);
        if (!visibleSphere(cam, aspect, centre, (c.radius + em.obj->splats->look.scatter) * scale * reach)) continue;
        jobs.push_back({ &em, -Vector3Transform(centre, view).z });
    }
    if (jobs.empty()) return;
    std::stable_sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) { return a.depth > b.depth; });
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    for (const Job& j : jobs) {
        const Splats& sp = *j.em->obj->splats;
        const splat::Cloud& c = *sp.cloud;
        Gpu::SplatGpu& g = splatGpu(sp.cloud);
        const Matrix mv = MatrixMultiply(j.em->world, view);
        float key[16];
        const float* mvf = &mv.m0;
        std::copy(mvf, mvf + 16, key);
        const float tx = 1.f / proj.m0, ty = 1.f / proj.m5;
        if (!g.sorted || std::memcmp(key, g.lastView, sizeof(key)) != 0) {
            std::vector<unsigned> order;
            sortSplats(c, mv, tx, ty, std::max(sp.look.scale, 1.f), order);
            std::vector<float> inst(order.size());
            for (size_t i = 0; i < order.size(); i++) inst[i] = (float)order[i];
            g.drawn = inst.size();
            if (!inst.empty()) rlUpdateVertexBuffer(g.indexVbo, inst.data(), (int)(inst.size() * sizeof(float)), 0);
            std::copy(key, key + 16, g.lastView);
            g.sorted = true;
        }
        if (g.drawn == 0) continue;
        const Matrix invWorld = MatrixInvert(j.em->world);
        const Vector3 camObj = Vector3Transform(cam.position, invWorld);
        const Vector2 viewport{ (float)vw, (float)vh };
        const Vector3 cn = Vector3Length(sp.look.clipNormal) > 1e-6f ? Vector3Normalize(sp.look.clipNormal) : Vector3{ 1, 0, 0 };
        const Vector4 plane{ cn.x, cn.y, cn.z, sp.look.clipOffset };
        const int sh = c.hasSH() ? std::clamp(sp.look.shDegree, 0, 1) : 0;
        rlEnableShader(gpu.splat.id);
        SetShaderValueMatrix(gpu.splat, gpu.P.view, view);
        SetShaderValueMatrix(gpu.splat, gpu.P.world, j.em->world);
        SetShaderValueMatrix(gpu.splat, gpu.P.proj, proj);
        SetShaderValue(gpu.splat, gpu.P.viewport, &viewport, SHADER_UNIFORM_VEC2);
        SetShaderValue(gpu.splat, gpu.P.camObj, &camObj, SHADER_UNIFORM_VEC3);
        SetShaderValue(gpu.splat, gpu.P.scaleMul, &sp.look.scale, SHADER_UNIFORM_FLOAT);
        SetShaderValue(gpu.splat, gpu.P.opacityMul, &sp.look.opacity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(gpu.splat, gpu.P.shDegree, &sh, SHADER_UNIFORM_INT);
        SetShaderValue(gpu.splat, gpu.P.clipPlane, &plane, SHADER_UNIFORM_VEC4);
        SetShaderValue(gpu.splat, gpu.P.clipBand, &sp.look.clipBand, SHADER_UNIFORM_FLOAT);
        SetShaderValue(gpu.splat, gpu.P.scatter, &sp.look.scatter, SHADER_UNIFORM_FLOAT);
        bindUnit(SPLAT_SLOT, g.tex);
        rlEnableShader(gpu.splat.id);
        rlEnableVertexArray(g.vao);
        rlDrawVertexArrayInstanced(0, 6, (int)g.drawn);
        rlDisableVertexArray();
        bindUnit(SPLAT_SLOT, 0);
    }
    rlDisableShader();
    restoreBlend();
    rlEnableDepthMask();
}

void drawParticles(const Scene& s, const Camera& cam, const std::vector<Emitting>& emitters, const Matrix& view, double t,
                   float compH) {
    std::vector<Quad> blended, added, top;
    for (const Emitting& em : emitters) {
        if (em.obj->emitter) particleQuads(s, cam, em, view, t, em.obj->emitter->additive ? added : blended);
        else if (em.obj->lines) lineQuads(s, cam, em, view, compH, em.obj->lines->onTop ? top : blended);
    }
    if (blended.empty() && added.empty() && top.empty()) return;
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
    if (!top.empty()) {                                       // lines over everything, in their own order
        std::stable_sort(top.begin(), top.end(), [](const Quad& a, const Quad& b) { return a.depth > b.depth; });
        rlDrawRenderBatchActive();
        rlDisableDepthTest();
        submit(top);
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
    }
    EndShaderMode();
    rlSetTexture(0);
    rlEnableDepthMask();
}

// ---- volumetric clouds (Scene::clouds) ------------------------------------------------------
// One frame's resolved cloud settings: the layer with the weather worked in, where the pattern has
// drifted to, and the light it is seen by.
struct CloudFrame {
    Clouds c;
    Vector2 travel{ 0, 0 };
    Vector2 shear{ 0.8f, 0.6f };                    // the way towers lean: downwind
    double seconds = 0;
    Vector3 sun{ 0, 1, 0 }, sunE{ 1, 1, 1 }, ambTop{ 0.3f, 0.4f, 0.6f }, ambBot{ 0.1f, 0.1f, 0.1f };
    Vector3 horizon{ 0.5f, 0.6f, 0.8f }, haze{ 1, 0.6f, 0.4f };
    float hazeAmount = 0;
    int steps = 56, lightSteps = 6, shadowSteps = 20, shadowRes = 256;
    float res = 0.5f;
};

constexpr float CLOUD_SHAPE_SIZE = 6000, CLOUD_DETAIL_SIZE = 900, CLOUD_WEATHER_SIZE = 24000;

float cloudHash(unsigned a, unsigned b, unsigned seed) {
    unsigned h = a * 374761393u + b * 668265263u + seed * 2246822519u + 0x9e3779b9u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    h *= 2246822519u;
    h ^= h >> 15;
    return (float)(h & 0xffffffu) / 16777215.f;
}
// Periodic value noise with f cells over 0..1.
float cloudValueNoise(float x, float y, int f, unsigned seed) {
    const float fx = x * f, fy = y * f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    const float tx = fx - ix, ty = fy - iy;
    auto q = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    auto h = [&](int a, int b) { return cloudHash((unsigned)(((a % f) + f) % f), (unsigned)(((b % f) + f) % f), seed); };
    const float u = q(tx), v = q(ty);
    return Lerp(Lerp(h(ix, iy), h(ix + 1, iy), u), Lerp(h(ix, iy + 1), h(ix + 1, iy + 1), u), v);
}
// The coverage map: two fields (R: where clouds are, G: how tall they grow), each pushed through its
// own rank so its values are uniform in 0..1. A cover of c then puts cloud on exactly the share c of the
// map, which is what lets the sky fraction match `cover`.
Image cloudWeatherImage() {
    constexpr int N = 256;
    Image img = GenImageColor(N, N, BLACK);
    Color* px = (Color*)img.data;
    for (int ch = 0; ch < 2; ch++) {
        std::vector<float> v((size_t)N * N);
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++) {
                const float u = (x + 0.5f) / N, w = (y + 0.5f) / N;
                float sum = 0;
                const int fs[4] = { 6, 12, 24, 48 };
                const float ws[4] = { 0.5f, 0.28f, 0.16f, 0.06f };
                for (int o = 0; o < 4; o++) sum += ws[o] * cloudValueNoise(u, w, fs[o], 101u * (unsigned)ch + 17u * (unsigned)o + 3u);
                v[(size_t)y * N + x] = sum;
            }
        std::vector<int> order(v.size());
        for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
        std::sort(order.begin(), order.end(), [&](int a, int b) { return v[(size_t)a] < v[(size_t)b] || (v[(size_t)a] == v[(size_t)b] && a < b); });
        for (size_t r = 0; r < order.size(); r++) {
            const unsigned char b = (unsigned char)std::lround(255.0 * r / (double)(order.size() - 1));
            Color& c = px[order[r]];
            (ch == 0 ? c.r : c.g) = b;
        }
    }
    for (int i = 0; i < N * N; i++) px[i].a = 255;
    return img;
}

// Draws `sh` over a rectangle (vx, vy, vw, vh in the framebuffer's pixels) of `fbo`, one fragment
// per pixel (gl_FragCoord), blending off. The caller restores its framebuffer and viewport.
void cloudPass(const Shader& sh, unsigned fbo, int vx, int vy, int vw, int vh) {
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(fbo);
    rlViewport(vx, vy, vw, vh);
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    rlOrtho(0, vw, vh, 0, -1, 1);
    rlMatrixMode(RL_MODELVIEW);
    rlPushMatrix();
    rlLoadIdentity();
    rlDisableColorBlend();
    BeginShaderMode(sh);
    DrawRectangle(0, 0, vw, vh, WHITE);
    EndShaderMode();
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    rlMatrixMode(RL_MODELVIEW);
    rlPopMatrix();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
}

void shF(const Shader& sh, const char* n, float v) { SetShaderValue(sh, GetShaderLocation(sh, n), &v, SHADER_UNIFORM_FLOAT); }
void shI(const Shader& sh, const char* n, int v) { SetShaderValue(sh, GetShaderLocation(sh, n), &v, SHADER_UNIFORM_INT); }
void shV2(const Shader& sh, const char* n, Vector2 v) { SetShaderValue(sh, GetShaderLocation(sh, n), &v, SHADER_UNIFORM_VEC2); }
void shV3(const Shader& sh, const char* n, Vector3 v) { SetShaderValue(sh, GetShaderLocation(sh, n), &v, SHADER_UNIFORM_VEC3); }

bool colorTarget(unsigned& fbo, unsigned& tex, int& cw, int& ch, int w, int h) {
    if (fbo && cw == w && ch == h) return true;
    if (fbo) rlUnloadFramebuffer(fbo);
    fbo = rlLoadFramebuffer();
    tex = rlLoadTexture(nullptr, w, h, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    rlFramebufferAttach(fbo, tex, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    const bool ok = rlFramebufferComplete(fbo);
    rlDisableFramebuffer();
    if (!ok) { rlUnloadFramebuffer(fbo); fbo = tex = 0; cw = ch = 0; throw std::runtime_error("g3d: cloud render target incomplete"); }
    cw = w; ch = h;
    return true;
}

// Compiles the cloud shaders and bakes the noise, once. Leaves `callerFbo` and its viewport bound.
// A 64 x 64 tileable blue-noise image by void and cluster (Ulichney): a binary pattern is relaxed until its
// ones are as evenly spread as they can be, then every pixel gets its rank, in the order the ones would
// be removed (the first ten percent), added (to half) and the zeros removed (the rest). Deterministic (the
// start is a hash), toroidal, a few milliseconds, built once.
Image cloudBlueNoiseImage() {
    constexpr int N = 64, M = N * N, R = 6;
    constexpr float sigma = 1.5f;
    std::vector<float> kern((size_t)(2 * R + 1) * (2 * R + 1));
    for (int dy = -R; dy <= R; dy++)
        for (int dx = -R; dx <= R; dx++) kern[(size_t)(dy + R) * (2 * R + 1) + dx + R] = std::exp(-(float)(dx * dx + dy * dy) / (2 * sigma * sigma));
    std::vector<float> e1((size_t)M, 0.f), e0((size_t)M, 0.f);        // the energy of the ones' pattern and of the zeros'
    std::vector<char> bit((size_t)M, 0);
    auto splat = [&](std::vector<float>& e, int i, float sgn) {
        const int x = i % N, y = i / N;
        for (int dy = -R; dy <= R; dy++)
            for (int dx = -R; dx <= R; dx++)
                e[(size_t)(((y + dy + N) % N) * N + (x + dx + N) % N)] += sgn * kern[(size_t)(dy + R) * (2 * R + 1) + dx + R];
    };
    auto set = [&](int i, bool on) {
        bit[(size_t)i] = on ? 1 : 0;
        splat(e1, i, on ? 1.f : -1.f);
        splat(e0, i, on ? -1.f : 1.f);
    };
    auto pick = [&](bool among, const std::vector<float>& e, bool wantMax) {       // the pixel with bit == among, of most or least energy
        int best = -1;
        for (int i = 0; i < M; i++)
            if ((bit[(size_t)i] != 0) == among && (best < 0 || (wantMax ? e[(size_t)i] > e[(size_t)best] : e[(size_t)i] < e[(size_t)best]))) best = i;
        return best;
    };
    std::vector<int> order((size_t)M);
    for (int i = 0; i < M; i++) order[(size_t)i] = i;
    std::sort(order.begin(), order.end(), [](int a, int b) { return cloudHash((unsigned)a, 3, 77) < cloudHash((unsigned)b, 3, 77); });
    const int K = M / 10;
    for (int i = 0; i < M; i++) splat(e0, i, 1.f);
    for (int i = 0; i < K; i++) set(order[(size_t)i], true);
    for (int it = 0; it < 4000; it++) {                                           // relax: move the tightest one into the largest void
        const int c = pick(true, e1, true);
        set(c, false);
        const int v = pick(false, e1, false);
        set(v, true);
        if (v == c) break;
    }
    const std::vector<char> bit0 = bit;
    const std::vector<float> e10 = e1, e00 = e0;
    std::vector<int> rank((size_t)M, 0);
    for (int r = K - 1; r >= 0; r--) { const int c = pick(true, e1, true); rank[(size_t)c] = r; set(c, false); }
    bit = bit0; e1 = e10; e0 = e00;
    for (int r = K; r < M / 2; r++) { const int v = pick(false, e1, false); rank[(size_t)v] = r; set(v, true); }
    for (int r = M / 2; r < M; r++) { const int c = pick(false, e0, true); rank[(size_t)c] = r; set(c, true); }
    Image img = GenImageColor(N, N, BLACK);
    for (int i = 0; i < M; i++) {
        const unsigned char v = (unsigned char)std::lround(255.0 * rank[(size_t)i] / (M - 1));
        ImageDrawPixel(&img, i % N, i / N, { v, v, v, 255 });
    }
    return img;
}

void ensureCloudGpu(unsigned callerFbo, int fbW, int fbH) {
    CloudGpu& c = gpu.cl;
    if (c.ready) return;
    auto build = [](const char* fs) {
        std::string src = fs;
        const size_t at = src.find("\nCOMMON\n");
        if (at != std::string::npos) src.replace(at + 1, 6, CLOUD_COMMON);
        return LoadShaderFromMemory(nullptr, src.c_str());
    };
    c.bake = LoadShaderFromMemory(nullptr, CLOUD_BAKE_FS);
    c.march = build(CLOUD_MARCH_FS);
    c.shadow = build(CLOUD_SHADOW_FS);
    c.up = LoadShaderFromMemory(nullptr, CLOUD_UP_FS);
    if (!IsShaderValid(c.bake) || !IsShaderValid(c.march) || !IsShaderValid(c.shadow) || !IsShaderValid(c.up))
        throw std::runtime_error("g3d: cloud shader compilation failed");
    auto bake = [&](RenderTexture& rt, int w, int h, int mode) {
        rt = LoadRenderTexture(w, h);
        SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(rt.texture, TEXTURE_WRAP_CLAMP);
        shI(c.bake, "mode", mode);
        cloudPass(c.bake, rt.id, 0, 0, w, h);
    };
    bake(c.shape, 2080, 1040, 0);                     // 128 slices of 128 x 128, padded to 130, 16 x 8 tiles
    bake(c.detail, 272, 136, 1);                      // 32 slices of 32 x 32, padded to 34, 8 x 4 tiles
    Image w = cloudWeatherImage();
    c.weather = LoadTextureFromImage(w);
    UnloadImage(w);
    GenTextureMipmaps(&c.weather);                    // read by explicit LOD, so far clouds see a blurred coverage
    SetTextureFilter(c.weather, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(c.weather, TEXTURE_WRAP_REPEAT);
    Image bn = cloudBlueNoiseImage();
    c.blue = LoadTextureFromImage(bn);
    UnloadImage(bn);
    SetTextureFilter(c.blue, TEXTURE_FILTER_POINT);
    SetTextureWrap(c.blue, TEXTURE_WRAP_REPEAT);
    rlEnableFramebuffer(callerFbo);
    rlViewport(0, 0, fbW, fbH);
    c.ready = true;
}

bool cloudsActive(const Scene& s, const Camera& cam) {
    if (!s.clouds.enabled || cam.orthographic) return false;
    if (s.water.enabled && cam.position.y < waterOf(s).level) return false;       // from under the sea: no clouds
    return true;
}

CloudFrame cloudFrame(const Scene& s, double frame) {
    CloudFrame f;
    f.c = s.weather.enabled ? inWeather(s.clouds, s.weather) : s.clouds;
    const double fps = s.weather.enabled && !s.clouds.ownWeather ? s.weather.fps : s.clouds.fps;
    f.seconds = frame / std::max(1.0, fps);
    f.travel = s.clouds.travel(s.weather, f.seconds);
    { const Vector2 w = (s.weather.enabled && !s.clouds.ownWeather) ? Vector2{ std::cos(s.weather.wind.direction * DEG2RAD), std::sin(s.weather.wind.direction * DEG2RAD) } : Vector2{ s.clouds.drift.x, s.clouds.drift.y };
      const float l = std::sqrt(w.x * w.x + w.y * w.y); if (l > 1e-4f) f.shear = { w.x / l, w.y / l }; }
    struct Q { int steps, light, shadowSteps, shadowRes; float res; };
    static const Q table[3] = { { 28, 4, 12, 128, 0.5f }, { 96, 6, 20, 256, 1.f }, { 160, 10, 32, 384, 1.f } };
    const Q& q = table[std::clamp((int)f.c.quality, 0, 2)];
    f.steps = q.steps; f.lightSteps = q.light; f.shadowSteps = q.shadowSteps; f.shadowRes = q.shadowRes;
    f.res = std::clamp(f.c.resolution > 0 ? f.c.resolution : q.res, 0.1f, 1.f);
    // The sun: the sky's, else the first directional light's.
    const Sky k = s.sky.enabled ? (s.weather.enabled ? inWeather(s.sky, s.weather, frame) : s.sky) : Sky{};
    f.sun = sunOf(s);
    Vector3 sunCol{ 1, 0.96f, 0.88f };
    if (s.sky.enabled) sunCol = linear3(k.sunColor);
    else
        for (const Light& l : s.lights)
            if (l.type == Light::Type::Directional) {
                const Vector3 d = Vector3Subtract(l.position, l.target);
                if (Vector3Length(d) > 1e-6f) { f.sun = Vector3Normalize(d); sunCol = linear3(l.color); }
                break;
            }
    const float vis = std::clamp((f.sun.y + 0.02f) / 0.08f, 0.f, 1.f);
    f.sunE = Vector3Scale(sunCol, 0.62f * vis * vis * (3 - 2 * vis));
    Vector3 zen, hor, gr;
    if (s.sky.enabled) { zen = linear3(k.zenith); hor = linear3(k.horizon); gr = linear3(k.ground); f.haze = linear3(k.haze); f.hazeAmount = k.hazeAmount; }
    else { zen = linear3(s.skyColor); hor = Vector3Lerp(zen, { 1, 1, 1 }, 0.4f); gr = linear3(s.groundColor); f.haze = hor; f.hazeAmount = 0; }
    f.horizon = hor;
    f.ambTop = Vector3Add(Vector3Scale(Vector3Add(Vector3Scale(zen, 0.5f), Vector3Scale(hor, 0.5f)), 0.85f), { 0.08f, 0.08f, 0.08f });
    f.ambBot = Vector3Scale(Vector3Add(Vector3Scale(gr, 0.3f), Vector3Scale(hor, 0.7f)), 0.7f);
    return f;
}

// The shared uniforms of the march and the shadow shaders, and the noise textures on their units.
void setCloudCommon(const Shader& sh, const CloudFrame& f, Vector2 focus) {
    const Clouds& c = f.c;
    shF(sh, "planetR", Clouds::kPlanetRadius);
    shF(sh, "cloudBase", c.base);
    shF(sh, "cloudTop", std::max(c.top, c.base + 50));
    shF(sh, "cover", std::clamp(c.cover, 0.f, 1.f));
    shF(sh, "densityScale", std::max(c.density, 0.f));
    shF(sh, "detailAmt", std::clamp(c.detail, 0.f, 1.f));
    shI(sh, "cloudType", (int)c.type);
    shV2(sh, "windOffset", f.travel);
    const Vector3 dir = Vector3Normalize({ 0.5f, 1.0f, 0.3f });
    const float ev = c.evolution * (float)f.seconds;
    const Vector3 off{ cloudHash(c.seed, 11, 5) * CLOUD_SHAPE_SIZE, cloudHash(c.seed, 12, 5) * CLOUD_SHAPE_SIZE, cloudHash(c.seed, 13, 5) * CLOUD_SHAPE_SIZE };
    shV3(sh, "slide", { off.x + dir.x * ev, off.y + dir.y * ev, off.z + dir.z * ev });
    shV2(sh, "seedOffset", { cloudHash(c.seed, 21, 9), cloudHash(c.seed, 22, 9) });
    shF(sh, "shapeSize", CLOUD_SHAPE_SIZE);
    shF(sh, "detailSize", CLOUD_DETAIL_SIZE);
    shF(sh, "weatherSize", CLOUD_WEATHER_SIZE);
    shI(sh, "shapeTex", CLOUD_SHAPE_SLOT);
    shI(sh, "detailTex", CLOUD_DETAIL_SLOT);
    shI(sh, "weatherTex", CLOUD_WEATHER_SLOT);
    bindUnit(CLOUD_SHAPE_SLOT, gpu.cl.shape.texture.id);
    bindUnit(CLOUD_DETAIL_SLOT, gpu.cl.detail.texture.id);
    bindUnit(CLOUD_WEATHER_SLOT, gpu.cl.weather.id);
    shI(sh, "extruded", c.extruded ? 1 : 0);
    shF(sh, "nearestZ", c.nearestSlice ? 1.f : 0.f);
    shV2(sh, "shearDir", f.shear);
    shF(sh, "anvilOn", c.anvil > 0.5f ? 1.f : 0.f);
    shF(sh, "towerVariety", std::clamp(c.towerVariety, 0.f, 1.f));
    {   // the storm cells near the view (the pattern's frame), nearest 12
        const float cx = focus.x - f.travel.x, cz = focus.y - f.travel.y, reach = 150000.f;
        std::vector<Clouds::Storm> st = stormCells(c, cx - reach, cz - reach, cx + reach, cz + reach);
        std::sort(st.begin(), st.end(), [&](const Clouds::Storm& a, const Clouds::Storm& b) { return std::hypot(a.x - cx, a.z - cz) < std::hypot(b.x - cx, b.z - cz); });
        if (st.size() > 12) st.resize(12);
        float d[48] = {}, t[12] = {};
        const float layer = std::max(c.top, c.base + 50) - c.base;
        for (size_t i = 0; i < st.size(); i++) {
            d[i * 4] = st[i].x; d[i * 4 + 1] = st[i].z; d[i * 4 + 2] = st[i].radius;
            d[i * 4 + 3] = (float)(int)(std::fmod(std::fabs(st[i].x * 0.013f + st[i].z * 0.0071f), 4000.f) + 1.f);
            t[i] = st[i].top > 0 ? std::clamp((st[i].top - c.base) / layer, 0.3f, 0.99f) : 0.93f;
        }
        const int n = (int)st.size();
        shI(sh, "stormN", c.anvil > 0.5f ? n : 0);
        SetShaderValueV(sh, GetShaderLocation(sh, "stormD"), d, SHADER_UNIFORM_VEC4, 12);
        SetShaderValueV(sh, GetShaderLocation(sh, "stormT"), t, SHADER_UNIFORM_FLOAT, 12);
    }
}
void unbindCloudUnits() {
    for (int slot : { CLOUD_SHAPE_SLOT, CLOUD_DETAIL_SLOT, CLOUD_WEATHER_SLOT, CLOUD_LOW_SLOT, CLOUD_BLUE_SLOT, DEPTH_SLOT }) bindUnit(slot, 0);
}

// The ground shadow map, before the scene is drawn: the sun's transmittance through the clouds, on the
// plane y = 0 over `shadowExtent` metres round the view. Leaves the caller's framebuffer and viewport.
void renderCloudShadow(const Scene& s, const CloudFrame& f, const Camera& cam, unsigned callerFbo, int fbW, int fbH) {
    CloudGpu& g = gpu.cl;
    g.shadowOn = false;
    if (!f.c.shadows || f.sun.y < 0.03f || f.c.cover <= 0) return;
    int light = -1;
    if (s.sky.enabled && s.sky.sunLight >= 0 && s.sky.sunLight < (int)s.lights.size()) light = s.sky.sunLight;
    else
        for (int i = 0; i < (int)s.lights.size() && i < MAX_LIGHTS; i++) {
            const Light& l = s.lights[(size_t)i];
            const Vector3 d = Vector3Subtract(l.position, l.target);
            if (l.type == Light::Type::Directional && Vector3Length(d) > 1e-6f && Vector3DotProduct(Vector3Normalize(d), f.sun) > 0.97f) { light = i; break; }
        }
    if (light < 0) return;                       // no light along the sun to dim
    ensureCloudGpu(callerFbo, fbW, fbH);
    const int res = f.shadowRes;
    if (g.shadowRT.id && g.shadowRes != res) { UnloadRenderTexture(g.shadowRT); g.shadowRT = RenderTexture{}; }
    if (!g.shadowRT.id) {
        g.shadowRT = LoadRenderTexture(res, res);
        SetTextureFilter(g.shadowRT.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(g.shadowRT.texture, TEXTURE_WRAP_CLAMP);
        g.shadowRes = res;
    }
    const float extent = std::max(f.c.shadowExtent, 100.f);
    Vector3 fwd = Vector3Subtract(cam.target, cam.position);
    fwd.y = 0;
    fwd = Vector3Length(fwd) > 1e-4f ? Vector3Normalize(fwd) : Vector3{ 0, 0, 0 };
    const Vector2 centre{ cam.position.x + fwd.x * extent * 0.2f, cam.position.z + fwd.z * extent * 0.2f };
    const Shader& sh = g.shadow;
    setCloudCommon(sh, f, { cam.position.x, cam.position.z });
    shV2(sh, "shSize", { (float)res, (float)res });
    shV2(sh, "shCentre", centre);
    shF(sh, "shExtent", extent);
    shF(sh, "shY", 0);
    shV3(sh, "sunDir", f.sun);
    shI(sh, "shSteps", f.shadowSteps);
    cloudPass(sh, g.shadowRT.id, 0, 0, res, res);
    unbindCloudUnits();
    rlEnableFramebuffer(callerFbo);
    rlViewport(0, 0, fbW, fbH);
    g.shadowOn = true;
    g.shadowMap = { centre.x, centre.y, 1.f / extent, 0 };
    g.shadowSun = f.sun;
    g.shadowLight = light;
    g.shadowFloor = 0.1f;
}

// ---- lightning and the rainbow (Scene::lightning, Weather::rainbow) -------------------------------
// The strike that is brightest now, and where its light and its glow in the cloud are.
struct FlashFrame {
    bool on = false;
    float power = 0;                         // flash x strike intensity x Lightning::intensity
    LightningActive strike;
    Vector3 lightAt{ 0, 0, 0 };              // the point light's place on the channel
    Vector3 segA{ 0, 0, 0 }, segB{ 0, 0, 0 };    // the part of the channel inside the cloud, for the cloud's glow
};
double lightningSeconds(const Scene& s, double frame) { return frame / std::max(1.f, s.weather.enabled ? s.weather.fps : s.lightning.fps); }

FlashFrame flashFrame(const Scene& s, double frame) {
    FlashFrame f;
    if (!s.lightning.enabled) return f;
    const Lightning& l = s.lightning;
    for (const LightningActive& a : lightningAt(s, lightningSeconds(s, frame))) {
        const float p = a.profile.flash * a.strike.intensity * l.intensity;
        if (p > f.power) { f.power = p; f.strike = a; f.on = true; }
    }
    if (!f.on) return f;
    const Lightning::Strike& st = f.strike.strike;
    if (f.strike.toGround) {
        f.lightAt = Vector3Lerp(st.end, st.start, 0.3f);
        const float up = 700.f;                                                  // the channel's charge region: above where it leaves the cloud base
        f.segA = st.start;
        f.segB = { st.start.x, st.start.y + up, st.start.z };
    } else {
        f.lightAt = Vector3Scale(Vector3Add(st.start, st.end), 0.5f);
        f.segA = st.start; f.segB = st.end;
    }
    return f;
}

// While a frame is drawn: the strongest strike's light, added to the scene's lights (the last is replaced when there are
// 8). The user's lights come back when it ends.
struct FlashScope {
    Scene& s;
    bool added = false, replaced = false;
    Light saved;
    FlashScope(Scene& sc, int t) : s(sc) {
        if (!s.lightning.enabled || s.lightning.light <= 0) return;
        const FlashFrame f = flashFrame(s, frameTime(t));
        if (!f.on || f.power < 0.01f) return;
        Light li = Light::point(f.lightAt, s.lightning.color, 2.5f * s.lightning.light * f.power, s.lightning.lightRange);
        if ((int)s.lights.size() < MAX_LIGHTS) { s.lights.push_back(li); added = true; }
        else { saved = s.lights.back(); s.lights.back() = li; replaced = true; }
    }
    ~FlashScope() {
        if (added) s.lights.pop_back();
        else if (replaced) s.lights.back() = saved;
    }
    FlashScope(const FlashScope&) = delete;
    FlashScope& operator=(const FlashScope&) = delete;
};

// The bolts' ribbons, over the clouds, depth tested against the opaque scene. Into `scene`'s framebuffer, inside the
// viewport (vx, vyGL, vw, vh); leaves the framebuffer bound and the matrices as they were.
void drawBolts(const Scene& s, const Camera& cam, const std::vector<LightningActive>& act, const DepthTarget& scene, int fbW, int fbH,
               int vx, int vyGL, int vw, int vh) {
    if (act.empty() || cam.orthographic) return;
    const Lightning& l = s.lightning;
    if (!gpu.bolt.id) {
        gpu.bolt = LoadShaderFromMemory(BOLT_VS, BOLT_FS);
        if (!IsShaderValid(gpu.bolt)) throw std::runtime_error("g3d: lightning shader compilation failed");
    }
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(scene.fbo);
    rlViewport(vx, vyGL, vw, vh);
    const Matrix savedProj = rlGetMatrixProjection(), savedView = rlGetMatrixModelview();
    const double aspect = (double)vw / vh, top = cam.nearPlane * std::tan(cam.fov * 0.5 * DEG2RAD);
    rlSetMatrixProjection(MatrixFrustum(-top * aspect, top * aspect, -top, top, cam.nearPlane, cam.farPlane));
    rlSetMatrixModelview(MatrixLookAt(cam.position, cam.target, safeUp(Vector3Subtract(cam.target, cam.position), cam.upVector)));
    rlEnableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ZERO, RL_ONE, RL_MAX, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    rlSetTexture(rlGetTextureIdDefault());
    BeginShaderMode(gpu.bolt);
    shV3(gpu.bolt, "glowCol", linear3(l.color));
    shV3(gpu.bolt, "coreCol", { 1.f, 0.97f, 0.92f });
    const float tanY = std::tan(cam.fov * 0.5f * DEG2RAD);
    const float pixelAt1 = 2.f * tanY / 1080.f;                  // metres a 1080-line pixel spans at one metre
    for (const LightningActive& a : act) {
        const float lvl = std::min(a.profile.flash * a.strike.intensity * l.intensity, 1.f);
        if (lvl <= 0.003f) continue;
        const bolt::Bolt b = bolt::make(a.strike.start, a.strike.end, a.strike.seed, l.shape);
        const float reach = b.reach * a.profile.visible;
        const float bright = a.toGround ? lvl : 0.3f * lvl;        // a channel inside the cloud shows as a faint glow
        for (const bolt::Segment& sg : b.segments) {
            if (sg.reachA >= reach) continue;
            Vector3 p0 = sg.a, p1 = sg.b;
            if (sg.reachB > reach) p1 = Vector3Lerp(sg.a, sg.b, (reach - sg.reachA) / std::max(sg.reachB - sg.reachA, 1e-4f));
            const Vector3 dir = Vector3Subtract(p1, p0), mid = Vector3Scale(Vector3Add(p0, p1), 0.5f);
            const float len = Vector3Length(dir);
            if (len < 1e-3f) continue;
            const Vector3 u = Vector3Scale(dir, 1.f / len);
            Vector3 side = Vector3CrossProduct(u, Vector3Subtract(cam.position, mid));
            if (Vector3Length(side) < 1e-5f) continue;
            side = Vector3Normalize(side);
            const float dist = Vector3Distance(cam.position, mid), px = pixelAt1 * dist;
            const float sw = std::max(sg.width, 0.12f);
            const float hc = 0.5f * std::max(l.width * sw, l.minPixels * px * (0.5f + 0.5f * sw));          // the core's half width
            const float hg = std::max(hc * 7.f, 9.f * px * l.glow * (0.4f + 0.6f * sw)) * (0.7f + 0.3f * lvl);   // the glow's
            const float c = a.toGround ? std::clamp(hc / hg, 0.02f, 1.f) : 0.02f;
            const float br = bright * (sg.level == 0 ? 1.f : 0.75f);
            const float gs = std::clamp(l.glow, 0.f, 2.f) * 0.5f * (a.toGround ? 1.f : 1.5f);
            const unsigned char cr = (unsigned char)std::lround(std::clamp(br, 0.f, 1.f) * 255.f), cg = (unsigned char)std::lround(c * 255.f),
                                cb = (unsigned char)std::lround(std::clamp(gs, 0.f, 1.f) * 255.f);
            const float ext = hg;                                                                  // how far the end pieces reach past the piece
            const Vector3 sd = Vector3Scale(side, hg);
            // Four slices along the piece: beyond the start, the start, the end, beyond the end (1 at the outer ones).
            const Vector3 s0 = Vector3Subtract(p0, Vector3Scale(u, ext)), s3 = Vector3Add(p1, Vector3Scale(u, ext));
            const Vector3 cen[4] = { s0, p0, p1, s3 };
            const float beyond[4] = { 1.f, 0.f, 0.f, 1.f };
            rlBegin(RL_QUADS);
            rlColor4ub(cr, cg, cb, 255);
            for (int k = 0; k < 3; k++) {
                const Vector3 qa = cen[k], qb = cen[k + 1];
                rlTexCoord2f(-1.f, beyond[k]);     rlVertex3f(qa.x - sd.x, qa.y - sd.y, qa.z - sd.z);
                rlTexCoord2f(1.f, beyond[k]);      rlVertex3f(qa.x + sd.x, qa.y + sd.y, qa.z + sd.z);
                rlTexCoord2f(1.f, beyond[k + 1]);  rlVertex3f(qb.x + sd.x, qb.y + sd.y, qb.z + sd.z);
                rlTexCoord2f(-1.f, beyond[k + 1]); rlVertex3f(qb.x - sd.x, qb.y - sd.y, qb.z - sd.z);
            }
            rlEnd();
        }
    }
    EndShaderMode();
    rlDrawRenderBatchActive();
    rlSetTexture(0);
    EndBlendMode();
    restoreBlend();
    rlEnableDepthMask();
    rlDisableDepthTest();
    rlSetMatrixProjection(savedProj);
    rlSetMatrixModelview(savedView);
    rlViewport(0, 0, fbW, fbH);
}

// The rainbow over the picture (see RainbowFrame). Two passes of one full-screen shader into `scene`.
void drawRainbow(const Scene& s, const RainbowFrame& rf, const Camera& cam, const DepthTarget& scene, int fbW, int fbH,
                 int vx, int vyGL, int vw, int vh) {
    if (rf.strength <= 0.002f || cam.orthographic) return;
    if (!gpu.rainbow.id) {
        gpu.rainbow = LoadShaderFromMemory(nullptr, RAINBOW_FS);
        if (!IsShaderValid(gpu.rainbow)) throw std::runtime_error("g3d: rainbow shader compilation failed");
    }
    const Shader& sh = gpu.rainbow;
    const Vector3 look = Vector3Subtract(cam.target, cam.position);
    const Vector3 fwd = Vector3Normalize(look);
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, safeUp(look, cam.upVector))), up = Vector3CrossProduct(right, fwd);
    const float tanY = std::tan(cam.fov * 0.5f * DEG2RAD), aspect = (float)vw / (float)vh;
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(scene.fbo);
    rlViewport(vx, vyGL, vw, vh);
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    rlOrtho(0, vw, vh, 0, -1, 1);
    rlMatrixMode(RL_MODELVIEW);
    rlPushMatrix();
    rlLoadIdentity();
    rlDisableDepthTest();
    rlDisableDepthMask();
    shV3(sh, "camPos", cam.position); shV3(sh, "camFwd", fwd); shV3(sh, "camRight", right); shV3(sh, "camUp", up);
    shV2(sh, "tanHalf", { tanY * aspect, tanY });
    shV2(sh, "vpOrigin", { (float)vx, (float)vyGL });
    shV2(sh, "vpSize", { (float)vw, (float)vh });
    shV2(sh, "zNF", { cam.nearPlane, cam.farPlane });
    shV3(sh, "antiSolar", rf.antiSolar);
    shF(sh, "strength", rf.strength);
    shF(sh, "rainDist", std::max(s.weather.rainbow.rainDistance, 10.f));
    shF(sh, "waterLevel", s.water.enabled ? waterOf(s).level : 0.f);
    shI(sh, "waterOn", s.water.enabled ? 1 : 0);
    shF(sh, "gain", 0.75f);
    shI(sh, "depthTex", DEPTH_SLOT);
    bindUnit(DEPTH_SLOT, scene.depth);
    for (int pass = 0; pass < 2; pass++) {
        shI(sh, "pass", pass);
        if (pass == 0) rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);          // Alexander band: the sky darkened
        else rlSetBlendFactorsSeparate(RL_DST_COLOR, RL_ONE, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);                                     // light added in proportion to the sky behind: dst x (1 + bow)
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        BeginShaderMode(sh);
        DrawRectangle(0, 0, vw, vh, WHITE);
        EndShaderMode();
        rlDrawRenderBatchActive();
        EndBlendMode();
    }
    bindUnit(DEPTH_SLOT, 0);
    restoreBlend();
    rlMatrixMode(RL_MODELVIEW);
    rlPopMatrix();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlViewport(0, 0, fbW, fbH);
}

// The clouds over the finished opaque scene in `scene` (colour and depth), inside the viewport
// (vx, vyGL, vw, vh: framebuffer pixels, y up). The march runs at `res` of the viewport and a
// depth-aware upsample brings it to full size, where it is blended over the scene.
// `mirrored`: `cam` is the water's mirrored camera (see the reflection pass). Its picture is laid out as the
// real view's, so the screen's right is the mirror image of the one `cam` would give; and the march does
// not stop at the water plane.
void drawClouds(const Scene& s, const CloudFrame& f, const Camera& cam, const DepthTarget& scene, int fbW, int fbH,
                int vx, int vyGL, int vw, int vh, bool mirrored = false) {
    CloudGpu& g = gpu.cl;
    ensureCloudGpu(scene.fbo, fbW, fbH);
    const int lw = std::max(8, (int)std::ceil(vw * f.res)), lh = std::max(8, (int)std::ceil(vh * f.res));
    colorTarget(g.lowFbo, g.lowTex, g.lowW, g.lowH, lw, lh);
    colorTarget(g.fullFbo, g.fullTex, g.fullW, g.fullH, vw, vh);

    // The march.
    const Shader& m = g.march;
    setCloudCommon(m, f, { cam.position.x, cam.position.z });
    const Vector3 look = Vector3Subtract(cam.target, cam.position);
    const Vector3 fwd = Vector3Normalize(look);
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, safeUp(look, cam.upVector)));
    const Vector3 up = Vector3CrossProduct(right, fwd);
    if (mirrored) right = Vector3Scale(right, -1.f);
    const float tanY = std::tan(cam.fov * 0.5f * DEG2RAD), aspect = (float)vw / (float)vh;
    shV2(m, "lowSize", { (float)lw, (float)lh });
    Vector3 eye = cam.position;
    if (mirrored) eye.y = std::max(eye.y, 1.f);                  // the march wants the eye above the planet's surface (a few metres' error against clouds kilometres off)
    shV3(m, "camPos", eye);
    shV3(m, "camFwd", fwd);
    shV3(m, "camRight", right);
    shV3(m, "camUp", up);
    shV2(m, "tanHalf", { tanY * aspect, tanY });
    shV2(m, "vpOrigin", { (float)vx, (float)vyGL });
    shV2(m, "vpSize", { (float)vw, (float)vh });
    shV2(m, "zNF", { cam.nearPlane, cam.farPlane });
    shI(m, "depthTex", DEPTH_SLOT);
    shV3(m, "sunDir", f.sun);
    shV3(m, "sunE", f.sunE);
    shV3(m, "ambTop", f.ambTop);
    shV3(m, "ambBot", f.ambBot);
    shV3(m, "horizonCol", f.horizon);
    shV3(m, "hazeCol", f.haze);
    shF(m, "hazeAmount", f.hazeAmount);
    shF(m, "airK", 3.0e-5f * std::max(f.c.haze, 0.f));
    shI(m, "steps", f.steps);
    shI(m, "lightSteps", f.lightSteps);
    shF(m, "jitterOn", f.c.jitter ? 1.f : 0.f);
    shF(m, "denoiseOn", f.c.denoise ? 1.f : 0.f);
    shI(m, "blueTex", CLOUD_BLUE_SLOT);
    bindUnit(CLOUD_BLUE_SLOT, g.blue.id);
    shF(m, "waterLevel", s.water.enabled ? waterOf(s).level : 0.f);
    shI(m, "waterOn", s.water.enabled && !mirrored ? 1 : 0);
    const Clouds::Flash& fl = f.c.flash;
    const float fi = std::max(fl.intensity, 0.f);
    shV3(m, "flashPos", fl.position);
    shV3(m, "flashCol", Vector3Scale(linear3(fl.color), fi));
    shF(m, "flashRadius", std::max(fl.radius, 1.f));
    {   // the strongest lightning strike's channel in the cloud
        const FlashFrame ff = flashFrame(s, frameTime(gpu.frame));
        const float k = ff.on ? 5.f * s.lightning.cloudLight * ff.power : 0.f;
        shV3(m, "boltA", ff.segA);
        shV3(m, "boltB", ff.segB);
        shV3(m, "boltCol", Vector3Scale(linear3(s.lightning.color), k));
        shF(m, "boltRadius", ff.on && !ff.strike.toGround ? 700.f : 450.f);
    }
    bindUnit(DEPTH_SLOT, scene.depth);
    cloudPass(m, g.lowFbo, 0, 0, lw, lh);

    // The upsample, into a viewport-sized image.
    const Shader& u = g.up;
    shV2(u, "lowSize", { (float)lw, (float)lh });
    shV2(u, "vpOrigin", { (float)vx, (float)vyGL });
    shV2(u, "vpSize", { (float)vw, (float)vh });
    shV2(u, "zNF", { cam.nearPlane, cam.farPlane });
    shI(u, "lowTex", CLOUD_LOW_SLOT);
    shF(u, "denoiseOn", f.c.denoise ? 1.f : 0.f);
    shI(u, "depthTex", DEPTH_SLOT);
    bindUnit(CLOUD_LOW_SLOT, g.lowTex);
    cloudPass(u, g.fullFbo, 0, 0, vw, vh);
    unbindCloudUnits();

    // Over the scene (straight colour and alpha onto a premultiplied target).
    rlEnableFramebuffer(scene.fbo);
    rlViewport(vx, vyGL, vw, vh);
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    rlOrtho(0, vw, vh, 0, -1, 1);
    rlMatrixMode(RL_MODELVIEW);
    rlPushMatrix();
    rlLoadIdentity();
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    const Texture2D full{ g.fullTex, vw, vh, 1, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16 };
    DrawTexturePro(full, { 0, 0, (float)vw, -(float)vh }, { 0, 0, (float)vw, (float)vh }, { 0, 0 }, 0, WHITE);
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_MODELVIEW);
    rlPopMatrix();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlViewport(0, 0, fbW, fbH);
}

// ---- light in the air (Scene::volumetric, Scene::aerial) ------------------------------------------------
DepthTarget& copyScene(const DepthTarget& src);

// Something for the march to go through: the scene's fog (own, the weather's, or the one Volumetric::density sets).
bool airFogged(const Scene& s) { return s.fogMode == FogMode::Linear ? s.fogFar > s.fogNear : s.fogDensity > 0; }
bool airEye(const Scene& s, const Camera& cam) {          // a perspective view from the air
    return !cam.orthographic && !(s.water.enabled && cam.position.y < waterOf(s).level);
}
bool volumetricActive(const Scene& s, const Camera& cam) { return s.volumetric.enabled() && airEye(s, cam) && airFogged(s); }
bool aerialActive(const Scene& s, const Camera& cam) { return s.aerial.enabled && s.aerial.strength > 0 && airEye(s, cam); }

// The light that is the sun for the shafts: the sky's, else the directional light along the sky's sun, else the first.
int airSunLight(const Scene& s) {
    const int n = std::min((int)s.lights.size(), MAX_LIGHTS);
    auto towards = [&](int i, Vector3& d) {
        if (i < 0 || i >= n || s.lights[(size_t)i].type != Light::Type::Directional) return false;
        d = Vector3Subtract(s.lights[(size_t)i].position, s.lights[(size_t)i].target);
        if (Vector3Length(d) < 1e-6f) return false;
        d = Vector3Normalize(d);
        return true;
    };
    Vector3 d;
    if (s.sky.enabled && towards(s.sky.sunLight, d)) return s.sky.sunLight;
    if (s.sky.enabled)
        for (int i = 0; i < n; i++)
            if (towards(i, d) && Vector3DotProduct(d, sunOf(s)) > 0.97f) return i;
    for (int i = 0; i < n; i++)
        if (towards(i, d)) return i;
    return -1;
}

// How far a ray is marched when nothing stops it: to where the fog is all but opaque, within the far plane.
float airReach(const Scene& s, const Camera& cam) {
    if (s.fogMode == FogMode::Linear) return std::clamp(s.fogFar, 1.f, cam.farPlane);
    float sigma = s.fogDensity;
    if (s.fogFalloff > 0) sigma *= std::exp(std::clamp(-s.fogFalloff * (cam.position.y - s.fogHeight), -30.f, 30.f));
    return std::clamp((s.fogMode == FogMode::Exponential ? 3.9f : 1.98f) / std::max(sigma, 1e-7f), 10.f, cam.farPlane);
}

// The environment the lit shader reads, for the two air shaders.
void setAirEnvironment(const Shader& sh, const Scene& s, Texture2D envTex) {
    const int hasEnv = envTex.id ? 1 : 0, envLin = hasEnv && isFloatFormat(envTex.format) ? 1 : 0;
    const float maxLod = hasEnv ? std::floor(std::log2((float)std::max(envTex.width, envTex.height))) : 0;
    const float rot = s.sky.enabled ? 0.f : s.environmentRotation / 360.f;
    const float intensity = s.sky.enabled ? s.sky.lighting : s.environmentIntensity;
    const float backdrop = s.sky.enabled ? 1.f / std::max(intensity, 1e-4f) : 1.f;
    const Vector3 zen = linear3(s.skyColor), fallback = Vector3Lerp(zen, { 1, 1, 1 }, 0.4f);
    shI(sh, "hasEnv", hasEnv); shI(sh, "envLinear", envLin); shF(sh, "envMaxLod", maxLod);
    shF(sh, "envIntensity", intensity); shF(sh, "envRotation", rot); shF(sh, "backdropScale", backdrop);
    shV3(sh, "horizonFallback", fallback);
    shI(sh, "envMap", ENV_SLOT);
}

// The picture's fog made into shafts, and the aerial perspective, over the finished opaque scene in `scene`
// (colour and depth), inside the viewport (vx, vyGL, vw, vh). The march writes an image at a share of the size, the
// composite brings it up with a depth-aware filter, adds it and applies the aerial perspective.
void drawAir(const Scene& s, const Camera& cam, const Shadows& sh, Texture2D envTex, const DepthTarget& scene, int fbW, int fbH,
             int vx, int vyGL, int vw, int vh, bool vol, bool aerial) {
    AirGpu& g = gpu.air;
    if (!g.comp.id) {
        g.march = LoadShaderFromMemory(nullptr, AIR_MARCH_FS);
        g.comp = LoadShaderFromMemory(nullptr, AIR_COMPOSITE_FS);
        if (!IsShaderValid(g.march) || !IsShaderValid(g.comp)) throw std::runtime_error("g3d: volumetric light shader compilation failed");
    }
    const Volumetric& v = s.volumetric;
    const Vector3 look = Vector3Subtract(cam.target, cam.position);
    const Vector3 fwd = Vector3Normalize(look);
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, safeUp(look, cam.upVector))), up = Vector3CrossProduct(right, fwd);
    const float tanY = std::tan(cam.fov * 0.5f * DEG2RAD), aspect = (float)vw / (float)vh;
    auto camera = [&](const Shader& sh2, Vector2 lowSize) {
        shV3(sh2, "camPos", cam.position); shV3(sh2, "camFwd", fwd); shV3(sh2, "camRight", right); shV3(sh2, "camUp", up);
        shV2(sh2, "tanHalf", { tanY * aspect, tanY });
        shV2(sh2, "vpOrigin", { (float)vx, (float)vyGL });
        shV2(sh2, "vpSize", { (float)vw, (float)vh });
        shV2(sh2, "zNF", { cam.nearPlane, cam.farPlane });
        shV2(sh2, "lowSize", lowSize);
    };
    const DepthTarget& copy = copyScene(scene);                  // colour and depth, to read while the picture is written
    int lw = 1, lh = 1;
    if (vol) {
        struct Q { int steps; float res; };
        static const Q table[3] = { { 16, 0.35f }, { 32, 0.5f }, { 48, 0.75f } };
        const Q& q = table[std::clamp((int)v.quality - 1, 0, 2)];
        lw = std::max(8, (int)std::ceil(vw * q.res)); lh = std::max(8, (int)std::ceil(vh * q.res));
        colorTarget(g.lowFbo, g.lowTex, g.lowW, g.lowH, lw, lh);
        const Shader& m = g.march;
        camera(m, { (float)lw, (float)lh });
        setAirEnvironment(m, s, envTex);
        const Vector3 fogCol = linear(s.fogColor);
        shV3(m, "fogColor", fogCol); shF(m, "fogNear", s.fogNear); shF(m, "fogFar", s.fogFar);
        shI(m, "fogMode", (int)s.fogMode); shF(m, "fogDensity", s.fogDensity); shF(m, "fogHeight", s.fogHeight); shF(m, "fogFalloff", s.fogFalloff);
        shI(m, "fogSky", s.fogFromSky ? 1 : 0);
        shI(m, "seaSky", s.water.enabled && s.water.spectrum.enabled ? 1 : 0);
        shI(m, "steps", q.steps); shF(m, "reach", airReach(s, cam));
        shF(m, "waterLevel", s.water.enabled ? waterOf(s).level : 0.f); shI(m, "waterOn", s.water.enabled ? 1 : 0);
        const int sun = airSunLight(s);
        float gain = 0;
        Vector3 toSun{ 0, 1, 0 };
        if (sun >= 0) {
            const Light& l = s.lights[(size_t)sun];
            toSun = Vector3Normalize(Vector3Subtract(l.position, l.target));
            const float k = std::clamp((toSun.y + 0.02f) / 0.08f, 0.f, 1.f);
            gain = l.intensity > 0 ? k * k * (3 - 2 * k) : 0.f;
            if (s.weather.enabled && !s.sky.ownLight) gain *= std::clamp(s.weather.sunScale(), 0.f, 1.f);
        }
        shI(m, "sun", sun); shV3(m, "sunDir", toSun); shF(m, "sunGain", gain);
        shF(m, "aniso", std::clamp(v.anisotropy, 0.f, 0.95f)); shF(m, "ambientK", std::clamp(v.ambient, 0.f, 1.f));
        const CloudGpu& cg = gpu.cl;
        shI(m, "cloudShadowOn", cg.shadowOn ? 1 : 0); shI(m, "cloudShadowLight", cg.shadowLight);
        shV3(m, "cloudShadowSun", cg.shadowSun); shF(m, "cloudShadowFloor", cg.shadowFloor);
        { const Vector4 map = cg.shadowMap; SetShaderValue(m, GetShaderLocation(m, "cloudShadowMap"), &map, SHADER_UNIFORM_VEC4); }
        shI(m, "cloudShadowTex", CLOUD_SLOT);
        // The lights, as the lit shader gets them.
        const int count = std::min((int)s.lights.size(), MAX_LIGHTS);
        int type[MAX_LIGHTS] = {};
        Vector3 pos[MAX_LIGHTS] = {}, dir[MAX_LIGHTS] = {}, col[MAX_LIGHTS] = {};
        float range[MAX_LIGHTS] = {}, cin[MAX_LIGHTS] = {}, cosOut[MAX_LIGHTS] = {}, bias[MAX_LIGHTS] = {};
        for (int i = 0; i < count; i++) {
            const Light& l = s.lights[(size_t)i];
            type[i] = (int)l.type; pos[i] = l.position;
            const Vector3 d = Vector3Subtract(l.target, l.position);
            dir[i] = Vector3Length(d) > 1e-6f ? Vector3Normalize(d) : Vector3{ 0, -1, 0 };
            col[i] = linear(l.color, l.intensity); range[i] = l.range;
            const float outer = std::clamp(l.outerAngle, 0.1f, 89.9f), inner = std::clamp(l.innerAngle, 0.f, outer - 0.05f);
            cin[i] = std::cos(inner * DEG2RAD); cosOut[i] = std::cos(outer * DEG2RAD); bias[i] = l.shadowBias;
        }
        shI(m, "scatterLights", v.scatterLights ? 1 : 0); shI(m, "lightCount", count);
        SetShaderValueV(m, GetShaderLocation(m, "lightType"), type, SHADER_UNIFORM_INT, MAX_LIGHTS);
        SetShaderValueV(m, GetShaderLocation(m, "lightPos"), pos, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
        SetShaderValueV(m, GetShaderLocation(m, "lightDir"), dir, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
        SetShaderValueV(m, GetShaderLocation(m, "lightColor"), col, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
        SetShaderValueV(m, GetShaderLocation(m, "lightRange"), range, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
        SetShaderValueV(m, GetShaderLocation(m, "lightCosInner"), cin, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
        SetShaderValueV(m, GetShaderLocation(m, "lightCosOuter"), cosOut, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
        SetShaderValueV(m, GetShaderLocation(m, "lightShadow"), sh.lightTile, SHADER_UNIFORM_INT, MAX_LIGHTS);
        SetShaderValueV(m, GetShaderLocation(m, "lightBias"), bias, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
        if (!sh.tiles.empty()) {
            Matrix vps[MAX_TILES];
            Vector4 rects[MAX_TILES], params[MAX_TILES];
            for (size_t i = 0; i < sh.tiles.size(); i++) { vps[i] = sh.tiles[i].vp; rects[i] = sh.tiles[i].rect; params[i] = sh.tiles[i].params; }
            rlEnableShader(m.id);
            rlSetUniformMatrices(GetShaderLocation(m, "shadowVP"), vps, (int)sh.tiles.size());
            SetShaderValueV(m, GetShaderLocation(m, "shadowRect"), rects, SHADER_UNIFORM_VEC4, (int)sh.tiles.size());
            SetShaderValueV(m, GetShaderLocation(m, "shadowParams"), params, SHADER_UNIFORM_VEC4, (int)sh.tiles.size());
            shF(m, "atlasTexel", 1.f / (float)gpu.atlasSize);
        }
        shI(m, "shadowAtlas", SHADOW_SLOT);
        shI(m, "depthTex", DEPTH_SLOT);
        bindUnit(DEPTH_SLOT, copy.depth);
        bindUnit(SHADOW_SLOT, sh.tiles.empty() ? 0 : gpu.atlasTex);
        bindUnit(ENV_SLOT, envTex.id);
        if (cg.shadowOn) bindUnit(CLOUD_SLOT, cg.shadowRT.texture.id);
        cloudPass(m, g.lowFbo, 0, 0, lw, lh);
        for (int slot : { SHADOW_SLOT, CLOUD_SLOT }) bindUnit(slot, 0);
    }
    const Shader& c = g.comp;
    camera(c, { (float)lw, (float)lh });
    setAirEnvironment(c, s, envTex);
    shI(c, "volOn", vol ? 1 : 0); shI(c, "filterOn", 1);
    shI(c, "aerialOn", aerial ? 1 : 0);
    const AerialPerspective& a = s.aerial;
    shF(c, "aerialK", std::max(a.strength, 0.f) / 30000.f); shF(c, "aerialH", std::max(a.scaleHeight, 10.f));
    shV3(c, "aerialRGB", { 0.77f, 1.f, 1.7f });
    shF(c, "waterLevel", s.water.enabled ? waterOf(s).level : 0.f); shI(c, "waterOn", s.water.enabled ? 1 : 0);
    shI(c, "sceneTex", REFR_COLOR_SLOT); shI(c, "depthTex", DEPTH_SLOT); shI(c, "lowTex", CLOUD_LOW_SLOT);
    bindUnit(REFR_COLOR_SLOT, copy.color);
    bindUnit(DEPTH_SLOT, copy.depth);
    bindUnit(CLOUD_LOW_SLOT, g.lowTex);
    bindUnit(ENV_SLOT, envTex.id);
    cloudPass(c, scene.fbo, vx, vyGL, vw, vh);
    for (int slot : { REFR_COLOR_SLOT, DEPTH_SLOT, CLOUD_LOW_SLOT, ENV_SLOT }) bindUnit(slot, 0);
    rlEnableFramebuffer(scene.fbo);
    rlViewport(0, 0, fbW, fbH);
}

// While the volumetric march has the fog, the scene is drawn without it (the march fogs the finished picture); released
// before the march reads the scene's fog.
struct FogHold {
    Scene& s;
    FogMode mode;
    float density;
    bool held;
    FogHold(Scene& sc, bool hold) : s(sc), mode(sc.fogMode), density(sc.fogDensity), held(hold) {
        if (held) { s.fogMode = FogMode::Exponential; s.fogDensity = 0; }
    }
    void release() { if (held) { s.fogMode = mode; s.fogDensity = density; held = false; } }
    ~FogHold() { release(); }
    FogHold(const FogHold&) = delete;
    FogHold& operator=(const FogHold&) = delete;
};

// ---- the water's planar reflection and refraction (Water::reflections, Water::refraction)
// How the camera sees the water this frame: above the surface, and with water somewhere in the picture
// (some corner of the view frustum lies under the surface plane).
struct WaterView { bool above = false, visible = false; Water water; float level = 0; };
WaterView waterView(const Scene& s, const Camera& cam, float aspect, int t) {
    WaterView v;
    if (!s.water.enabled || cam.orthographic) return v;
    v.water = waterOf(s);
    v.level = v.water.level;
    const double seconds = frameTime(t) / std::max(v.water.fps, 1.f);
    v.above = cam.position.y >= v.water.height(cam.position.x, cam.position.z, seconds);
    if (!v.above) return v;
    const Vector3 look = Vector3Subtract(cam.target, cam.position);
    const Vector3 f = Vector3Normalize(look);
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(f, safeUp(look, cam.upVector)));
    const Vector3 up = Vector3CrossProduct(right, f);
    const float ty = std::tan(cam.fov * 0.5f * DEG2RAD), tx = ty * aspect;
    const bool spectrum = v.water.spectrum.enabled;
    const float band = (spectrum ? 2 * v.water.spectrumHs() : packWaves(v.water).amplitude) + 0.05f;
    const float far = spectrum ? std::max(cam.farPlane, 1.5f * seaHorizon(std::max(cam.position.y - v.level, 0.f)) + 500.f) : cam.farPlane;
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) {
            const float y = cam.position.y + (f.y + right.y * sx * tx + up.y * sy * ty) * far;
            if (y < v.level + band) v.visible = true;
        }
    return v;
}
// The mirror image of the camera in the water plane: what the reflection pass looks from.
Camera mirroredCamera(const Camera& cam, float level) {
    Camera m = cam;
    m.up = nullptr;
    m.dof.aperture = 0;
    m.position.y = 2 * level - cam.position.y;
    m.target.y = 2 * level - cam.target.y;
    m.upVector.y = -cam.upVector.y;
    return m;
}
// An offscreen target that is read back as a texture: linear filtering, clamped, with `mips` the colour's chain.
DepthTarget& waterTarget(DepthTarget& slot, int w, int h, bool mips) {
    const bool fresh = !(slot.fbo && slot.w == w && slot.h == h);
    DepthTarget& t = depthTarget(slot, w, h);
    if (fresh) {
        rlTextureParameters(t.color, RL_TEXTURE_MIN_FILTER, mips ? RL_TEXTURE_FILTER_MIP_LINEAR : RL_TEXTURE_FILTER_LINEAR);
        rlTextureParameters(t.color, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
        for (unsigned int id : { t.color, t.depth }) {
            rlTextureParameters(id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
            rlTextureParameters(id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
        }
        rlTextureParameters(t.depth, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
        rlTextureParameters(t.depth, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
    }
    return t;
}

// ---- water bodies (Scene::waterBodies)
constexpr size_t MAX_WATER_BODIES = 32;
bool validBody(const WaterBody& b) { return b.isRiver() || b.outline.size() >= 3; }

// Whether a body can be on screen (its bounding circle against the view cone), and how far it is along the view.
bool bodyInView(const WaterBody& b, const Camera& cam, float aspect, float* depth = nullptr) {
    const Vector4 e = b.extent();
    const Vector3 c{ 0.5f * (e.x + e.z), b.level, 0.5f * (e.y + e.w) };
    const float r = 0.5f * std::hypot(e.z - e.x, e.w - e.y) + 1.f;
    const Matrix view = MatrixLookAt(cam.position, cam.target, safeUp(Vector3Subtract(cam.target, cam.position), cam.upVector));
    const Vector3 p = Vector3Transform(c, view);
    if (depth) *depth = -p.z;
    if (-p.z + r < cam.nearPlane || -p.z - r > cam.farPlane) return false;
    if (cam.orthographic) return true;
    const float ty = std::tan(cam.fov * 0.5f * DEG2RAD), tx = ty * aspect, z = std::max(-p.z, 0.f);
    return std::fabs(p.x) <= z * tx + r * 1.5f && std::fabs(p.y) <= z * ty + r * 1.5f;
}

// The one planar reflection a frame can draw: the sea's if it reflects and is in view, else the first body (in
// view, camera above its level) whose look reflects. `visible` is false when nothing needs the mirrored scene.
WaterView reflectionView(const Scene& s, const Camera& cam, float aspect, int t) {
    if (cam.orthographic) return {};
    if (s.water.enabled && s.water.reflections != Water::Reflections::Off) {
        const WaterView v = waterView(s, cam, aspect, t);
        if (v.visible) return v;
    }
    size_t n = 0;
    for (const WaterBody& b : s.waterBodies) {
        if (n++ >= MAX_WATER_BODIES) break;
        if (!validBody(b) || b.look.reflections == Water::Reflections::Off || cam.position.y < b.level) continue;
        if (!bodyInView(b, cam, aspect)) continue;
        WaterView v;
        v.water = waterOf(s, b);
        v.level = b.level;
        v.above = v.visible = true;
        return v;
    }
    return {};
}

// How far the waves bend a reflection: full from a significant wave height of 0.1 m, none at 1 cm.
float reflectionBend(const Water& w) {
    float sumA2 = 0;
    for (const Water::Wave& wv : w.waves) sumA2 += wv.amplitude * wv.amplitude * 0.5f;
    const float hs = w.spectrum.enabled ? w.spectrumHs() : 4 * std::sqrt(sumA2), tq = std::clamp((hs - 0.01f) / 0.09f, 0.f, 1.f);
    return std::max(w.reflectionDistort, 0.f) * tq * tq * (3 - 2 * tq);
}

struct BodyDraw { const WaterBody* b; Water look; Object3D* obj; float depth; };

Mesh makeBodyMesh(const WaterBody& b) {
    const WaterBodyMesh src = waterBodyMesh(b);
    Mesh m = {};
    const bool empty = src.index.size() < 3;
    m.vertexCount = empty ? 3 : (int)(src.position.size() / 3);
    m.triangleCount = empty ? 1 : (int)(src.index.size() / 3);
    m.vertices = (float*)MemAlloc((unsigned)m.vertexCount * 3 * sizeof(float));
    m.normals = (float*)MemAlloc((unsigned)m.vertexCount * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc((unsigned)m.vertexCount * 2 * sizeof(float));
    m.indices = (unsigned short*)MemAlloc((unsigned)m.triangleCount * 3 * sizeof(unsigned short));
    std::fill(m.vertices, m.vertices + m.vertexCount * 3, 0.f);
    std::fill(m.normals, m.normals + m.vertexCount * 3, 0.f);
    std::fill(m.texcoords, m.texcoords + m.vertexCount * 2, 0.f);
    if (empty) {
        m.indices[0] = 0; m.indices[1] = 1; m.indices[2] = 2;
    } else {
        std::copy(src.position.begin(), src.position.end(), m.vertices);
        std::copy(src.normal.begin(), src.normal.end(), m.normals);
        std::copy(src.uv.begin(), src.uv.end(), m.texcoords);
        std::copy(src.index.begin(), src.index.end(), m.indices);
    }
    UploadMesh(&m, false);
    return m;
}

// The drawing object of a body's shape: built once, kept by the shape's key, dropped when unused for a while.
Object3D& bodyObject(const WaterBody& b, int stamp) {
    auto& slot = gpu.bodyMeshes[waterBodyKey(b)];
    if (!slot.obj) {
        slot.obj = std::make_unique<Object3D>();
        slot.obj->name = "waterBody";
        auto shape = std::make_shared<WaterBody>(b);
        slot.obj->geometry = std::make_shared<Geometry>([shape] { return makeBodyMesh(*shape); });
        slot.obj->material.doubleSided = true;
        slot.obj->material.castShadow = false;
    }
    slot.used = stamp;
    return *slot.obj;
}

// The bodies that can be seen this frame (at most MAX_WATER_BODIES of the scene's, in order).
std::vector<BodyDraw> collectBodies(const Scene& s, const Camera& cam, float aspect, int stamp) {
    std::vector<BodyDraw> out;
    size_t n = 0;
    for (const WaterBody& b : s.waterBodies) {
        if (n++ >= MAX_WATER_BODIES) break;
        float depth = 0;
        if (!validBody(b) || cam.position.y < b.level - 0.01f || !bodyInView(b, cam, aspect, &depth)) continue;
        out.push_back({ &b, waterOf(s, b), &bodyObject(b, stamp), depth });
    }
    for (auto it = gpu.bodyMeshes.begin(); gpu.bodyMeshes.size() > 48 && it != gpu.bodyMeshes.end();)
        it = it->second.used != stamp ? gpu.bodyMeshes.erase(it) : std::next(it);
    return out;
}

// The lit shader's water uniforms for drawing one body: its look, its level, mode 2, and its shore and flow.
// How rough a body's water is, 0 (glass) to 1 (a gale on a lake): the significant wave height of the waves it is
// drawn with (the weather's wind and the body's fetch worked in), and for a river the current's speed as well.
// Shore foam scales with it: a calm lake keeps a few thin threads, a breezy one a net, a fast river whitewater.
float bodyEnergy(const Water& w, const WaterBody& b) {
    float ss = 0;
    for (const Water::Wave& v : w.waves) ss += v.amplitude * v.amplitude;
    const float hs = 2.83f * std::sqrt(ss);
    auto smooth = [](float a, float c, float x) { const float t = std::clamp((x - a) / (c - a), 0.f, 1.f); return t * t * (3 - 2 * t); };
    float e = smooth(0.02f, 0.3f, hs);
    if (b.isRiver()) e = std::max(e, smooth(0.8f, 3.2f, b.flowSpeed));
    return e;
}

void setBodyUniforms(const Scene& s, const Camera& cam, const BodyDraw& d) {
    const Water& w = d.look;
    const WaterBody& b = *d.b;
    const Shader& lit = gpu.lit;
    const double seconds = frameTime(gpu.frame) / std::max(w.fps, 1.f);
    setWaterLook(s, cam, w, -1e9f, seconds);                // the camera is above it: never "from below"
    const int on = 1, mode = 2;
    SetShaderValue(lit, gpu.W.on, &on, SHADER_UNIFORM_INT);
    SetShaderValue(lit, gpu.W.mode, &mode, SHADER_UNIFORM_INT);
    const float energy = bodyEnergy(w, b);
    const Vector4 shore{ std::max(b.edgeSoft, 0.f), std::max(b.foamWidth, 0.01f) * (0.4f + 0.6f * energy), std::clamp(b.shoreFoam, 0.f, 1.f), energy };
    const Vector4 flow{ std::max(b.flowPeriod, 0.2f), std::max(b.ripple, 0.f), std::max(b.rippleLength, 0.05f), b.isRiver() ? 1.f : 0.f };
    SetShaderValue(lit, GetShaderLocation(lit, "bodyShore"), &shore, SHADER_UNIFORM_VEC4);
    SetShaderValue(lit, GetShaderLocation(lit, "bodyFlow"), &flow, SHADER_UNIFORM_VEC4);
    { const int dbg = s.ground.debug; SetShaderValue(lit, GetShaderLocation(lit, "bodyDebug"), &dbg, SHADER_UNIFORM_INT); }
}

// The scene mirrored about the water level, into the reflection target, as the viewing camera sees it
// (so a point's mirror image lands on the pixel that shows its reflection). Only the world is mirrored: the
// view matrix gets the flip, the lights, shadows and world positions stay real; the eye the shader sees
// is the mirrored one. What lies under the level is discarded in the lit shader (reflClip), which keeps the
// depth buffer an ordinary perspective one for the cloud march. The winding flips with the mirror: front
// faces are culled instead of back faces. `mirrorItems` are collected against the mirrored frustum.
void renderReflection(Scene& s, const Camera& cam, const Camera& mirror, const WaterView& wv, std::vector<Item>& mirrorItems,
                      const Shadows& shadows, Texture2D envTex, float scale, const CloudFrame* clouds, int vw, int vh) {
    const float q = wv.water.reflections == Water::Reflections::Half ? 0.5f : wv.water.reflections == Water::Reflections::Quarter ? 0.25f : 1.f;
    const int rw = std::max(16, (int)std::lround(vw * q)), rh = std::max(16, (int)std::lround(vh * q));
    DepthTarget& rt = waterTarget(gpu.reflTarget, rw, rh, true);
    const long long blades = gpu.fieldBlades, shadowBlades = gpu.fieldShadowBlades, bodies = gpu.instanceBodies, tris = gpu.instanceTriangles;
    const Vector3 savedView = gpu.viewCam;
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(rt.fbo);
    rlViewport(0, 0, rw, rh);
    rlEnableDepthMask();
    rlClearColor(0, 0, 0, 0);
    rlClearScreenBuffers();
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    const double aspect = (double)vw / vh, top = cam.nearPlane * std::tan(cam.fov * 0.5 * DEG2RAD);
    rlFrustum(-top * aspect, top * aspect, -top, top, cam.nearPlane, cam.farPlane);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    const Matrix view = MatrixLookAt(cam.position, cam.target, safeUp(Vector3Subtract(cam.target, cam.position), cam.upVector));
    const Matrix flip = MatrixMultiply(MatrixScale(1, -1, 1), MatrixTranslate(0, 2 * wv.level, 0));
    const Matrix viewR = MatrixMultiply(flip, view);
    rlMultMatrixf(MatrixToFloat(viewR));
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlSetCullFace(RL_CULL_FACE_FRONT);

    setSceneUniforms(s, cam, shadows, envTex, scale);
    const int zero = 0, one = 1;
    gpu.viewCam = mirror.position;
    SetShaderValue(gpu.lit, gpu.L.viewPos, &mirror.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(gpu.lit, gpu.W.on, &zero, SHADER_UNIFORM_INT);
    SetShaderValue(gpu.lit, gpu.W.level, &wv.level, SHADER_UNIFORM_FLOAT);        // what is cut away is what lies under this plane (a body's level may not be the sea's)
    SetShaderValue(gpu.lit, gpu.W.reflClip, &one, SHADER_UNIFORM_INT);
    bindUnit(SHADOW_SLOT, shadows.tiles.empty() ? 0 : gpu.atlasTex);
    bindUnit(ENV_SLOT, envTex.id);
    if (gpu.cl.shadowOn) bindUnit(CLOUD_SLOT, gpu.cl.shadowRT.texture.id);
    std::vector<Item> clear;
    for (Item& it : mirrorItems) {
        if (opacityOf(it) < 1.f) {
            const Vector3 p = Vector3Transform({ 0, 0, 0 }, MatrixMultiply(it.world, viewR));
            it.viewDepth = -p.z;
            clear.push_back(it);
        } else drawItem(it, Pass::Lit);
    }
    std::stable_sort(clear.begin(), clear.end(), [](const Item& a, const Item& b) { return a.viewDepth > b.viewDepth; });
    rlDisableDepthMask();
    for (const Item& it : clear) drawItem(it, Pass::Lit);
    rlEnableDepthMask();
    rlSetCullFace(RL_CULL_FACE_BACK);
    bindUnit(SHADOW_SLOT, 0);
    bindUnit(ENV_SLOT, 0);
    bindUnit(CLOUD_SLOT, 0);
    SetShaderValue(gpu.lit, gpu.W.reflClip, &zero, SHADER_UNIFORM_INT);
    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    if (clouds) drawClouds(s, *clouds, mirror, rt, rw, rh, 0, 0, rw, rh, true);
    int mips = 0;
    rlGenTextureMipmaps(rt.color, rw, rh, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, &mips);
    gpu.fieldBlades = blades; gpu.fieldShadowBlades = shadowBlades; gpu.instanceBodies = bodies; gpu.instanceTriangles = tris;
    gpu.viewCam = savedView;
}

// Copies the colour and depth of `src` (the opaque scene so far) into the refraction target.
DepthTarget& copyScene(const DepthTarget& src) {
    DepthTarget& dst = waterTarget(gpu.refrTarget, src.w, src.h, false);
    rlDrawRenderBatchActive();
    rlBindFramebuffer(RL_READ_FRAMEBUFFER, src.fbo);
    rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, dst.fbo);
    rlBlitFramebuffer(0, 0, src.w, src.h, 0, 0, src.w, src.h, 0x00000100 | 0x00004000);   // depth and colour
    rlEnableFramebuffer(src.fbo);
    return dst;
}

// ---- precipitation (Scene::precipitation): see Downfall in scene3d.hpp and precip_glsl.hpp
// The settings of the frame being drawn (with the weather worked in) and what the occlusion pass made.
struct PrecipFrame {
    bool on = false;
    Downfall d;
    double secs = 0;
    Vector2 aRain{ 0, 0 }, aSnow{ 0, 0 };
    bool map = false;                    // the depth map from above was drawn this frame
    Matrix occVP{}, occInv{};
    float occRange = 1;
};
constexpr float PRECIP_BAND = 0.1f;      // the shader's rank band (BAND)

PrecipFrame precipFrame(const Scene& s) {
    PrecipFrame f;
    f.d = inWeather(s.precipitation, s.weather);
    f.on = f.d.enabled && f.d.kind != Precipitation::None && f.d.intensity > 0.001f && f.d.radius > 1.f;
    f.secs = frameTime(gpu.frame) / std::max(f.d.fps, 1.f);
    f.aRain = f.d.slant(false);
    f.aSnow = f.d.slant(true);
    return f;
}

float smooth01(float e0, float e1, float x) {
    const float u = std::clamp((x - e0) / std::max(e1 - e0, 1e-6f), 0.f, 1.f);
    return u * u * (3 - 2 * u);
}
// The shader's near-layer keep fraction at a distance.
float precipKeep(const Downfall& d, float dist) {
    return std::min(1.f, std::pow(0.35f * d.radius / std::max(dist, 0.01f), 1.5f)) * (1 - smooth01(0.75f * d.radius, d.radius, dist));
}

// Renders the opaque scene's depth from above along the fall into gpu.pr.occ, over `radius` round the camera.
// It is needed to keep the rain off what is under a roof (occlusion) and to find where drops land (splashes).
void renderPrecipOcclusion(const Camera& cam, PrecipFrame& f, const std::vector<Item>& items) {
    const Downfall& d = f.d;
    const bool splash = d.splashes && d.kind != Precipitation::Snow;
    if (!f.on || !(d.occlusion || splash)) return;
    const int N = std::clamp(d.occlusionSize, 128, 4096);
    DepthTarget& t = waterTarget(gpu.pr.occ, N, N, false);
    const Vector2 a = d.kind == Precipitation::Snow ? f.aSnow : f.aRain;
    const Vector3 dir = Vector3Normalize({ a.x, -1.f, a.y });                // the way a drop moves
    const Vector3 up0 = safeUp(dir, { 0, 1, 0 });
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(dir, up0)), up = Vector3CrossProduct(right, dir);
    const float E = d.radius + 4.f, texel = 2 * E / (float)N;
    // The map's centre moves in whole texels, so a roof's edge does not crawl as the camera moves.
    Vector3 target = cam.position;
    const float cx = Vector3DotProduct(target, right), cy = Vector3DotProduct(target, up);
    target = Vector3Add(target, Vector3Add(Vector3Scale(right, std::floor(cx / texel) * texel - cx), Vector3Scale(up, std::floor(cy / texel) * texel - cy)));
    const float back = 80.f, zNear = 0.5f, zFar = back + 120.f;
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(t.fbo);
    rlViewport(0, 0, N, N);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlClearColor(0, 0, 0, 0);
    rlClearScreenBuffers();
    rlDisableBackfaceCulling();                      // a thin roof is a roof from either side
    Vector4 params;
    f.occVP = loadTileCamera(Vector3Subtract(target, Vector3Scale(dir, back)), target, up0, false, E, zNear, zFar, N, params);
    f.occInv = MatrixInvert(f.occVP);
    f.occRange = zFar - zNear;
    for (const Item& it : items)
        if (!it.particle && !it.tiles && opacityOf(it) >= 1.f) drawItem(it, Pass::Depth);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    f.map = true;
}

struct PrecipTile { int ix, iy, iz, n; };

// The tiles of one layer (0 near, 1 far, 2 splashes) that can be seen, each with how many of its drops to draw.
void gatherPrecip(const Downfall& d, Vector2 a, const Camera& cam, float aspect, int layer, std::vector<PrecipTile>& out) {
    const bool farL = layer == 1;
    const float S = farL ? Downfall::kFarTile : Downfall::kTile, Hc = farL ? Downfall::kFarCell : Downfall::kCell;
    const uint32_t full = farL ? d.farDropsPerTile() : d.dropsPerTile();
    const float rMax = layer == 0 ? d.radius : layer == 1 ? d.farRadius : d.splashRadius, rMin = farL ? 0.7f * d.radius : 0.f;
    const float I = std::clamp(d.intensity, 0.f, 1.f);
    const Vector3 c0 = cam.position;
    const float yLo = c0.y - (farL ? 0.6f * d.farRadius : rMax), yHi = c0.y + (farL ? 0.4f * d.farRadius : rMax);
    const int iy0 = (int)std::floor(yLo / Hc), iy1 = (int)std::floor(yHi / Hc);
    const int ix0 = (int)std::floor((c0.x - rMax + std::min(a.x, 0.f) * Hc) / S) - 1, ix1 = (int)std::floor((c0.x + rMax + std::max(a.x, 0.f) * Hc) / S) + 1;
    const int iz0 = (int)std::floor((c0.z - rMax + std::min(a.y, 0.f) * Hc) / S) - 1, iz1 = (int)std::floor((c0.z + rMax + std::max(a.y, 0.f) * Hc) / S) + 1;
    const float rad = std::sqrt(std::pow(S * 0.5f + std::fabs(a.x) * Hc * 0.5f, 2.f) + Hc * Hc * 0.25f + std::pow(S * 0.5f + std::fabs(a.y) * Hc * 0.5f, 2.f));
    for (int iy = iy0; iy <= iy1; iy++)
        for (int iz = iz0; iz <= iz1; iz++)
            for (int ix = ix0; ix <= ix1; ix++) {
                const Vector3 c{ ((float)ix + 0.5f) * S - a.x * Hc * 0.5f, ((float)iy + 0.5f) * Hc, ((float)iz + 0.5f) * S - a.y * Hc * 0.5f };
                const float dist = Vector3Distance(c, c0);
                if (dist - rad > rMax || (farL && dist + rad < rMin)) continue;
                if (!visibleSphere(cam, aspect, c, rad)) continue;
                const float dn = std::max(0.f, dist - rad);
                const float keep = layer == 0 ? precipKeep(d, dn) : layer == 1 ? 1.f : 1 - smooth01(0.5f * d.splashRadius, d.splashRadius, dn);
                const float m = I * keep;
                const int n = std::min((int)full, (int)std::ceil((float)full * std::min(1.f, m * (1 + PRECIP_BAND))));
                if (n > 0) out.push_back({ ix, iy, iz, n });
            }
}

// Draws the rain, snow and splashes of the frame into the bound target (depth tested, not written).
void drawPrecipitation(const Scene& s, const Camera& cam, PrecipFrame& f, const Matrix& view, const Matrix& proj, int vw, int vh,
                       Texture2D envTex) {
    const Downfall& d = f.d;
    Gpu::PrecipGpu& pg = gpu.pr;
    pg.drops = pg.splashes = 0;
    if (!pg.sh.id) {
        pg.sh = LoadShaderFromMemory(PRECIP_VS, PRECIP_FS);
        pg.vao = rlLoadVertexArray();
        rlEnableVertexArray(pg.vao);
        rlDisableVertexArray();
    }
    const Shader& sh = pg.sh;
    auto L = [&](const char* name) {
        auto it = pg.loc.find(name);
        if (it != pg.loc.end()) return it->second;
        return pg.loc[name] = GetShaderLocation(sh, name);
    };
    auto setF = [&](const char* n, float v) { SetShaderValue(sh, L(n), &v, SHADER_UNIFORM_FLOAT); };
    auto setI = [&](const char* n, int v) { SetShaderValue(sh, L(n), &v, SHADER_UNIFORM_INT); };
    auto set2 = [&](const char* n, float x, float y) { const float v[2] = { x, y }; SetShaderValue(sh, L(n), v, SHADER_UNIFORM_VEC2); };
    auto set3 = [&](const char* n, Vector3 v) { SetShaderValue(sh, L(n), &v, SHADER_UNIFORM_VEC3); };
    auto set4 = [&](const char* n, float x, float y, float z, float w) { const float v[4] = { x, y, z, w }; SetShaderValue(sh, L(n), v, SHADER_UNIFORM_VEC4); };

    const float aspect = (float)vw / (float)vh;
    std::vector<PrecipTile> nearT, farT, splashT;
    const bool snowOnly = d.kind == Precipitation::Snow;
    gatherPrecip(d, snowOnly ? f.aSnow : f.aRain, cam, aspect, 0, nearT);
    if (d.farLayer && d.farRadius > d.radius) gatherPrecip(d, snowOnly ? f.aSnow : f.aRain, cam, aspect, 1, farT);
    const bool splashOn = d.splashes && !snowOnly && f.map;
    if (splashOn) gatherPrecip(d, f.aRain, cam, aspect, 2, splashT);
    if (nearT.empty() && farT.empty() && splashT.empty()) return;

    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    rlEnableShader(sh.id);

    // What does not change from tile to tile.
    const Vector3 look = Vector3Subtract(cam.target, cam.position);
    const Vector3 fwd = Vector3Normalize(look);
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, safeUp(look, cam.upVector))), upv = Vector3CrossProduct(right, fwd);
    SetShaderValueMatrix(sh, L("prVP"), MatrixMultiply(view, proj));
    set3("prCam", cam.position); set3("prForward", fwd); set3("prRight", right); set3("prUp", upv);
    setI("prOrtho", cam.orthographic ? 1 : 0);
    setF("prPixel", cam.orthographic ? cam.orthoHeight / (float)vh : 2 * std::tan(cam.fov * 0.5f * DEG2RAD) / (float)vh);
    setF("prSecs", (float)f.secs);
    setI("prSeed", (int)d.seed);
    setI("prKind", d.kind == Precipitation::Rain ? 0 : d.kind == Precipitation::Snow ? 1 : 2);
    set2("prAr", f.aRain.x, f.aRain.y); set2("prAs", f.aSnow.x, f.aSnow.y);
    set4("prRainP", d.rainSpeed.x, d.rainSpeed.y, d.rainSize.x, d.rainSize.y);
    set4("prSnowP", d.snowSpeed.x, d.snowSpeed.y, d.snowSize.x, d.snowSize.y);
    setF("prFlutter", d.flutter); setF("prExposure", std::max(d.exposure, 1e-4f));
    setF("prIntensity", std::clamp(d.intensity, 0.f, 1.f)); setF("prOpacity", std::clamp(d.opacity, 0.f, 2.f));
    const float trueDensity = d.kind == Precipitation::Snow ? d.snowDensity : d.rainDensity;
    set4("prRange", d.radius, 0.35f * d.radius, d.farRadius, std::max(trueDensity / std::max(d.farDensity, 1e-3f), 1.f));
    setF("prSplashR", d.splashRadius);
    set2("prQShift", d.worldFixed ? 0.f : cam.position.x, d.worldFixed ? 0.f : cam.position.z);
    setI("prOccOn", f.map ? (d.occlusion ? 1 : 2) : 0);
    if (f.map) {
        SetShaderValueMatrix(sh, L("prOccVP"), f.occVP);
        SetShaderValueMatrix(sh, L("prOccInv"), f.occInv);
        setF("prOccRange", f.occRange);
    }
    setI("prOcc", REFR_COLOR_SLOT);
    bindUnit(REFR_COLOR_SLOT, f.map ? gpu.pr.occ.depth : 0);
    setF("prWater", s.water.enabled ? waterOf(s).level : -1e9f);
    // Lights, sky and fog as the lit shader gets them.
    const int count = std::min((int)s.lights.size(), MAX_LIGHTS);
    int type[MAX_LIGHTS] = {};
    Vector3 pos[MAX_LIGHTS] = {}, dir[MAX_LIGHTS] = {}, col[MAX_LIGHTS] = {};
    float range[MAX_LIGHTS] = {}, cin[MAX_LIGHTS] = {}, cosOut[MAX_LIGHTS] = {};
    for (int i = 0; i < count; i++) {
        const Light& l = s.lights[(size_t)i];
        type[i] = (int)l.type; pos[i] = l.position;
        const Vector3 dl = Vector3Subtract(l.target, l.position);
        dir[i] = Vector3Length(dl) > 1e-6f ? Vector3Normalize(dl) : Vector3{ 0, -1, 0 };
        col[i] = linear(l.color, l.intensity);
        range[i] = l.range;
        const float outer = std::clamp(l.outerAngle, 0.1f, 89.9f), inner = std::clamp(l.innerAngle, 0.f, outer - 0.05f);
        cin[i] = std::cos(inner * DEG2RAD); cosOut[i] = std::cos(outer * DEG2RAD);
    }
    setI("prLightCount", count);
    SetShaderValueV(sh, L("prLightType"), type, SHADER_UNIFORM_INT, MAX_LIGHTS);
    SetShaderValueV(sh, L("prLightPos"), pos, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
    SetShaderValueV(sh, L("prLightDir"), dir, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
    SetShaderValueV(sh, L("prLightColor"), col, SHADER_UNIFORM_VEC3, MAX_LIGHTS);
    SetShaderValueV(sh, L("prLightRange"), range, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
    SetShaderValueV(sh, L("prCosInner"), cin, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
    SetShaderValueV(sh, L("prCosOuter"), cosOut, SHADER_UNIFORM_FLOAT, MAX_LIGHTS);
    set3("prSky", linear(s.skyColor, s.ambient)); set3("prGround", linear(s.groundColor, s.ambient));
    const int hasEnv = envTex.id ? 1 : 0, envLin = hasEnv && isFloatFormat(envTex.format) ? 1 : 0;
    const float maxLod = hasEnv ? std::floor(std::log2((float)std::max(envTex.width, envTex.height))) : 0;
    const float intensity = s.sky.enabled ? s.sky.lighting : s.environmentIntensity;
    setI("hasEnv", hasEnv); setI("envLinear", envLin); setF("envMaxLod", maxLod); setF("envIntensity", intensity);
    setF("envRotation", s.sky.enabled ? 0.f : s.environmentRotation / 360.f);
    setF("backdropScale", s.sky.enabled ? 1.f / std::max(intensity, 1e-4f) : 1.f);
    setI("envMap", ENV_SLOT);
    set3("fogColor", linear(s.fogColor));
    setF("fogNear", s.fogNear); setF("fogFar", s.fogFar); setI("fogMode", (int)s.fogMode); setF("fogDensity", s.fogDensity);
    setF("fogHeight", s.fogHeight); setF("fogFalloff", s.fogFalloff); setI("fogSky", s.fogFromSky ? 1 : 0);

    const int locTile = L("prTile"), locCell = L("prCell"), locSplash = L("prSplash"), locFar = L("prFar");
    auto layer = [&](const std::vector<PrecipTile>& tiles, bool isFar, bool isSplash, uint32_t full, long long& counter) {
        if (tiles.empty()) return;
        const int fi = isFar ? 1 : 0, si = isSplash ? 1 : 0;
        SetShaderValue(sh, locFar, &fi, SHADER_UNIFORM_INT);
        SetShaderValue(sh, locSplash, &si, SHADER_UNIFORM_INT);
        const float cell[3] = { isFar ? Downfall::kFarTile : Downfall::kTile, isFar ? Downfall::kFarCell : Downfall::kCell, (float)full };
        SetShaderValue(sh, locCell, cell, SHADER_UNIFORM_VEC3);
        rlEnableVertexArray(pg.vao);
        for (const PrecipTile& t : tiles) {
            const int key[3] = { t.ix, t.iy, t.iz };
            SetShaderValue(sh, locTile, key, SHADER_UNIFORM_IVEC3);
            rlDrawVertexArrayInstanced(0, isSplash ? 36 : 6, t.n);     // a splash: the ring and five droplets
            counter += t.n;
        }
        rlDisableVertexArray();
    };
    layer(farT, true, false, d.farDropsPerTile(), pg.drops);
    layer(nearT, false, false, d.dropsPerTile(), pg.drops);
    layer(splashT, false, true, d.dropsPerTile(), pg.splashes);

    bindUnit(REFR_COLOR_SLOT, 0);
    rlDisableShader();
    restoreBlend();
    rlEnableDepthMask();
}

// ---- the ground (Scene::ground): the wet and snow state of this frame, and the uniforms that carry it
struct WetFrame {
    bool on = false;                     // there is wetness or snow to draw
    bool ssr = false;                    // and the scene is to be reflected in it (a second draw after the opaque pass)
    Weather::GroundState g;
};

WetFrame wetFrame(const Scene& s) {
    WetFrame w;
    const float fps = s.weather.enabled ? s.weather.fps : s.precipitation.fps;
    w.g = groundOf(s, frameTime(gpu.frame) / std::max(fps, 1.f));
    w.on = w.g.wetness > 0.001f || w.g.snowDepth > 0.0005f;
    w.ssr = w.on && s.ground.reflections && w.g.wetness > 0.05f && w.g.wetness > w.g.snowDepth / Weather::kSnowFull;
    return w;
}

// Sets the ground's uniforms of the lit program for this frame (the rain's rings too, which water uses without a
// wet ground) and binds the rain's depth map for the shelter test. Call before any pass that draws lit items.
void setWetUniforms(const Scene& s, const WetFrame& w, const PrecipFrame& pf) {
    const Shader& sh = gpu.lit;
    auto I = [&](const char* n, int v) { SetShaderValue(sh, GetShaderLocation(sh, n), &v, SHADER_UNIFORM_INT); };
    auto F = [&](const char* n, float v) { SetShaderValue(sh, GetShaderLocation(sh, n), &v, SHADER_UNIFORM_FLOAT); };
    const Ground& g = s.ground;
    I("ssrPass", 0);
    I("wetOn", w.on ? 1 : 0);
    F("wetLevel", w.g.wetness);
    F("snowDepth", w.g.snowDepth);
    F("wetPuddles", std::clamp(g.puddles, 0.f, 2.f));
    F("wetPuddleSize", std::max(g.puddleSize, 0.1f));
    I("wetSeed", (int)g.seed);
    I("wetGrass", g.snowOnGrass ? 1 : 0);
    I("wetDebug", g.debug);
    const bool shelter = w.on && pf.map;
    I("wetOccOn", shelter ? 1 : 0);
    if (shelter) {
        SetShaderValueMatrix(sh, GetShaderLocation(sh, "wetOccVP"), pf.occVP);
        F("wetOccRange", pf.occRange);
    }
    bindUnit(WET_OCC_SLOT, shelter ? gpu.pr.occ.depth : 0);
    // The rings: rain and the rain share of sleet, as many a square metre as the rate and the intensity say.
    const float share = pf.d.kind == Precipitation::Rain ? 1.f : pf.d.kind == Precipitation::Sleet ? 0.6f : 0.f;
    const float rate = pf.on ? std::clamp(g.rippleRate, 0.f, Ground::maxRate()) * std::clamp(pf.d.intensity, 0.f, 1.f) * share : 0.f;
    F("rippleRate", rate);
    F("rippleTime", (float)pf.secs);
    I("rippleSeed", (int)(pf.d.seed * 2654435761u + g.seed));
}

// The scene's reflection in flat wet ground and puddles: a copy of the opaque picture, then the plain opaque meshes
// drawn again over it (the depth test keeps what is the nearest surface), each fragment on flat wet ground marching
// its reflected ray through the copy and blending in what it finds by the film's Fresnel. Rays that find nothing leave the
// sky reflection the first pass already added.
void drawWetReflections(const std::vector<Item>& items, const DepthTarget& scene, const Vector4& map) {
    const Matrix vpm = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    const DepthTarget& copy = copyScene(scene);
    SetShaderValueMatrix(gpu.lit, gpu.W.reflVP, vpm);
    SetShaderValueMatrix(gpu.lit, gpu.W.refrInvVP, MatrixInvert(vpm));
    SetShaderValue(gpu.lit, gpu.W.refrMap, &map, SHADER_UNIFORM_VEC4);
    bindUnit(REFR_COLOR_SLOT, copy.color);
    bindUnit(REFR_DEPTH_SLOT, copy.depth);
    const int on = 1, off = 0;
    SetShaderValue(gpu.lit, GetShaderLocation(gpu.lit, "ssrPass"), &on, SHADER_UNIFORM_INT);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    for (const Item& it : items) {
        const g3d::Material& m = it.obj->material;
        if (it.inst || it.tiles || it.particle || opacityOf(it) < 1.f || !m.weathering || m.unlit || m.shading != Shading::Standard) continue;
        drawItem(it, Pass::Lit);
    }
    rlDrawRenderBatchActive();
    restoreBlend();
    rlEnableDepthMask();
    SetShaderValue(gpu.lit, GetShaderLocation(gpu.lit, "ssrPass"), &off, SHADER_UNIFORM_INT);
    bindUnit(REFR_COLOR_SLOT, 0);
    bindUnit(REFR_DEPTH_SLOT, 0);
}

void renderBody(Scene& s, Camera& camIn, Rectangle vp, const RenderTexture& target, float scale, int t, const Cull& cull) {
    ensureGpu();
    Camera cam = camIn;
    if (s.water.enabled && s.water.spectrum.enabled && !cam.orthographic) {
        // The spectrum sea reaches the horizon: the far plane goes out to it (this frame's copy of the camera only).
        const float above = std::max(cam.position.y - s.water.level, 0.f);
        cam.farPlane = std::max(cam.farPlane, 1.5f * seaHorizon(above) + 500.f);
    }
    std::vector<Item> items;
    std::vector<Emitting> emitters;
    const double ft = frameTime(t);              // particles follow motion-blur subframes
    gpu.frame = t;
    std::deque<std::vector<Matrix>> instanceStore;           // what the instanced items point into
    std::deque<std::vector<FieldTile>> fieldStore;            // and the fields' tiles
    gpu.viewCam = cam.position;                              // fields thin out by distance from it, in every pass
    gpu.fieldBlades = 0;
    gpu.fieldShadowBlades = 0;
    gpu.instanceBodies = gpu.instanceTriangles = 0;
    pruneFields();
    collect(s.objects, MatrixIdentity(), items, emitters, ft, cull, instanceStore, fieldStore);
    // The water: seen from above with water in the picture, it may need the scene mirrored (what the mirrored
    // camera sees, so collected again against its frustum) and a copy of the opaque scene to look through.
    const int vw0 = std::max(1, (int)std::lround(vp.width * scale)), vh0 = std::max(1, (int)std::lround(vp.height * scale));
    const WaterView wview = waterView(s, cam, (float)vw0 / (float)vh0, t);
    const float aspect0 = (float)vw0 / (float)vh0;
    const WaterView rview = reflectionView(s, cam, aspect0, t);      // the one level that is mirrored: the sea's, else a body's
    const bool reflect = cull.mirror && rview.visible;
    const bool seaReflect = reflect && wview.visible && s.water.reflections != Water::Reflections::Off;
    static int bodyStamp = 0;
    const std::vector<BodyDraw> bodies = collectBodies(s, cam, aspect0, ++bodyStamp);
    const bool refractOn = wview.visible && wview.water.refraction;
    std::vector<Item> mirrorItems;
    std::deque<std::vector<Matrix>> mirrorInstances;
    std::deque<std::vector<FieldTile>> mirrorFields;
    if (reflect) {
        std::vector<Emitting> unused;
        Cull km = cull;
        km.cam = cull.mirror;
        km.mirror = nullptr;
        collect(s.objects, MatrixIdentity(), mirrorItems, unused, ft, km, mirrorInstances, mirrorFields);
        mirrorItems.erase(std::remove_if(mirrorItems.begin(), mirrorItems.end(), [](const Item& it) { return it.obj->material.shadowOnly; }), mirrorItems.end());
    }
    const Texture2D envTex = s.sky.enabled ? renderSky(s, ft, target.id, target.texture.width, target.texture.height)
                                           : environmentTexture(s);
    gpu.cl.shadowOn = false;
    const bool cloudsOn = cloudsActive(s, cam);
    CloudFrame cloudNow;
    if (cloudsOn) {
        cloudNow = cloudFrame(s, ft);
        renderCloudShadow(s, cloudNow, cam, target.id, target.texture.width, target.texture.height);
    }

    // Background, in 2D before the viewport changes.
    if (s.background.a > 0) {
        DrawRectangleRec(vp, s.background);
        rlDrawRenderBatchActive();
    }

    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    const Shadows shadows = renderShadows(s, items);
    items.erase(std::remove_if(items.begin(), items.end(), [](const Item& it) { return it.obj->material.shadowOnly; }), items.end());
    gpu.pr.drops = gpu.pr.splashes = 0;
    PrecipFrame precip = precipFrame(s);
    renderPrecipOcclusion(cam, precip, items);
    const WetFrame wet = wetFrame(s);
    setWetUniforms(s, wet, precip);
    if (reflect) renderReflection(s, cam, *cull.mirror, rview, mirrorItems, shadows, envTex, scale, cloudsOn ? &cloudNow : nullptr, vw0, vh0);

    // Main pass, into the target or (for depth of field) a target with a depth texture.
    const int fbW = target.texture.width, fbH = target.texture.height;
    const bool dof = cam.dof.aperture > 0 && cam.dof.maxBlur > 0;    const DepthTarget* dofTarget = nullptr;
    // Lightning and the rainbow are drawn over the finished picture, against the opaque depth.
    const std::vector<LightningActive> bolts = s.lightning.enabled && !cam.orthographic ? lightningAt(s, lightningSeconds(s, ft)) : std::vector<LightningActive>{};
    RainbowFrame bow;
    if (!cam.orthographic && s.weather.enabled) {
        bow = rainbowOf(s, ft);
        const float ty = std::tan(cam.fov * 0.5f * DEG2RAD), tx = ty * (float)vw0 / (float)vh0;
        const Vector3 fwd0 = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
        const float off = std::acos(std::clamp(Vector3DotProduct(fwd0, bow.antiSolar), -1.f, 1.f)) * RAD2DEG;
        if (off > std::atan(std::sqrt(tx * tx + ty * ty)) * RAD2DEG + 56.f) bow.strength = 0;     // the bow's whole ring lies behind the view
    }
    const bool volOn = volumetricActive(s, cam), aerialOn = aerialActive(s, cam);       // light in the air: both read the opaque depth
    if (dof || cloudsOn || refractOn || wet.ssr || !bodies.empty() || !bolts.empty() || bow.strength > 0.002f || volOn || aerialOn) {         // clouds and the water need the opaque depth as a texture: draw into the depth target
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

    FogHold fogHold(s, volOn);                         // the march fogs the picture (drawAir): the surfaces carry none
    setSceneUniforms(s, cam, shadows, envTex, scale);
    if (refractOn && s.water.enabled) { const int skipTint = 2; SetShaderValue(gpu.lit, gpu.W.refrMode, &skipTint, SHADER_UNIFORM_INT); }    // what is under the water is absorbed once, by the surface that looks through it
    bindUnit(SHADOW_SLOT, shadows.tiles.empty() ? 0 : gpu.atlasTex);
    bindUnit(ENV_SLOT, envTex.id);
    if (gpu.cl.shadowOn) bindUnit(CLOUD_SLOT, gpu.cl.shadowRT.texture.id);
    if (envTex.id && (s.sky.enabled ? s.sky.show : s.showEnvironment)) drawSkybox(cam);
    std::vector<Item> clear;
    for (Item& it : items) {
        if (opacityOf(it) < 1.f) {
            Vector3 p = Vector3Transform({ 0, 0, 0 }, MatrixMultiply(it.world, view));
            it.viewDepth = -p.z;
            clear.push_back(it);
        } else drawItem(it, Pass::Lit);
    }
    if (wet.ssr && dofTarget)
        drawWetReflections(items, *dofTarget, { (float)vx / fbW, (float)(fbH - vy - vh) / fbH, (float)vw / fbW, (float)vh / fbH });
    // Stable: at equal depth keep scene order, so coplanar layers do not swap between frames.
    const bool spectrumSea = s.water.enabled && s.water.spectrum.enabled;
    if (s.water.enabled) clear.push_back({ spectrumSea ? &oceanObject() : &waterObject(), MatrixIdentity(), std::numeric_limits<float>::max() });
    std::map<const Object3D*, size_t> bodyOf;                     // the water bodies, drawn among the transparent items
    for (size_t i = 0; i < bodies.size(); i++) {
        clear.push_back({ bodies[i].obj, MatrixIdentity(), bodies[i].depth });
        bodyOf[bodies[i].obj] = i;
    }
    std::stable_sort(clear.begin(), clear.end(), [](const Item& a, const Item& b) { return a.viewDepth > b.viewDepth; });
    // The opaque scene, copied once for everything that looks through it: the sea's refraction, the bodies' shore.
    const DepthTarget* sceneCopy = (refractOn || !bodies.empty()) && dofTarget ? &copyScene(*dofTarget) : nullptr;
    const Vector4 sceneMap{ (float)vx / fbW, (float)(fbH - vy - vh) / fbH, (float)vw / fbW, (float)vh / fbH };
    rlDisableDepthMask();
    for (const Item& it : clear) {
        if (const auto bi = bodyOf.find(it.obj); bi != bodyOf.end() && sceneCopy) {
            const BodyDraw& bd = bodies[bi->second];
            setBodyUniforms(s, cam, bd);
            const int on = 1, refr = bd.look.refraction ? 1 : 0;
            const Matrix vpm = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
            SetShaderValueMatrix(gpu.lit, gpu.W.reflVP, vpm);
            SetShaderValueMatrix(gpu.lit, gpu.W.refrInvVP, MatrixInvert(vpm));
            SetShaderValue(gpu.lit, gpu.W.refrMap, &sceneMap, SHADER_UNIFORM_VEC4);
            SetShaderValue(gpu.lit, gpu.W.refrMode, &refr, SHADER_UNIFORM_INT);
            bindUnit(REFR_COLOR_SLOT, sceneCopy->color);
            bindUnit(REFR_DEPTH_SLOT, sceneCopy->depth);
            if (reflect && bd.look.reflections != Water::Reflections::Off && std::fabs(bd.b->level - rview.level) < 1e-3f) {
                const DepthTarget& rt = gpu.reflTarget;
                const Vector2 size{ (float)rt.w, (float)rt.h };
                const float distort = reflectionBend(bd.look);
                SetShaderValue(gpu.lit, gpu.W.reflMode, &on, SHADER_UNIFORM_INT);
                SetShaderValue(gpu.lit, gpu.W.reflSize, &size, SHADER_UNIFORM_VEC2);
                SetShaderValue(gpu.lit, gpu.W.reflDistort, &distort, SHADER_UNIFORM_FLOAT);
                { const float full = std::max(bd.look.reflectionDistort, 0.f); SetShaderValue(gpu.lit, GetShaderLocation(gpu.lit, "reflDistortFull"), &full, SHADER_UNIFORM_FLOAT); }
                bindUnit(REFL_SLOT, rt.color);
            }
            drawItem(it, Pass::Lit);
            for (int slot : { REFL_SLOT, REFR_COLOR_SLOT, REFR_DEPTH_SLOT }) bindUnit(slot, 0);
            setWaterUniforms(s, cam);                                 // the sea's uniforms again, for what is drawn after
            continue;
        }
        const bool surface = it.obj == gpu.waterObj.get() || it.obj == gpu.oceanObj.get();       // the sea's surface is its own mode of the lit shader
        if (surface) {
            const int on = 1;
            SetShaderValue(gpu.lit, gpu.W.mode, &on, SHADER_UNIFORM_INT);
            if (spectrumSea) bindOceanTextures(true);
            if (seaReflect || refractOn) {
                const Matrix vpm = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
                SetShaderValueMatrix(gpu.lit, gpu.W.reflVP, vpm);
                if (seaReflect) {
                    const DepthTarget& rt = gpu.reflTarget;
                    const Vector2 size{ (float)rt.w, (float)rt.h };
                    // How far the waves bend the reflection follows how tall the sea is (reflectionBend).
                    const float distort = reflectionBend(wview.water);
                    SetShaderValue(gpu.lit, gpu.W.reflMode, &on, SHADER_UNIFORM_INT);
                    SetShaderValue(gpu.lit, gpu.W.reflSize, &size, SHADER_UNIFORM_VEC2);
                    SetShaderValue(gpu.lit, gpu.W.reflDistort, &distort, SHADER_UNIFORM_FLOAT);
                    { const float full = std::max(wview.water.reflectionDistort, 0.f); SetShaderValue(gpu.lit, GetShaderLocation(gpu.lit, "reflDistortFull"), &full, SHADER_UNIFORM_FLOAT); }
                    bindUnit(REFL_SLOT, rt.color);
                }
                if (refractOn) {
                    SetShaderValue(gpu.lit, gpu.W.refrMode, &on, SHADER_UNIFORM_INT);
                    SetShaderValue(gpu.lit, gpu.W.refrMap, &sceneMap, SHADER_UNIFORM_VEC4);
                    SetShaderValueMatrix(gpu.lit, gpu.W.refrInvVP, MatrixInvert(vpm));
                    bindUnit(REFR_COLOR_SLOT, sceneCopy->color);
                    bindUnit(REFR_DEPTH_SLOT, sceneCopy->depth);
                }
            }
        }
        drawItem(it, Pass::Lit);
        if (surface) {
            const int off = 0;
            SetShaderValue(gpu.lit, gpu.W.mode, &off, SHADER_UNIFORM_INT);
            SetShaderValue(gpu.lit, gpu.W.reflMode, &off, SHADER_UNIFORM_INT);
            SetShaderValue(gpu.lit, gpu.W.refrMode, &off, SHADER_UNIFORM_INT);
            for (int slot : { REFL_SLOT, REFR_COLOR_SLOT, REFR_DEPTH_SLOT }) bindUnit(slot, 0);
            if (spectrumSea) bindOceanTextures(false);
        }
    }
    rlEnableDepthMask();
    if (!emitters.empty()) {
        drawSplats(s, cam, emitters, rlGetMatrixModelview(), rlGetMatrixProjection(), vw, vh);
        drawParticles(s, cam, emitters, view, ft, cull.compH);
    }
    if (precip.on) drawPrecipitation(s, cam, precip, rlGetMatrixModelview(), rlGetMatrixProjection(), vw, vh, envTex);

    bindUnit(SHADOW_SLOT, 0);
    bindUnit(ENV_SLOT, 0);
    bindUnit(CLOUD_SLOT, 0);
    bindUnit(WET_OCC_SLOT, 0);
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
    fogHold.release();
    if (dofTarget && (volOn || aerialOn)) drawAir(s, cam, shadows, envTex, *dofTarget, fbW, fbH, vx, fbH - vy - vh, vw, vh, volOn, aerialOn);
    if (cloudsOn) drawClouds(s, cloudNow, cam, *dofTarget, fbW, fbH, vx, fbH - vy - vh, vw, vh);
    if (dofTarget && bow.strength > 0.002f) drawRainbow(s, bow, cam, *dofTarget, fbW, fbH, vx, fbH - vy - vh, vw, vh);
    if (dofTarget && !bolts.empty()) drawBolts(s, cam, bolts, *dofTarget, fbW, fbH, vx, fbH - vy - vh, vw, vh);
    if (lt && styles.table.size() > 1) {
        rlEnableFramebuffer(dofTarget ? dofTarget->fbo : target.id);
        compositeLines(s, cam, *lt, styles, vh, scale, t);
    }
    if (dofTarget) {
        rlEnableFramebuffer(target.id);
        rlScalef(scale, scale, 1.f);                    // the 2D state drawOffscreen bound
        if (dof) compositeDof(*dofTarget, cam, scale, { (float)(fbH - vy - vh) / fbH, (float)vh / fbH });
        else {                                          // clouds only: the depth target is copied over (premultiplied)
            rlSetBlendFactorsSeparate(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
            const Texture2D color{ dofTarget->color, dofTarget->w, dofTarget->h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
            DrawTexturePro(color, { 0, 0, (float)dofTarget->w, -(float)dofTarget->h }, { 0, 0, dofTarget->w / scale, dofTarget->h / scale }, { 0, 0 }, 0, WHITE);
            EndBlendMode();
            rlDrawRenderBatchActive();
        }
    }
}

} // namespace

// ------------------------------------------------------------------ skies
namespace {
Color mixColor(Color a, Color b, float t) {
    auto m = [t](unsigned char x, unsigned char y) { return (unsigned char)std::lround(x + (y - x) * t); };
    return { m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a) };
}
// Towards the sun at an hour: up from the east (+x) at 6, over the south (-z), down in the
// west at 18; below the horizon at night.
Vector3 sunAt(float hour) {
    const float a = (hour - 6) / 12 * PI;
    return Vector3Normalize({ std::cos(a), std::sin(a) * 0.85f, -0.45f - 0.2f * std::sin(a) });
}
} // namespace

// ------------------------------------------------------------------ water
namespace {
// The shader's sum, in the same float arithmetic: height and slope at p.
float waveSum(const PackedWaves& p, float x, float z, double seconds, Vector2* slope) {
    float h = 0, gx = 0, gz = 0;
    const float t = (float)seconds;
    for (int i = 0; i < p.count; i++) {
        const float s = p.a[i].z * (p.a[i].x * x + p.a[i].y * z) - p.b[i].x * t + p.b[i].y;
        const float u = std::max(0.5f + 0.5f * std::sin(s), 1e-4f), sh = p.b[i].z;
        h += p.a[i].w * (2 * std::pow(u, sh) - 1);
        const float d = p.a[i].w * sh * std::pow(u, sh - 1) * std::cos(s) * p.a[i].z;
        gx += d * p.a[i].x;
        gz += d * p.a[i].y;
    }
    if (slope) *slope = { gx, gz };
    return h;
}
} // namespace

float Water::height(float x, float z, double seconds) const {
    if (spectrum.enabled) {
        // The same grids the mesh reads, at full detail: the cascades with a patch of 8 m or more, and the curvature.
        const auto f = ocean::frameAt(*this, seconds);
        const float drop = spectrum.curvature ? (x * x + z * z) * 0.5f / kPlanetRadius : 0.f;
        return level + ocean::heightAt(*f, x, z) - drop;
    }
    return level + waveSum(packWaves(*this), x, z, seconds, nullptr);
}
Vector2 Water::slope(float x, float z, double seconds) const {
    Vector2 g;
    if (spectrum.enabled) {
        const auto f = ocean::frameAt(*this, seconds);
        ocean::heightAt(*f, x, z, &g);
        if (spectrum.curvature) { g.x -= x / kPlanetRadius; g.y -= z / kPlanetRadius; }
        return g;
    }
    waveSum(packWaves(*this), x, z, seconds, &g);
    return g;
}
Water Water::calm() {
    Water w;
    w.enabled = true;
    // Directions within 40 degrees of the heading and lengths that are no simple ratio of each other (a grid of
    // regular sinusoids reads as a lattice); the amplitudes, so Hs, and the longest length are what they were.
    w.waves = { { 20, 7.0f, 0.045f, 1, 1 }, { -3, 3.4f, 0.03f, 1, 1 }, { 51, 1.93f, 0.022f, 1, 1 },
                { -18, 1.13f, 0.014f, 1, 1 }, { 34, 0.69f, 0.009f, 1, 1 }, { 57, 0.46f, 0.0065f, 1, 1 },
                { 11, 0.33f, 0.005f, 1, 1 }, { -14, 0.23f, 0.0035f, 1, 1 } };
    w.foam = 0.2f;
    w.fetch = 2000;
    return w;
}
Water Water::choppy() {
    Water w;
    w.enabled = true;
    w.waves = { { 15, 14.0f, 0.16f, 1.6f, 1 }, { -13, 6.3f, 0.1f, 1.8f, 1 }, { 51, 2.9f, 0.06f, 1.5f, 1 },
                { -18, 1.55f, 0.035f, 1, 1 }, { 32, 0.83f, 0.02f, 1, 1 }, { 53, 0.44f, 0.01f, 1, 1 } };
    w.fetch = 10000;
    return w;
}
Water Water::storm() {
    Water w;
    w.enabled = true;
    w.waves = { { 10, 34.0f, 0.7f, 2.4f, 1 }, { -20, 15.5f, 0.42f, 2.6f, 1 }, { 45, 8.2f, 0.24f, 2.2f, 1 },
                { -27, 4.0f, 0.12f, 1.8f, 1 }, { 30, 2.1f, 0.06f, 1.4f, 1 }, { -2, 1.05f, 0.03f, 1, 1 } };
    w.foam = 1;
    w.fetch = 200000;
    w.deep = { 10, 52, 66, 255 };
    return w;
}

Sky Sky::day() {
    Sky k;
    k.enabled = true;
    k.sunDirection = sunAt(12.5f);
    k.clouds = 0.25f;
    return k;
}
Sky Sky::dawn() {
    Sky k;
    k.enabled = true;
    k.zenith = { 54, 78, 140, 255 };
    k.horizon = { 236, 176, 150, 255 };
    k.ground = { 52, 48, 56, 255 };
    k.curve = 0.6f;
    k.sunDirection = sunAt(6.6f);
    k.sunColor = { 255, 214, 170, 255 };
    k.haze = { 255, 150, 110, 255 };
    k.hazeAmount = 0.7f;
    k.sunGlow = 0.8f;
    k.stars = 0.15f;
    k.clouds = 0.2f;
    k.cloudColor = { 255, 206, 186, 255 };
    k.cloudShade = { 112, 104, 138, 255 };
    return k;
}
Sky Sky::dusk() {
    Sky k = dawn();
    k.zenith = { 40, 52, 112, 255 };
    k.horizon = { 246, 150, 96, 255 };
    k.haze = { 255, 112, 64, 255 };
    k.sunDirection = sunAt(17.6f);
    k.sunColor = { 255, 186, 120, 255 };
    k.cloudColor = { 255, 170, 130, 255 };
    k.cloudShade = { 96, 80, 120, 255 };
    k.stars = 0.25f;
    return k;
}
Sky Sky::night() {
    Sky k;
    k.enabled = true;
    k.zenith = { 6, 10, 26, 255 };
    k.horizon = { 26, 36, 64, 255 };
    k.ground = { 10, 10, 14, 255 };
    k.curve = 0.5f;
    k.sunDirection = sunAt(0);
    k.sunSize = 0;
    k.sunGlow = 0;
    k.hazeAmount = 0;
    k.stars = 1;
    k.clouds = 0.15f;
    k.cloudColor = { 60, 66, 88, 255 };
    k.cloudShade = { 18, 22, 34, 255 };
    k.lighting = 0.6f;
    return k;
}

Sky Sky::mix(const Sky& a, const Sky& b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    Sky k = t < 0.5f ? a : b;
    auto f = [t](float x, float y) { return x + (y - x) * t; };
    k.zenith = mixColor(a.zenith, b.zenith, t);
    k.horizon = mixColor(a.horizon, b.horizon, t);
    k.ground = mixColor(a.ground, b.ground, t);
    k.sunColor = mixColor(a.sunColor, b.sunColor, t);
    k.haze = mixColor(a.haze, b.haze, t);
    k.cloudColor = mixColor(a.cloudColor, b.cloudColor, t);
    k.cloudShade = mixColor(a.cloudShade, b.cloudShade, t);
    k.curve = f(a.curve, b.curve);
    k.sunSize = f(a.sunSize, b.sunSize);
    k.sunGlow = f(a.sunGlow, b.sunGlow);
    k.hazeAmount = f(a.hazeAmount, b.hazeAmount);
    k.stars = f(a.stars, b.stars);
    k.clouds = f(a.clouds, b.clouds);
    k.cloudScale = f(a.cloudScale, b.cloudScale);
    k.lighting = f(a.lighting, b.lighting);
    const Vector3 d = Vector3Lerp(Vector3Normalize(a.sunDirection), Vector3Normalize(b.sunDirection), t);
    k.sunDirection = Vector3Length(d) > 1e-6f ? d : b.sunDirection;
    return k;
}

Sky Sky::at(float hour) {
    hour = std::fmod(std::fmod(hour, 24.f) + 24.f, 24.f);
    // Keyframes through the day; the sun follows the hour throughout.
    struct Key { float hour; Sky sky; };
    static const Key keys[] = { { 0, night() }, { 5.2f, night() }, { 6.5f, dawn() }, { 9, day() }, { 15.5f, day() },
                                { 17.8f, dusk() }, { 19.2f, night() }, { 24, night() } };
    size_t i = 1;
    while (i + 1 < std::size(keys) && hour > keys[i].hour) i++;
    const Key& a = keys[i - 1];
    const Key& b = keys[i];
    Sky k = mix(a.sky, b.sky, (hour - a.hour) / std::max(1e-3f, b.hour - a.hour));
    k.sunDirection = sunAt(hour);
    const float up = sunAt(hour).y;
    k.sunSize = up > -0.05f ? std::max(k.sunSize, 1.2f) : 0.f;
    return k;
}

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
    if (!pending_.empty()) {
        auto edits = std::move(pending_);
        pending_.clear();
        unsigned buffers = 0;
        for (auto& [mask, edit] : edits) {
            edit(mesh_);
            buffers |= mask;
        }
        // Geometry edits can change any fixed-topology CPU attribute, not only position
        // and normals. Keep each existing GPU buffer in step with the mesh returned here.
        // The script Geometry3D handle uses this path for UV, tangent, color and index edits.
        if (mesh_.vboId) {
            const int vertices = mesh_.vertexCount;
            auto update = [&](int slot, const void* data, int bytes) {
                if (data && bytes > 0 && mesh_.vboId[slot] != 0)
                    UpdateMeshBuffer(mesh_, slot, data, bytes, 0);
            };
            if (buffers & PositionBuffer) update(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, mesh_.vertices, vertices * 3 * (int)sizeof(float));
            if (buffers & TexcoordBuffer) update(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, mesh_.texcoords, vertices * 2 * (int)sizeof(float));
            if (buffers & NormalBuffer) update(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL, mesh_.normals, vertices * 3 * (int)sizeof(float));
            if (buffers & ColorBuffer) update(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, mesh_.colors, vertices * 4 * (int)sizeof(unsigned char));
            if (buffers & TangentBuffer) update(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, mesh_.tangents, vertices * 4 * (int)sizeof(float));
            if (buffers & Texcoord2Buffer) update(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2, mesh_.texcoords2, vertices * 2 * (int)sizeof(float));
            if ((buffers & IndexBuffer) && mesh_.indices && mesh_.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES] != 0)
                rlUpdateVertexBufferElements(mesh_.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES], mesh_.indices,
                                             mesh_.triangleCount * 3 * (int)sizeof(unsigned short), 0);
        }
    }
    return mesh_;
}

void Geometry::update(std::function<void(Mesh&)> edit) {
    update(std::move(edit), PositionBuffer | NormalBuffer);
}

void Geometry::update(std::function<void(Mesh&)> edit, unsigned buffers) {
    // One pending edit per buffer set: a geometry updated every frame while it is not drawn
    // (culled, hidden) keeps only the latest edit instead of a growing chain of them.
    for (auto& [mask, pending] : pending_)
        if (mask == buffers) {
            pending = std::move(edit);
            return;
        }
    pending_.emplace_back(buffers, std::move(edit));
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

// A plain torus in the xy plane. raylib's GenMeshTorus clamps tube / radius to 0.1..1, so
// a thin ring (a dome's rib: 80 m round, 0.5 m thick) came out with an 8 m tube.
// Built like torusKnot (same frame and winding) on a circle.
Mesh ringTorus(float radius, float tube, int radSeg, int sides) {
    radSeg = std::max(3, radSeg);
    sides = std::max(3, sides);
    while ((radSeg + 1) * (sides + 1) > 65535) radSeg /= 2;              // 16-bit indices
    Mesh m{};
    m.vertexCount = (radSeg + 1) * (sides + 1);
    m.triangleCount = radSeg * sides * 2;
    m.vertices = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
    m.normals = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
    m.texcoords = (float*)MemAlloc((unsigned)(m.vertexCount * 2 * sizeof(float)));
    m.indices = (unsigned short*)MemAlloc((unsigned)(m.triangleCount * 3 * sizeof(unsigned short)));
    int v = 0;
    for (int j = 0; j <= radSeg; j++) {
        const float u = (float)j / radSeg * 2 * PI, cu = std::cos(u), su = std::sin(u);
        for (int i = 0; i <= sides; i++, v++) {
            const float a = (float)i / sides * 2 * PI, cx = -std::cos(a), cy = std::sin(a);
            const Vector3 nrm{ cx * cu, cx * su, -cy };                   // n = outward, b = -z
            m.vertices[v * 3] = radius * cu + tube * nrm.x;
            m.vertices[v * 3 + 1] = radius * su + tube * nrm.y;
            m.vertices[v * 3 + 2] = tube * nrm.z;
            m.normals[v * 3] = nrm.x; m.normals[v * 3 + 1] = nrm.y; m.normals[v * 3 + 2] = nrm.z;
            m.texcoords[v * 2] = (float)j / radSeg; m.texcoords[v * 2 + 1] = (float)i / sides;
        }
    }
    int k = 0;
    for (int j = 1; j <= radSeg; j++)
        for (int i = 1; i <= sides; i++) {
            const unsigned short a = (unsigned short)((sides + 1) * (j - 1) + (i - 1)), b = (unsigned short)((sides + 1) * j + (i - 1));
            const unsigned short c = (unsigned short)((sides + 1) * j + i), d = (unsigned short)((sides + 1) * (j - 1) + i);
            const unsigned short tri[6] = { a, b, d, b, c, d };
            for (unsigned short x : tri) m.indices[k++] = x;
        }
    UploadMesh(&m, false);
    return m;
}
}

namespace {
Mesh slabMesh(float w, float h, float d, float radius, int seg) {
    radius = std::clamp(radius, 0.f, 0.5f * std::min(w, h));
    seg = radius > 0 ? std::max(1, seg) : 0;
    struct P { float x, y, nx, ny; };
    std::vector<P> ring;                                      // the outline, anticlockwise seen from +z
    const float cx[4] = { w / 2 - radius, -w / 2 + radius, -w / 2 + radius, w / 2 - radius };
    const float cy[4] = { h / 2 - radius, h / 2 - radius, -h / 2 + radius, -h / 2 + radius };
    for (int c = 0; c < 4; c++)
        for (int i = 0; i <= seg; i++) {
            const float a = (c + (seg ? (float)i / seg : 0.5f)) * PI / 2;
            const float nx = std::cos(a), ny = std::sin(a);
            ring.push_back({ cx[c] + radius * nx, cy[c] + radius * ny, seg ? nx : (c == 0 || c == 3 ? 1.f : -1.f) * 0.7071f,
                             seg ? ny : (c < 2 ? 1.f : -1.f) * 0.7071f });
        }
    const int n = (int)ring.size();
    Mesh m{};
    m.vertexCount = 2 * (n + 1) + 4 * n;
    m.triangleCount = 2 * n + 2 * n;
    m.vertices = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
    m.normals = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
    m.texcoords = (float*)MemAlloc((unsigned)(m.vertexCount * 2 * sizeof(float)));
    m.indices = (unsigned short*)MemAlloc((unsigned)(m.triangleCount * 3 * sizeof(unsigned short)));
    int v = 0, k = 0;
    auto vert = [&](float x, float y, float z, float nx, float ny, float nz) {
        m.vertices[v * 3] = x; m.vertices[v * 3 + 1] = y; m.vertices[v * 3 + 2] = z;
        m.normals[v * 3] = nx; m.normals[v * 3 + 1] = ny; m.normals[v * 3 + 2] = nz;
        m.texcoords[v * 2] = x / w + 0.5f; m.texcoords[v * 2 + 1] = 0.5f - y / h;
        return (unsigned short)v++;
    };
    auto tri = [&](unsigned short a, unsigned short b, unsigned short c) { m.indices[k++] = a; m.indices[k++] = b; m.indices[k++] = c; };
    for (int side = 0; side < 2; side++) {                    // the front (+z) and back faces, as fans
        const float z = side ? -d / 2 : d / 2, nz = side ? -1.f : 1.f;
        const unsigned short c = vert(0, 0, z, 0, 0, nz);
        for (int i = 0; i < n; i++) vert(ring[i].x, ring[i].y, z, 0, 0, nz);
        for (int i = 0; i < n; i++) {
            const unsigned short a = (unsigned short)(c + 1 + i), b = (unsigned short)(c + 1 + (i + 1) % n);
            if (side) tri(c, b, a); else tri(c, a, b);
        }
    }
    for (int i = 0; i < n; i++) {                             // the rim, its own vertices for smooth or hard normals
        const P& p = ring[i]; const P& q = ring[(i + 1) % n];
        const bool flat = !seg || std::fabs(p.nx - q.nx) + std::fabs(p.ny - q.ny) < 1e-4f;
        const float nx = flat ? (p.nx + q.nx) : p.nx, ny = flat ? (p.ny + q.ny) : p.ny;
        const float l = std::sqrt(nx * nx + ny * ny), mx = flat ? q.nx + p.nx : q.nx, my = flat ? q.ny + p.ny : q.ny;
        const float lm = std::sqrt(mx * mx + my * my);
        const unsigned short f0 = vert(p.x, p.y, d / 2, nx / l, ny / l, 0), b0 = vert(p.x, p.y, -d / 2, nx / l, ny / l, 0);
        const unsigned short f1 = vert(q.x, q.y, d / 2, mx / lm, my / lm, 0), b1 = vert(q.x, q.y, -d / 2, mx / lm, my / lm, 0);
        tri(f0, b0, f1);
        tri(f1, b0, b1);
    }
    UploadMesh(&m, false);
    return m;
}
}
std::shared_ptr<Geometry> Geometry::roundedSlab(float w, float h, float d, float radius, int segments) {
    return std::make_shared<Geometry>([=] { return slabMesh(w, h, d, radius, segments); });
}
std::shared_ptr<Geometry> Geometry::cylinder(float r, float h, int slices) {
    return std::make_shared<Geometry>([=] { return centeredY(GenMeshCylinder(r, h, slices)); });
}
std::shared_ptr<Geometry> Geometry::cone(float r, float h, int slices) {
    return std::make_shared<Geometry>([=] { return centeredY(GenMeshCone(r, h, slices)); });
}
std::shared_ptr<Geometry> Geometry::torus(float r, float tube, int radSeg, int sides) {
    return std::make_shared<Geometry>([=] { return ringTorus(r, tube, radSeg, sides); });
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
void ModelAsset::poseBones(const Matrix* matrices, int count) {
    ensure();
    if (!model_.boneMatrices || !model_.skeleton.boneCount) throw std::runtime_error("g3d: poseBones: model has no skeleton: " + path_);
    const int n = std::min<int>(count, (int)model_.skeleton.boneCount);
    for (int i = 0; i < n; i++) model_.boneMatrices[i] = matrices[i];
    overrideBones_ = true;
}
void ModelAsset::clearBones() { overrideBones_ = false; }
int ModelAsset::animationFrames(int index) {
    ensure();
    return index >= 0 && index < animCount_ ? anims_[index].keyframeCount : 0;
}
void ModelAsset::pose(int animation, float frame) {
    ensure();
    if (overrideBones_) return;
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
Object3D* Object3D::add(std::shared_ptr<Instances> i, Material m) {
    Object3D* o = add();
    o->instances = std::move(i);
    o->material = std::move(m);
    return o;
}
Object3D* Object3D::add(std::shared_ptr<Field> f, Material m) {
    Object3D* o = add();
    o->field = std::move(f);
    o->material = std::move(m);
    return o;
}
Object3D* Object3D::add(std::shared_ptr<Splats> s) {
    Object3D* o = add();
    o->splats = std::move(s);
    return o;
}
Object3D* Object3D::add(std::shared_ptr<Surface> s) {
    Object3D* o = add();
    makeSurface(*o, std::move(s));
    return o;
}
Lines::Path& Lines::path(std::vector<Vector3> points, Color color, float width, bool closed) {
    Path p;
    p.points = std::move(points);
    p.color = color;
    p.width = width;
    p.closed = closed;
    paths.push_back(std::move(p));
    return paths.back();
}
Lines::Path& Lines::line(Vector3 a, Vector3 b, Color color, float width) { return path({ a, b }, color, width); }
Lines::Path& Lines::arrow(Vector3 from, Vector3 to, Color color, float width, Head head) {
    Path& p = path({ from, to }, color, width);
    p.head = head;
    return p;
}
Lines::Path& Lines::circle(Vector3 centre, Vector3 normal, float radius, Color color, float width, float a0, float a1,
                           Vector3 start, int segments) {
    const Vector3 n = Vector3Normalize(normal);
    Vector3 s = start;
    if (Vector3Length(Vector3CrossProduct(s, n)) < 1e-6f) s = std::fabs(n.x) < 0.9f ? Vector3{ 1, 0, 0 } : Vector3{ 0, 0, 1 };
    const Vector3 e1 = Vector3Normalize(Vector3Subtract(s, Vector3Scale(n, Vector3DotProduct(s, n))));
    const Vector3 e2 = Vector3CrossProduct(n, e1);
    const bool full = std::fabs(a1 - a0) >= 360 - 1e-3f;
    const int k = std::max(3, (int)std::ceil(segments * std::fabs(a1 - a0) / 360));
    std::vector<Vector3> pts;
    for (int i = 0; i < (full ? k : k + 1); i++) {
        const float a = (a0 + (a1 - a0) * i / k) * DEG2RAD;
        pts.push_back(Vector3Add(centre, Vector3Add(Vector3Scale(e1, radius * std::cos(a)), Vector3Scale(e2, radius * std::sin(a)))));
    }
    return path(std::move(pts), color, width, full);
}
void Lines::box(Vector3 a, Vector3 b, Color color, float width) {
    path({ { a.x, a.y, a.z }, { b.x, a.y, a.z }, { b.x, a.y, b.z }, { a.x, a.y, b.z } }, color, width, true);   // bottom
    path({ { a.x, b.y, a.z }, { b.x, b.y, a.z }, { b.x, b.y, b.z }, { a.x, b.y, b.z } }, color, width, true);   // top
    line({ a.x, a.y, a.z }, { a.x, b.y, a.z }, color, width);                                                  // uprights
    line({ b.x, a.y, a.z }, { b.x, b.y, a.z }, color, width);
    line({ b.x, a.y, b.z }, { b.x, b.y, b.z }, color, width);
    line({ a.x, a.y, b.z }, { a.x, b.y, b.z }, color, width);
}

Object3D* Object3D::add(std::shared_ptr<Lines> l) {
    Object3D* o = add();
    o->lines = std::move(l);
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
Object3D* Scene::add(std::shared_ptr<Instances> i, Material m) {
    Object3D* o = add();
    o->instances = std::move(i);
    o->material = std::move(m);
    return o;
}
Object3D* Scene::add(std::shared_ptr<Field> f, Material m) {
    Object3D* o = add();
    o->field = std::move(f);
    o->material = std::move(m);
    return o;
}

// FIELD_VS's placement, on the CPU.
namespace {
uint32_t fieldHash(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
}
int Field::tilesX() const { return std::max(1, (int)std::ceil((max.x - min.x) / std::max(tileSize, 0.05f) - 1e-4f)); }
int Field::tilesZ() const { return std::max(1, (int)std::ceil((max.y - min.y) / std::max(tileSize, 0.05f) - 1e-4f)); }
uint32_t Field::bladesPerTile() const {
    const float ts = std::max(tileSize, 0.05f);
    return (uint32_t)std::clamp(std::lround(std::max(density, 0.f) * ts * ts), 0L, 1L << 22);
}
Vector2 Field::bladeRoot(int tx, int tz, uint32_t k) const {
    const uint32_t th = fieldHash(((uint32_t)tx * 73856093u) ^ ((uint32_t)tz * 19349663u) ^ (seed * 83492791u));
    const uint32_t x = k * 3242174889u + th, z = k * 2447445413u + fieldHash(th);
    const float ts = std::max(tileSize, 0.05f);
    return { min.x + (tx + (float)(x >> 8) / 16777216.f) * ts, min.y + (tz + (float)(z >> 8) / 16777216.f) * ts };
}
long long fieldBladesSubmitted() { return gpu.fieldBlades; }
long long precipitationDropsSubmitted() { return gpu.pr.drops; }
long long precipitationSplashesSubmitted() { return gpu.pr.splashes; }
long long fieldShadowBladesSubmitted() { return gpu.fieldShadowBlades; }
long long instanceBodiesSubmitted() { return gpu.instanceBodies; }
long long instanceTrianglesSubmitted() { return gpu.instanceTriangles; }

const Camera& drawingCamera() {
    static const Camera none{};
    return viewing ? *viewing : none;
}
const Scene* drawingScene() { return viewingScene; }
float drawingViewportHeight() { return viewingHeight; }

Object3D* Scene::add(std::shared_ptr<Splats> s) {
    Object3D* o = add();
    o->splats = std::move(s);
    return o;
}
Object3D* Scene::add(std::shared_ptr<Surface> s) {
    Object3D* o = add();
    makeSurface(*o, std::move(s));
    return o;
}
Object3D* Scene::add(std::shared_ptr<g3d::Lines> l) {
    Object3D* o = add();
    o->lines = std::move(l);
    return o;
}
Object3D* Scene::add(Object3D* object) {
    if (!object) return nullptr;
    objects.emplace_back(object);
    return object;
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

Camera cameraAt(const Camera& camera, int t) {
    Camera c = camera;
    if (c.up) c.up(t, c);
    return c;
}

// MatrixRotateXYZ(a) has columns R(-a) of x, y and z: its third column is
// (-sin y, cos y sin x, cos y cos x) and its first row (cos z cos y, sin z cos y, -sin y) in
// the negated angles. Solved for those from the wanted axes.
Vector3 lookRotation(Vector3 forward, Vector3 up) {
    const Vector3 Z = Vector3Normalize(forward);
    Vector3 X = Vector3CrossProduct(up, Z);
    if (Vector3Length(X) < 1e-6f) X = Vector3CrossProduct(std::fabs(Z.y) < 0.9f ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 }, Z);
    X = Vector3Normalize(X);
    const Vector3 Y = Vector3CrossProduct(Z, X);
    const float sy = std::clamp(-Z.x, -1.f, 1.f), y = std::asin(sy);
    float x, z;
    if (std::fabs(sy) < 0.9999f) {
        x = std::atan2(Z.y, Z.z);
        z = std::atan2(Y.x, X.x);
    } else {                                     // looking along x: only x + z matters
        z = 0;
        x = std::atan2(X.y * sy, X.z * sy);
    }
    return { -x * RAD2DEG, -y * RAD2DEG, -z * RAD2DEG };
}

bool project(const Camera& cam, Vector3 p, Rectangle vp, Vector2& out) {
    if (vp.width <= 0 || vp.height <= 0) return false;
    // The same view and projection as renderBody.
    const Matrix view = MatrixLookAt(cam.position, cam.target, safeUp(Vector3Subtract(cam.target, cam.position), cam.upVector));
    const Vector3 v = Vector3Transform(p, view);   // camera space: looking down -z
    const double aspect = (double)vp.width / vp.height;
    double nx, ny;
    if (cam.orthographic) {
        const double top = cam.orthoHeight / 2.0;
        nx = v.x / (top * aspect);
        ny = v.y / top;
    } else {
        if (-v.z <= cam.nearPlane) return false;
        const double f = 1.0 / std::tan(cam.fov * 0.5 * DEG2RAD);
        nx = f / aspect * v.x / -v.z;
        ny = f * v.y / -v.z;
    }
    out = { (float)(vp.x + (nx + 1) / 2 * vp.width), (float)(vp.y + (1 - ny) / 2 * vp.height) };
    return true;
}

namespace {
// `o`'s world matrix at frame t, found by walking down from the scene's roots and running the
// `up` of every object on the way (each is pure, so running it again changes nothing).
bool worldOf(std::vector<std::unique_ptr<Object3D>>& list, const Object3D* o, int t, const Matrix& parent, Matrix& out) {
    for (auto& c : list) {
        if (c.get() != o && c->children.empty()) continue;
        if (c->up) c->up(t, *c);
        const Matrix world = MatrixMultiply(localMatrix(*c), parent);
        if (c.get() == o) { out = world; return true; }
        if (worldOf(c->children, o, t, world, out)) return true;
    }
    return false;
}
} // namespace

Camera fillCamera(const Camera& base, Scene& scene, Object3D& o, int t, float aspect, float margin) {
    if (!o.surface) throw std::invalid_argument("g3d::fillCamera: the object is not a Surface");
    if (o.surface->billboard) throw std::invalid_argument("g3d::fillCamera: a billboard surface always faces the camera");
    Matrix m;
    if (!worldOf(scene.objects, &o, t, MatrixIdentity(), m)) throw std::invalid_argument("g3d::fillCamera: the object is not in the scene");
    const Surface& s = *o.surface;
    const SurfaceRect r = surfaceRect(s);
    const Vector3 centre = Vector3Transform({ (r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2, 0 }, m);
    const Vector3 X = column(m, 0), Y = column(m, 1), Z = column(m, 2);
    const float w = Vector3Length(X) * s.size.x, h = Vector3Length(Y) * s.size.y;
    const float fit = std::max(h, w / std::max(1e-3f, aspect));
    const float shown = fit / std::max(0.05f, 1 - 2 * margin);       // the frame's height, metres
    Camera c = base;
    c.up = nullptr;
    float d;
    if (c.orthographic) {
        c.orthoHeight = shown;
        d = std::max(shown, 1.f);
    } else {
        d = shown * 0.5f / std::tan(c.fov * 0.5f * DEG2RAD);
    }
    c.position = Vector3Add(centre, Vector3Scale(Vector3Normalize(Z), d));
    c.target = centre;
    c.upVector = Vector3Normalize(Y);
    if (c.nearPlane > d * 0.5f) c.nearPlane = d * 0.1f;
    return c;
}

Camera mix(const Camera& a, const Camera& b, float u) {
    Camera c = a;
    c.position = Vector3Lerp(a.position, b.position, u);
    c.target = Vector3Lerp(a.target, b.target, u);
    const Vector3 up = Vector3Lerp(Vector3Normalize(a.upVector), Vector3Normalize(b.upVector), u);
    c.upVector = Vector3Length(up) > 1e-4f ? Vector3Normalize(up) : b.upVector;
    c.fov = Lerp(a.fov, b.fov, u);
    c.orthoHeight = Lerp(a.orthoHeight, b.orthoHeight, u);
    c.nearPlane = Lerp(a.nearPlane, b.nearPlane, u);
    c.farPlane = Lerp(a.farPlane, b.farPlane, u);
    c.orthographic = u < 0.5f ? a.orthographic : b.orthographic;
    return c;
}

Ctx fillCtx(const Surface& s, float width, float height, float margin) {
    const float room = std::max(0.05f, 1 - 2 * margin);
    const float k = std::min(width * room / std::max(1.f, s.pixels.x), height * room / std::max(1.f, s.pixels.y));
    return Ctx{ { (width - s.pixels.x * k) / 2, (height - s.pixels.y * k) / 2 }, k, 0, 1 };
}

namespace {
// While a frame is drawn with Scene::weather enabled: the fog (from the haze) and the sun, sky and
// ambient light (under cloud) as the weather sets them. The user's values come back when it ends,
// so nothing carries from one frame to the next. The wind and the sky are worked in where they
// are drawn (inWeather), on copies.
struct WeatherScope {
    Scene& s;
    FogMode mode;
    float density, ambient, sunIntensity = 0, skyLighting;
    bool fromSky, fog = false, sky = false;
    int sun = -1;
    explicit WeatherScope(Scene& sc) : s(sc), mode(sc.fogMode), density(sc.fogDensity), ambient(sc.ambient), skyLighting(sc.sky.lighting),
                                       fromSky(sc.fogFromSky) {
        const Weather& w = s.weather;
        if (!w.enabled) return;
        if (!s.ownFog) {
            fog = true;
            s.fogMode = FogMode::Exponential;
            s.fogDensity = w.fogDensity();
            s.fogFromSky = true;
        }
        if (s.sky.enabled && !s.sky.ownLight) {              // a clouded sky lights the scene less
            sky = true;
            s.sky.lighting = skyLighting * w.skyLightScale();
        }
        if (s.sky.enabled && !s.sky.ownLight && s.sky.sunLight >= 0 && s.sky.sunLight < (int)s.lights.size()) {
            sun = s.sky.sunLight;
            sunIntensity = s.lights[(size_t)sun].intensity;
            s.lights[(size_t)sun].intensity = sunIntensity * w.sunScale();
            s.ambient = ambient * w.ambientScale();
        }
    }
    ~WeatherScope() {
        if (fog) { s.fogMode = mode; s.fogDensity = density; s.fogFromSky = fromSky; }
        if (sky) s.sky.lighting = skyLighting;
        if (sun >= 0 && sun < (int)s.lights.size()) { s.lights[(size_t)sun].intensity = sunIntensity; s.ambient = ambient; }
    }
    WeatherScope(const WeatherScope&) = delete;
    WeatherScope& operator=(const WeatherScope&) = delete;
};

// While a frame is drawn with Volumetric::density or heightFalloff set: the scene's fog is that exponential fog (so the
// surfaces and the march share one medium). The fog takes the sky's colour unless the scene gave one.
struct VolumetricScope {
    Scene& s;
    FogMode mode;
    float density, height, falloff;
    bool fromSky, on = false;
    explicit VolumetricScope(Scene& sc) : s(sc), mode(sc.fogMode), density(sc.fogDensity), height(sc.fogHeight), falloff(sc.fogFalloff), fromSky(sc.fogFromSky) {
        const Volumetric& v = s.volumetric;
        if (!v.enabled() || (v.density <= 0 && v.heightFalloff <= 0)) return;
        on = true;
        if (v.density > 0) {
            s.fogMode = FogMode::Exponential;
            s.fogDensity = v.density;
            if (s.fogColor.r == 0 && s.fogColor.g == 0 && s.fogColor.b == 0 && (s.sky.enabled || !s.environment.empty())) s.fogFromSky = true;
        }
        if (v.heightFalloff > 0) { s.fogFalloff = v.heightFalloff; s.fogHeight = v.heightBase; }
    }
    ~VolumetricScope() {
        if (!on) return;
        s.fogMode = mode; s.fogDensity = density; s.fogHeight = height; s.fogFalloff = falloff; s.fogFromSky = fromSky;
    }
    VolumetricScope(const VolumetricScope&) = delete;
    VolumetricScope& operator=(const VolumetricScope&) = delete;
};
} // namespace

void render(Scene& s, int t, const Ctx& c, Rectangle viewport, Camera* camera) {
    Camera& cam = camera ? *camera : s.camera;
    // Draw what 2D work the caller has batched first: creating canvas render textures or
    // uploading meshes below rebinds the framebuffer, and a later flush would send the
    // pending vertices to the wrong one.
    rlDrawRenderBatchActive();
    if (s.up) s.up(t, s);
    if (cam.up) cam.up(t, cam);
    for (Light& l : s.lights) if (l.up) l.up(t, l);
    Cull cull;
    cull.cam = &cam;
    cull.floorY = s.cullFloor;
    {
        const Vector2 cs = canvasSize();
        cull.aspect = viewport.width > 0 && viewport.height > 0 ? viewport.width / viewport.height : cs.x / std::max(1.f, cs.y);
        cull.on = cull.aspect > 0;
        for (const Light& l : s.lights) {
            if (!l.castShadow) continue;
            if (l.type != Light::Type::Directional) { cull.on = false; break; }
            const Vector3 d = Vector3Subtract(l.target, l.position);
            if (Vector3Length(d) > 1e-6f) {
                cull.shadowDirs.push_back(Vector3Normalize(d));
                cull.footprints.push_back({ l.target, Vector3Normalize(d), l.shadowExtent });
            }
        }
    }
    {
        const Vector2 cs = canvasSize();
        cull.screenH = viewport.width > 0 && viewport.height > 0 ? viewport.height * c.s : std::max(1.f, cs.y);
        cull.compH = viewport.width > 0 && viewport.height > 0 ? viewport.height : cull.screenH / std::max(1e-4f, c.s);
    }
    struct Viewing {                                      // drawingCamera() while the scene's `up`s run
        const Camera* before;
        const Scene* sceneBefore;
        float heightBefore;
        Viewing(const Camera* c, const Scene* s, float h) : before(viewing), sceneBefore(viewingScene), heightBefore(viewingHeight) { viewing = c; viewingScene = s; viewingHeight = h; }
        ~Viewing() { viewing = before; viewingScene = sceneBefore; viewingHeight = heightBefore; }
    } viewingNow(&cam, &s, cull.compH);
    Camera mirrorCam;                                     // the water's reflection pass looks from here (when there is one)
    {
        const WaterView wv = reflectionView(s, cam, cull.aspect, t);    // the sea's level, else a body's
        if (wv.visible) {
            mirrorCam = mirroredCamera(cam, wv.level);
            cull.mirror = &mirrorCam;
        }
    }
    {
        const Downfall pd = inWeather(s.precipitation, s.weather);
        if (pd.enabled && pd.kind != Precipitation::None && (pd.occlusion || pd.splashes)) cull.rainR = pd.radius + 6.f;   // roofs out of view still keep the rain off
    }
    animate(s.objects, t, MatrixIdentity(), cull);
    const WeatherScope weatherNow(s);
    const VolumetricScope volumetricNow(s);
    const FlashScope flashNow(s, t);                     // a strike's light, for this frame's lights
    if (canvasSize().x > 0) {
        sizeSurfaces(s.objects, MatrixIdentity(), cam, cull.screenH * cull.aspect, cull.screenH);
        drawCanvases(s.objects, t);
    }
    drawOffscreen(c.a, [&](const RenderTexture& target, float scale) {
        Rectangle vp = viewport;
        if (vp.width <= 0 || vp.height <= 0) vp = { 0, 0, target.texture.width / scale, target.texture.height / scale };
        else vp = { c.o.x + vp.x * c.s, c.o.y + vp.y * c.s, vp.width * c.s, vp.height * c.s };
        renderBody(s, cam, vp, target, scale, t, cull);
    });
}

} // namespace g3d
} // namespace rmtn
