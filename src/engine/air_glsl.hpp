// air_glsl.hpp -- the shaders of Scene::volumetric (sun shafts in the fog) and Scene::aerial (aerial perspective).
// Included by scene3d.cpp only.
#pragma once

namespace rmtn {
namespace g3d {
namespace {

// What both passes share: the sky as an environment (the lit shader's env()), the horizon colour fog and haze fade to,
// and the depth to distance along a ray.
#define AIR_COMMON_GLSL R"(
const float PI = 3.14159265;
uniform vec3 camPos; uniform vec3 camFwd; uniform vec3 camRight; uniform vec3 camUp; uniform vec2 tanHalf;
uniform vec2 vpOrigin; uniform vec2 vpSize; uniform vec2 zNF; uniform vec2 lowSize;
uniform sampler2D envMap; uniform int hasEnv; uniform int envLinear; uniform float envMaxLod;
uniform float envIntensity; uniform float envRotation; uniform float backdropScale;
uniform vec3 horizonFallback;
vec3 env(vec3 d, float lod) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5 + envRotation;
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;
    vec3 c = textureLod(envMap, vec2(u, v), lod).rgb;
    return (envLinear == 1 ? c : pow(c, vec3(2.2))) * envIntensity;
}
// The sky's colour at the horizon in the direction of d (just above it, so it is the sky's, not the ground's).
vec3 horizonColor(vec3 d) {
    if (hasEnv == 0) return horizonFallback;
    return env(normalize(vec3(d.x, 0.03 * length(d.xz), d.z) + vec3(1e-6, 0.0, 0.0)), max(envMaxLod - 7.0, 0.0)) * backdropScale;
}
float linz(float d) { return 2.0 * zNF.x * zNF.y / (zNF.y + zNF.x - (d * 2.0 - 1.0) * (zNF.y - zNF.x)); }
)"

// The march, one fragment per low-resolution pixel. While it is on the lit shader draws no fog (the surfaces would carry
// it twice, and 8 bits cannot take a bright fog off again), so this is all of it: rgb is the light the fog scatters to
// the eye (linear), a the share of the scene behind that gets through; the scene is scene * a + rgb.
const char* AIR_MARCH_FS = "#version 330\nout vec4 finalColor;\n" AIR_COMMON_GLSL R"(
uniform sampler2D depthTex;
uniform sampler2D shadowAtlas; uniform float atlasTexel;
uniform mat4 shadowVP[16]; uniform vec4 shadowRect[16]; uniform vec4 shadowParams[16];
uniform vec3 fogColor; uniform float fogNear; uniform float fogFar;
uniform int fogMode; uniform float fogDensity; uniform float fogHeight; uniform float fogFalloff; uniform int fogSky; uniform int seaSky;
uniform int steps; uniform float reach;
uniform float waterLevel; uniform int waterOn;
uniform int sun; uniform vec3 sunDir; uniform float sunGain; uniform float aniso; uniform float ambientK;
uniform int cloudShadowOn; uniform int cloudShadowLight; uniform sampler2D cloudShadowTex;
uniform vec4 cloudShadowMap; uniform vec3 cloudShadowSun; uniform float cloudShadowFloor;
uniform int scatterLights; uniform int lightCount;
uniform int lightType[8]; uniform vec3 lightPos[8]; uniform vec3 lightDir[8]; uniform vec3 lightColor[8];
uniform float lightRange[8]; uniform float lightCosInner[8]; uniform float lightCosOuter[8];
uniform int lightShadow[8]; uniform float lightBias[8];

// ---- the fog, as the lit shader has it: how much of the way to distance d along dir is fog (0 clear, 1 opaque)
float fogAt(vec3 dir, float d) {
    if (fogMode == 0) return fogFar > fogNear ? clamp((d - fogNear) / (fogFar - fogNear), 0.0, 1.0) : 0.0;
    if (fogDensity <= 0.0) return 0.0;
    float od = fogDensity * d;
    if (fogFalloff > 0.0) {
        float a = fogFalloff * dir.y * d;
        float g = abs(a) > 1e-4 ? (1.0 - exp(-a)) / a : 1.0 - 0.5 * a;
        od *= exp(-fogFalloff * (camPos.y - fogHeight)) * g;
    }
    return fogMode == 1 ? 1.0 - exp(-od) : 1.0 - exp(-od * od);
}
vec3 fogTint(vec3 dir) {
    if (fogSky == 1 && hasEnv == 1) {
        vec3 d = dir;
        if (seaSky == 1 && d.y < 0.0) d = vec3(d.x, 0.0, d.z) / max(length(d.xz), 1e-4);
        return env(d, max(envMaxLod - 7.0, 0.0)) * backdropScale;
    }
    return fogColor;
}
// The distance of boundary i of `steps`, so that each segment holds the same share of the fog's opacity where that has a
// closed form (the fogs without a height falloff); evenly in distance otherwise. dEnd is the last boundary.
float boundary(int i, float dEnd, float qEnd) {
    float u = float(i) / float(steps);
    if (fogFalloff <= 0.0 && fogDensity > 0.0 && fogMode >= 1) {
        float lt = -log(max(1.0 - qEnd * u, 1e-6));
        return min(dEnd, (fogMode == 1 ? lt : sqrt(lt)) / fogDensity);
    }
    return dEnd * u;
}

// ---- sun and lights: the shadow atlas read at a point in the air (no surface, so no normal offset)
float linearDepth(float d, vec4 prm) {
    if (prm.z > 0.5) { float z = d * 2.0 - 1.0; return 2.0 * prm.x * prm.y / (prm.y + prm.x - z * (prm.y - prm.x)); }
    return prm.x + d * (prm.y - prm.x);
}
float shadowAtP(int tile, float bias, vec3 p) {
    vec4 prm = shadowParams[tile], r = shadowRect[tile];
    vec4 clip = shadowVP[tile] * vec4(p, 1.0);
    if (clip.w <= 0.0) return 1.0;
    vec3 q = clip.xyz / clip.w * 0.5 + 0.5;
    if (q.z >= 1.0 || q.z <= 0.0 || q.x <= 0.0 || q.x >= 1.0 || q.y <= 0.0 || q.y >= 1.0) return 1.0;
    float zf = prm.z > 0.5 ? clip.w : linearDepth(q.z, prm);
    float texelWorld = prm.w * (prm.z > 0.5 ? zf : 1.0);
    float b = bias + texelWorld * 2.5;
    vec2 lo = r.xy + vec2(1.5 * atlasTexel), hi = r.xy + r.zw - vec2(1.5 * atlasTexel);
    float lit = 0.0;
    for (int k = 0; k < 4; k++) {
        vec2 o = vec2(k & 1, k >> 1) * 2.0 - 1.0;
        vec2 uv = clamp(r.xy + q.xy * r.zw + o * atlasTexel, lo, hi);
        lit += zf - b > linearDepth(texture(shadowAtlas, uv).r, prm) ? 0.0 : 1.0;
    }
    return lit * 0.25;
}
int cubeFace(vec3 d) {
    vec3 a = abs(d);
    if (a.x >= a.y && a.x >= a.z) return d.x > 0.0 ? 0 : 1;
    if (a.y >= a.z) return d.y > 0.0 ? 2 : 3;
    return d.z > 0.0 ? 4 : 5;
}
float cloudVisAt(vec3 p) {
    float s = (cloudShadowMap.w - p.y) / cloudShadowSun.y;
    vec2 uv = (p.xz + cloudShadowSun.xz * s - cloudShadowMap.xy) * cloudShadowMap.z + 0.5;
    float edge = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
    if (edge <= 0.0) return 1.0;
    float t = textureLod(cloudShadowTex, uv, 0.0).r;
    return mix(1.0, mix(cloudShadowFloor, 1.0, t), smoothstep(0.0, 0.08, edge));
}
// 4 pi times the Henyey-Greenstein phase function, compressed (a third of its swing) so that the glow round the sun
// does not blow out; its mean over the sphere is still 1, and it is 1 everywhere at g = 0.
float phaseShape(float c) {
    float k = 1.0 + aniso * aniso - 2.0 * aniso * c;
    float hg = (1.0 - aniso * aniso) / (k * sqrt(k));
    return 1.0 + 0.3 * (min(hg, 8.0) - 1.0);
}
// Interleaved gradient noise (Jimenez 2014) on the pixel grid: the same every frame.
float ign(vec2 p) { return fract(52.9829189 * fract(dot(floor(p), vec2(0.06711056, 0.00583715)))); }

void main() {
    vec2 uv = gl_FragCoord.xy / lowSize, ndc = uv * 2.0 - 1.0;
    vec3 dir = normalize(camFwd + camRight * (ndc.x * tanHalf.x) + camUp * (ndc.y * tanHalf.y));
    float dep = texelFetch(depthTex, ivec2(vpOrigin + uv * vpSize), 0).r;
    bool sky = dep >= 0.99999;
    float dSurf = 1e9;
    if (!sky) dSurf = linz(dep) / dot(dir, camFwd);
    if (waterOn == 1 && camPos.y > waterLevel && dir.y < 0.0) {            // the sea is not in the depth buffer: its plane is the surface
        float ds = (waterLevel - camPos.y) / dir.y;
        if (ds < dSurf) { dSurf = ds; sky = false; }
    }
    float dEnd = min(dSurf, reach);
    float tEnd = 1.0 - fogAt(dir, dEnd), qEnd = 1.0 - tEnd;
    if (qEnd < 1e-5) { finalColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
    float jit = ign(gl_FragCoord.xy);
    float shape = sunGain > 0.0 ? phaseShape(dot(dir, sunDir)) : 1.0;
    float sumLit = 0.0;
    vec3 lights = vec3(0.0);
    float qPrev = 0.0, bPrev = 0.0;
    for (int i = 1; i <= steps; i++) {
        float b = boundary(i, dEnd, qEnd);
        float q = fogAt(dir, b);
        float w = q - qPrev;
        vec3 p = camPos + dir * mix(bPrev, b, jit);
        qPrev = q; bPrev = b;
        if (w <= 0.0) continue;
        float vis = 1.0;
        if (sun >= 0 && lightShadow[sun] >= 0) vis = shadowAtP(lightShadow[sun], lightBias[sun], p);
        if (cloudShadowOn == 1 && cloudShadowLight == sun) vis *= cloudVisAt(p);
        sumLit += w * (ambientK + (1.0 - ambientK) * mix(1.0, vis * shape, sunGain));
        if (scatterLights == 1) {
            vec3 acc = vec3(0.0);
            for (int k = 0; k < 8; k++) {
                if (k >= lightCount) break;
                if (lightType[k] == 0) continue;
                vec3 dl = lightPos[k] - p;
                float dist = length(dl);
                vec3 l = dl / max(dist, 1e-4);
                float atten = 1.0 / (1.0 + 0.1 * dist * dist);
                if (lightRange[k] > 0.0) { float kk = clamp(1.0 - dist * dist / (lightRange[k] * lightRange[k]), 0.0, 1.0); atten = kk * kk; }
                if (lightType[k] == 2) atten *= smoothstep(lightCosOuter[k], lightCosInner[k], dot(-l, lightDir[k]));
                if (atten <= 0.0) continue;
                if (lightShadow[k] >= 0) atten *= shadowAtP(lightShadow[k] + (lightType[k] == 1 ? cubeFace(p - lightPos[k]) : 0), lightBias[k], p);
                acc += lightColor[k] * (atten * phaseShape(dot(dir, l)));
            }
            lights += w * 0.5 * acc;
        }
    }
    vec3 tint = fogTint(dir);
    finalColor = vec4(tint * sumLit + lights, tEnd);                       // what the fog adds, and what it lets through
}
)";

// The composite, one fragment per pixel: the march's image brought up to size (a depth-aware filter that also averages
// away the march's jitter), added to the picture, then the aerial perspective on whatever has a depth.
const char* AIR_COMPOSITE_FS = "#version 330\nout vec4 finalColor;\n" AIR_COMMON_GLSL R"(
uniform sampler2D sceneTex; uniform sampler2D depthTex; uniform sampler2D lowTex;
uniform int volOn; uniform int filterOn;
uniform int aerialOn; uniform float aerialK; uniform float aerialH; uniform vec3 aerialRGB;
uniform float waterLevel; uniform int waterOn;

void main() {
    ivec2 px = ivec2(gl_FragCoord.xy);
    vec4 src = texelFetch(sceneTex, px, 0);
    float dep = texelFetch(depthTex, px, 0).r;
    bool sky = dep >= 0.99999;
    vec2 uv = (gl_FragCoord.xy - vpOrigin) / vpSize, ndc = uv * 2.0 - 1.0;
    vec3 dir = normalize(camFwd + camRight * (ndc.x * tanHalf.x) + camUp * (ndc.y * tanHalf.y));
    vec3 c = pow(src.rgb, vec3(2.2));
    if (volOn == 1) {
        ivec2 n = ivec2(clamp(floor(uv * lowSize), vec2(0.0), lowSize - 1.0));
        vec4 cC = texelFetch(lowTex, n, 0);
        vec4 m = cC;
        if (filterOn == 1) {
            float zf = sky ? 1e7 : linz(dep);
            vec4 acc = vec4(0.0); float wsum = 0.0;
            for (int j = -2; j <= 2; j++)
                for (int i = -2; i <= 2; i++) {
                    ivec2 ij = clamp(n + ivec2(i, j), ivec2(0), ivec2(lowSize) - 1);
                    vec4 t = texelFetch(lowTex, ij, 0);
                    float dl = texelFetch(depthTex, ivec2(vpOrigin + (vec2(ij) + 0.5) / lowSize * vpSize), 0).r;
                    float zl = dl >= 0.99999 ? 1e7 : linz(dl);
                    float rel = min(abs(zf - zl) / max(min(zf, zl), 1.0), 4.0);
                    float w = exp(-float(i * i + j * j) / 3.4) * exp(-abs(t.a - cC.a) * 4.0) / (0.02 + rel);
                    acc += t * w;
                    wsum += w;
                }
            m = acc / wsum;
        }
        if (!(sky && src.a < 0.01)) c = c * m.a + m.rgb;
    }
    // The sea is not in the depth buffer: where its plane is in front of the depth (land under the water) the pixel is sea, whose own haze is the sea's.
    bool seaFront = false;
    if (waterOn == 1 && camPos.y > waterLevel && dir.y < 0.0 && !sky) {
        float ds = (waterLevel - camPos.y) / dir.y, k = 1.0 / dot(dir, camFwd);
        seaFront = ds < 0.97 * linz(dep) * k;
    }
    if (aerialOn == 1 && !sky && !seaFront) {
        float d = linz(dep) / dot(dir, camFwd);
        float s = 0.0;
        for (int k = 0; k < 6; k++) s += exp(-max(camPos.y + dir.y * d * ((float(k) + 0.5) / 6.0), 0.0) / aerialH);
        float od = aerialK * d * s / 6.0;
        // The thing's own light loses its blue first (blue is scattered out of the path most); what the air adds is the
        // sky's own horizon colour, by the green optical depth. So the thing goes bluer toward that colour, and never past it.
        c = c * exp(-od * aerialRGB) + horizonColor(dir) * (1.0 - exp(-od));
    }
    // A fixed dither (interleaved gradient noise, half a level either way): dark fog's gradients must not show 8-bit contours.
    float dn = fract(52.9829189 * fract(dot(floor(gl_FragCoord.xy), vec2(0.06711056, 0.00583715)))) - 0.5;
    finalColor = vec4(pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2)) + vec3(dn * step(0.01, src.a) / 255.0), src.a);
}
)";

} // namespace
} // namespace g3d
} // namespace rmtn
