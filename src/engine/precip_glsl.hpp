// precip_glsl.hpp -- the shaders of g3d::Downfall (GPU rain, snow and sleet). Included by scene3d.cpp only.
//
// Every drop is made here from (tile, k): see Downfall in scene3d.hpp for the layout. Downfall::drop()
// in weather.cpp is the CPU twin of makeDrop(); keep the two in step (the tests compare them).
// The mesh is a bare VAO: the corners come from gl_VertexID (6 for a streak or flake, 12 for a splash,
// whose second quad holds the droplets).
#pragma once

namespace rmtn {
namespace g3d {
namespace {

const char* PRECIP_VS = R"(#version 330
uniform mat4 prVP;
uniform vec3 prCam; uniform vec3 prForward; uniform vec3 prRight; uniform vec3 prUp;
uniform int prOrtho; uniform float prPixel;           // prPixel: the size of a pixel at distance 1 (ortho: everywhere)
uniform float prSecs; uniform int prSeed; uniform int prKind;    // 0 rain, 1 snow, 2 sleet
uniform int prSplash; uniform int prFar;
uniform ivec3 prTile; uniform vec3 prCell;            // the tile (ix, iy, iz); its width, its height, its drop count
uniform vec2 prAr; uniform vec2 prAs;                 // slant of rain and of snow
uniform vec4 prRainP; uniform vec4 prSnowP;           // speed min, max, diameter min, max
uniform float prFlutter; uniform float prExposure; uniform float prIntensity; uniform float prOpacity;
uniform vec4 prRange;                                 // near radius, distance the thinning starts, far radius, far gain
uniform float prSplashR;
uniform vec2 prQShift;                                // 0, or the camera (worldFixed off)
uniform int prOccOn; uniform mat4 prOccVP; uniform mat4 prOccInv; uniform sampler2D prOcc; uniform float prOccRange;
uniform float prWater;                                // the water level, or -1e9
uniform int prLightCount; uniform int prLightType[8]; uniform vec3 prLightPos[8]; uniform vec3 prLightDir[8];
uniform vec3 prLightColor[8]; uniform float prLightRange[8]; uniform float prCosInner[8]; uniform float prCosOuter[8];
uniform vec3 prSky; uniform vec3 prGround;
uniform int hasEnv; uniform int envLinear; uniform float envMaxLod; uniform float envIntensity; uniform float envRotation;
uniform float backdropScale; uniform sampler2D envMap;
uniform vec3 fogColor; uniform float fogNear; uniform float fogFar; uniform int fogMode; uniform float fogDensity;
uniform float fogHeight; uniform float fogFalloff; uniform int fogSky;
out vec2 vUV; out vec4 vCol; out float vMode; out float vExtra; out float vSeed;

const float PI = 3.14159265;
const float BAND = 0.1;
const vec2 QC[6] = vec2[6](vec2(-1.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(-1.0, 0.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

uint hsh(uint x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
float rnd(uint x) { return float(hsh(x) >> 8) / 16777216.0; }

// One drop. Filled by makeDrop().
vec3 dPos; vec3 dVel; vec3 dBase; float dDia; float dSnow; float dRank; float dF; float dV; float dR1; float dR5;
void makeDrop(uint k) {
    uint sd = prFar == 1 ? uint(prSeed) ^ 0x9E3779B9u : uint(prSeed);
    uint ck = hsh(uint(prTile.x) * 73856093u ^ uint(prTile.z) * 19349663u ^ uint(prTile.y) * 83492791u ^ sd * 2654435761u);
    uint h = hsh(k * 2654435769u ^ ck);
    float r1 = rnd(h), r2 = rnd(h + 1u), r3 = rnd(h + 2u), r4 = rnd(h + 3u), r5 = rnd(h + 4u), r6 = rnd(h + 5u);
    dR1 = r1; dR5 = r5;
    dRank = (float(k) + 0.5) / max(prCell.z, 1.0);
    dSnow = (prKind == 1 || (prKind == 2 && r5 < 0.4)) ? 1.0 : 0.0;
    vec4 P = dSnow > 0.5 ? prSnowP : prRainP;
    dV = dSnow > 0.5 ? mix(P.x, P.y, r6) : mix(P.x, P.y, r3);
    dDia = mix(P.z, P.w, r3 * r3);
    vec2 a = (dSnow > 0.5 && prKind == 1) ? prAs : prAr;
    dF = fract(prSecs * dV / prCell.y + r4);
    float hgt = prCell.y * (1.0 - dF);
    vec2 q = (vec2(prTile.x, prTile.z) + vec2(r1, r2)) * prCell.x + prQShift;
    vec2 xz = q - a * hgt;
    vec2 sway = vec2(0.0);
    if (dSnow > 0.5) {
        float r7 = rnd(h + 6u), r8 = rnd(h + 7u);
        sway = prFlutter * vec2(sin(prSecs * (1.0 + r1) + r7 * 6.2831853), sin(prSecs * 1.3 * (1.0 + r2) + r8 * 6.2831853));
    }
    dBase = vec3(q.x, float(prTile.y) * prCell.y, q.y);
    dPos = vec3(xz.x + sway.x, dBase.y + hgt, xz.y + sway.y);
    dVel = vec3(a.x * dV, -dV, a.y * dV);
}

vec3 env(vec3 d, float lod) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5 + envRotation;
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;
    vec3 c = textureLod(envMap, vec2(u, v), lod).rgb;
    return (envLinear == 1 ? c : pow(c, vec3(2.2))) * envIntensity;
}
float fogAmount(vec3 p) {
    vec3 ray = p - prCam;
    float d = length(ray);
    if (fogMode == 0) return fogFar > fogNear ? clamp((d - fogNear) / (fogFar - fogNear), 0.0, 1.0) : 0.0;
    if (fogDensity <= 0.0) return 0.0;
    float od = fogDensity * d;
    if (fogFalloff > 0.0) {
        float a = fogFalloff * ray.y;
        float g = abs(a) > 1e-4 ? (1.0 - exp(-a)) / a : 1.0 - 0.5 * a;
        od *= exp(-fogFalloff * (prCam.y - fogHeight)) * g;
    }
    return fogMode == 1 ? 1.0 - exp(-od) : 1.0 - exp(-od * od);
}
vec3 fogTint(vec3 p) {
    if (fogSky == 1 && hasEnv == 1) return env(normalize(p - prCam), max(envMaxLod - 7.0, 0.0)) * backdropScale;
    return fogColor;
}

// The colour a drop shows at P seen along V (camera to drop), linear. The sky lights it from all round; each
// light scatters towards the camera: a forward lobe (a streak between the camera and a lamp or the low sun
// glints) on top of an isotropic share, so a lamp behind the camera lights the drops it shines on too. A
// lamp's share falls off as a soft inverse square (a 1 m core), so a lamp in rain has a bright cloud of
// streaks round it. dLit leaves the lamps' share, which also makes the streak more opaque. Rain is nearly
// clear (a faint body); a flake is white ice: albedo 0.9, Lambert-sphere phase, a little translucency.
float dLit;
vec3 dropColour(vec3 P, vec3 V, float snow) {
    dLit = 0.0;
    vec3 amb;
    if (hasEnv == 1) {
        float lod = max(envMaxLod - 5.0, 0.0);
        amb = env(normalize(vec3(V.x, 0.25 + 0.5 * abs(V.y), V.z)), lod);
        if (snow > 0.5) {
            amb = 0.5 * (env(normalize(vec3(V.x, max(V.y, 0.15), V.z)), lod) + env(normalize(vec3(0.5 * V.x, 1.0, 0.5 * V.z)), lod));      // the sky behind the flake and the sky overhead
            amb *= backdropScale * 1.7;                          // as bright as the sky it is seen against: the baked sky is dimmer than the sky drawn (clouds, haze), so a fixed lift matches them
            amb = mix(vec3(dot(amb, vec3(0.299, 0.587, 0.114))), amb, 0.25);    // snow takes a quarter of the sky's tint
        }
    } else amb = mix(prGround, prSky, 0.7);
    vec3 c = amb * (snow > 0.5 ? 1.0 : 0.55);
    for (int i = 0; i < 8; i++) {
        if (i >= prLightCount) break;
        vec3 toL; float atten = 1.0;
        bool local = prLightType[i] != 0;
        if (!local) toL = -prLightDir[i];
        else {
            vec3 d = prLightPos[i] - P;
            float dist = length(d);
            toL = d / max(dist, 1e-4);
            if (prLightRange[i] > 0.0) { float k = clamp(1.0 - (dist * dist) / (prLightRange[i] * prLightRange[i]), 0.0, 1.0); atten = k * k; }
            atten *= 1.0 / (1.0 + dist * dist);
            if (prLightType[i] == 2) atten *= smoothstep(prCosOuter[i], prCosInner[i], dot(-toL, prLightDir[i]));
        }
        float cf = dot(V, toL);                           // 1: the light is straight behind the drop
        float phase;
        if (snow > 0.5) {
            float al = acos(clamp(-cf, -1.0, 1.0));       // the angle at the flake between the light and the camera
            phase = (sin(al) + (PI - al) * cos(al)) / PI + 0.2 * pow(max(cf, 0.0), 4.0);
        } else phase = (local ? 0.5 : 0.12) + 2.2 * pow(max(cf, 0.0), 12.0);
        vec3 add = prLightColor[i] * atten * phase;
        c += add;
        if (local) dLit += dot(add, vec3(0.299, 0.587, 0.114));
    }
    return snow > 0.5 ? c * 0.9 : c;
}

// The map's depth at the drop's fall line, and the drop's own depth (both 0..1); false outside the map.
bool occAt(vec3 p, out float stored, out float own, out vec2 ndc) {
    vec4 c = prOccVP * vec4(p, 1.0);
    ndc = c.xy;
    own = c.z * 0.5 + 0.5;
    stored = 1.0;
    vec2 uv = c.xy * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return false;
    stored = textureLod(prOcc, uv, 0.0).r;
    return true;
}

void cullVertex() { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); vUV = vec2(0.0); vCol = vec4(0.0); vMode = -1.0; vExtra = 0.0; vSeed = 0.0; }

void splash(vec2 c, int quad) {
    if (dSnow > 0.5) { cullVertex(); return; }
    if (length(dBase.xz - prCam.xz) > 1.1 * prSplashR + 3.0) { cullVertex(); return; }       // far from the camera sideways (the fall line is slanted a little)
    // The surface this drop's line of fall reaches: from the map, or the water.
    bool has = false; float landY = -1e9; float stored, own; vec2 ndc;
    if (prOccOn >= 1 && occAt(dBase, stored, own, ndc)) {
        if (stored < 0.99999) {
            vec4 w = prOccInv * vec4(ndc, stored * 2.0 - 1.0, 1.0);
            landY = w.y / w.w; has = true;
        }
    }
    bool water = false;
    if (prWater > -1e8 && prWater >= landY) { landY = prWater; has = true; water = true; }
    if (!has) { cullVertex(); return; }
    float hL = landY - dBase.y;
    if (hL <= 0.0 || hL > prCell.y) { cullVertex(); return; }   // a surface on a tile edge belongs to the tile below
    float fl = 1.0 - hL / prCell.y;
    if (dF < fl) { cullVertex(); return; }
    float age = (dF - fl) * prCell.y / dV, life = 0.2;
    if (age > life) { cullVertex(); return; }
    float u = age / life;
    vec2 a = prAr;
    vec3 land = vec3(dBase.x - a.x * hL, landY, dBase.z - a.y * hL);
    vec3 V = prOrtho == 1 ? prForward : normalize(land - prCam);
    float pix = prOrtho == 1 ? prPixel : prPixel * length(land - prCam);
    float keep = 1.0 - smoothstep(0.5 * prSplashR, prSplashR, length(land - prCam));      // by the distance to where it lands, not to the tile's base
    float vis = clamp((prIntensity * keep * (1.0 + BAND) - dRank) / BAND, 0.0, 1.0);
    if (vis <= 0.001) { cullVertex(); return; }
    float fade = vis * (1.0 - u);
    vec3 col = dropColour(land + vec3(0.0, 0.05, 0.0), V, 0.0) * 2.0 + vec3(0.08);      // lit as the drops are, brighter: a few droplets catch the light
    float f = fogAmount(land);
    col = mix(col, fogTint(land), f);
    float big = water ? 1.3 : 1.0;
    float rq = (0.02 + 3.0 * dDia) * big;                 // the ring's largest radius
    float seen = land.y > prCam.y ? 0.0 : 1.0;            // the top of a roof above the eye is not seen: only spray over its edge
    if (quad == 0) {
        // A thin, faint ring flat on the surface, growing and fading. Under 3 px across it keeps its coverage as less alpha.
        float want = rq * 1.2, sz = max(want, 1.5 * pix);
        float cover = min(1.0, (want * want) / (sz * sz) * 4.0);
        vec3 p = land + vec3(c.x * sz, 0.004, (c.y * 2.0 - 1.0) * sz);
        gl_Position = prVP * vec4(p, 1.0);
        vUV = vec2(c.x, c.y * 2.0 - 1.0);
        vMode = 2.0; vExtra = u; vSeed = 0.0;
        vCol = vec4(col, min(0.3, prOpacity * 0.4 * cover * fade * seen));
    } else {
        // Droplets thrown up and out on short arcs, each a tiny streak along its own velocity (the drops' shutter rule). Under a
        // pixel wide, a droplet keeps its coverage as less alpha.
        uint hd = hsh(uint(dR1 * 16777216.0) + 100u + uint(quad) * 7919u);
        float ang = 6.2831853 * rnd(hd), vh = 0.15 + 0.35 * rnd(hd + 1u), vy = 0.7 + 0.6 * rnd(hd + 2u);
        float t = age;
        vec3 vel = vec3(vh * cos(ang), vy - 9.81 * t, vh * sin(ang));
        vec3 pos = land + vec3(vel.x * t, vy * t - 4.905 * t * t, vel.z * t) + vec3(0.0, 0.004, 0.0);
        float spd = length(vel);
        vec3 dir = vel / spd;
        vec3 Vd = prOrtho == 1 ? prForward : normalize(pos - prCam);
        vec3 s = cross(dir, Vd);
        s = length(s) > 1e-4 ? normalize(s) : prRight;
        float Wm = 0.006 * (0.7 + 0.6 * rnd(hd + 3u));
        float W = max(Wm, 1.0 * pix);
        float L = spd * prExposure;
        vec3 p = pos + dir * ((0.5 - c.y) * (L + 2.0 * pix)) + s * (c.x * W * 0.5 * 1.15);
        gl_Position = prVP * vec4(p, 1.0);
        vUV = vec2(c.x, c.y);
        vMode = 0.0; vExtra = 0.0; vSeed = 0.0;
        float cover = pow(min(1.0, Wm / W), 0.6);       // a sub-pixel droplet is a glint: its visibility falls slower than its area
        vCol = vec4(col, min(0.5, smoothstep(0.0, 0.25, u) * prOpacity * 0.7 * cover * fade * (seen > 0.5 ? 1.0 : 0.35) * (water ? 0.6 : 1.0)));
    }
}

void main() {
    vec2 c = QC[gl_VertexID % 6];
    int quad = gl_VertexID / 6;
    makeDrop(uint(gl_InstanceID));
    if (prSplash == 1) { splash(c, quad); return; }

    float dist = length(dPos - prCam);
    float m, fadeFar = 1.0, keepN = 1.0;
    if (prFar == 1) {
        m = prIntensity;
        fadeFar = smoothstep(0.7 * prRange.x, prRange.x, dist) * (1.0 - smoothstep(0.8 * prRange.z, prRange.z, dist));
    } else {
        float keep = min(1.0, pow(prRange.y / max(dist, 0.01), 1.5)) * (1.0 - smoothstep(0.75 * prRange.x, prRange.x, dist));
        m = prIntensity * keep; keepN = max(keep, 0.05);
    }
    float nearK = dSnow > 0.5 ? smoothstep(0.15, 0.8, dist) : smoothstep(0.4, 2.0, dist);       // a drop at the lens is out of focus
    float vis = clamp((m * (1.0 + BAND) - dRank) / BAND, 0.0, 1.0) * fadeFar * nearK;
    if (vis <= 0.001) { cullVertex(); return; }
    // Under a roof, a tree or a bridge: deeper than the map along the drop's line of fall.
    if (prOccOn == 1 && prFar == 0) {
        float stored, own; vec2 ndc;
        if (occAt(dPos, stored, own, ndc) && own > stored + 0.05 / prOccRange) { cullVertex(); return; }
    }
    if (dPos.y < prWater) { cullVertex(); return; }

    vec3 V = prOrtho == 1 ? prForward : (dist > 1e-4 ? (dPos - prCam) / dist : prForward);
    float pix = prOrtho == 1 ? prPixel : prPixel * dist;
    vec3 col = dropColour(dPos, V, dSnow);
    float f = fogAmount(dPos);
    col = mix(col, fogTint(dPos), f);
    vec3 world; float alpha;
    float soft = 0.0;
    if (dSnow > 0.5) {
        // Near flakes are out of focus: bigger, softer and fainter.
        soft = 1.0 - smoothstep(0.3, 2.5, dist);
        float D = max(dDia, 1.0 * pix) * (1.0 + 2.5 * soft);        // under a pixel a flake keeps its coverage, not its size: far snow is a veil
        float cover = min(1.0, (dDia * dDia) / (D * D) * (1.0 + 6.0 * soft) / keepN * (prFar == 1 ? prRange.w : 1.0));
        world = dPos + prRight * (c.x * D * 0.5) + prUp * ((c.y * 2.0 - 1.0) * D * 0.5);
        vUV = vec2(c.x, c.y * 2.0 - 1.0);
        vMode = 1.0;
        alpha = prOpacity * 0.9 * cover * (1.0 - 0.5 * soft);
    } else {
        float spd = length(dVel);
        vec3 dir = dVel / spd;
        float L = spd * prExposure * (prFar == 1 ? 2.0 : 1.0);
        vec3 s = cross(dir, V);
        s = length(s) > 1e-4 ? normalize(s) : prRight;
        float W = max(dDia, 1.4 * pix) * (1.0 + min(1.0, 4.0 * dLit));      // a lit streak is a little wider
        float cover = min(1.0, dDia / (W / (1.0 + min(1.0, 4.0 * dLit))) * (prFar == 1 ? prRange.w : 1.0));
        world = dPos + dir * ((0.5 - c.y) * (L + 2.0 * pix)) + s * (c.x * W * 0.5 * 1.15);
        vUV = vec2(c.x, c.y);
        vMode = 0.0;
        // Brightness spreads with the drop (a hash apart from the ones that place it), and a lamp's light thickens the streak.
        float spread = mix(0.5, 1.3, rnd(hsh(uint(dR1 * 16777216.0) + 31u)));
        alpha = prOpacity * 0.55 * cover * spread * (1.0 + min(3.0, 8.0 * dLit)) * (prFar == 1 ? 0.5 : 1.0);
    }
    vExtra = soft; vSeed = 0.0;
    vCol = vec4(col, alpha * vis);
    gl_Position = prVP * vec4(world, 1.0);
}
)";

const char* PRECIP_FS = R"(#version 330
in vec2 vUV; in vec4 vCol; in float vMode; in float vExtra; in float vSeed;
out vec4 finalColor;
void main() {
    float a = vCol.a;
    if (vMode < 0.0) discard;
    if (vMode < 0.5) {                                    // a streak: across -1..1, along 0 (head) to 1 (tail)
        float across = 1.0 - vUV.x * vUV.x;
        a *= across * across * smoothstep(0.0, 0.1, vUV.y) * (1.0 - smoothstep(0.88, 1.0, vUV.y)) * mix(1.0, 0.65, vUV.y);
    } else if (vMode < 1.5) {                             // a flake: soft and round
        float r = length(vUV);
        a *= 1.0 - smoothstep(0.25 * (1.0 - vExtra), 1.0, r);
    } else if (vMode < 2.5) {                             // a ring on the surface
        float r = length(vUV);
        float rr = 0.1 + 0.75 * sqrt(vExtra);
        float w = 0.06 + 0.04 * vExtra;
        a *= exp(-pow((r - rr) / w, 2.0)) * (1.0 - smoothstep(0.9, 1.0, r));
    } else {                                              // droplets thrown up: three arcs
        float s = 0.0;
        float u = vExtra;
        for (int i = 0; i < 4; i++) {
            float ang = (float(i) + vSeed) * 2.399;
            float sx = sin(ang) * 0.7, hh = 0.45 + 0.5 * fract(vSeed * 7.0 + float(i) * 0.37);
            vec2 p = vec2(sx * sqrt(u), 4.0 * u * (1.0 - u) * hh);
            float r = length(vUV - p);
            s += 1.0 - smoothstep(0.05, 0.10 * (1.0 - 0.4 * u), r);
        }
        a *= min(s, 1.0);
    }
    if (a < 0.004) discard;
    finalColor = vec4(pow(clamp(vCol.rgb, 0.0, 1.0), vec3(1.0 / 2.2)), a);
}
)";

} // namespace
} // namespace g3d
} // namespace rmtn
