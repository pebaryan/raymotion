// clouds_glsl.hpp -- the shaders of g3d::Clouds (volumetric clouds). Included by scene3d.cpp only.
//
// The noise lives in 2D atlases (raylib has no 3D textures): a slice of the volume is a tile of the
// atlas, padded by a texel on every side with the wrapped neighbour, so a bilinear read inside a tile
// wraps by itself; the third axis is interpolated by hand between two tiles. Every read is
// textureLod(.., 0): no implicit derivatives, which are undefined in the non-uniform flow of a march
// (and which Mesa gets wrong).
#pragma once

namespace rmtn {
namespace g3d {
namespace {

// ---- baking: the shape volume (128^3: R Perlin-Worley, G B A Worley at 6, 12, 24 cells) and the
// detail volume (32^3: R G B Worley at 2, 4, 8 cells). All periodic. Run once on the GPU, lazily.
const char* CLOUD_BAKE_FS = R"(#version 330
out vec4 finalColor;
uniform int mode;                                   // 0 shape, 1 detail
uvec3 pcg3d(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
    return v;
}
int wrapi(int a, int f) { return a - f * int(floor(float(a) / float(f))); }
vec3 hash3(ivec3 c, int f, uint salt) {
    ivec3 w = ivec3(wrapi(c.x, f), wrapi(c.y, f), wrapi(c.z, f));
    uvec3 h = pcg3d(uvec3(w) + uvec3(salt * 7919u, salt * 104729u, salt * 1299709u));
    return vec3(h) * (1.0 / 4294967295.0);
}
float worley(vec3 p, int f, uint salt) {            // 1 - the distance to the nearest feature point
    vec3 x = p * float(f);
    ivec3 ic = ivec3(floor(x));
    vec3 fr = x - vec3(ic);
    float dmin = 8.0;
    for (int k = -1; k <= 1; k++)
        for (int j = -1; j <= 1; j++)
            for (int i = -1; i <= 1; i++) {
                ivec3 o = ivec3(i, j, k);
                vec3 dv = vec3(o) + hash3(ic + o, f, salt) - fr;
                dmin = min(dmin, dot(dv, dv));
            }
    return 1.0 - clamp(sqrt(dmin), 0.0, 1.0);
}
float perlin(vec3 p, int f, uint salt) {            // gradient noise, about -0.9..0.9
    vec3 x = p * float(f);
    ivec3 i0 = ivec3(floor(x));
    vec3 fr = x - vec3(i0);
    vec3 u = fr * fr * fr * (fr * (fr * 6.0 - 15.0) + 10.0);
    float n[8];
    for (int c = 0; c < 8; c++) {
        ivec3 o = ivec3(c & 1, (c >> 1) & 1, (c >> 2) & 1);
        vec3 g = hash3(i0 + o, f, salt) * 2.0 - 1.0;
        g = g / max(length(g), 1e-3);
        n[c] = dot(g, fr - vec3(o));
    }
    float x00 = mix(n[0], n[1], u.x), x10 = mix(n[2], n[3], u.x), x01 = mix(n[4], n[5], u.x), x11 = mix(n[6], n[7], u.x);
    return mix(mix(x00, x10, u.y), mix(x01, x11, u.y), u.z);
}
void main() {
    int N = mode == 0 ? 128 : 32, T = N + 2, gx = mode == 0 ? 16 : 8;
    ivec2 px = ivec2(gl_FragCoord.xy);
    ivec2 tile = ivec2(px.x / T, px.y / T), l = px - tile * T;
    int slice = tile.y * gx + tile.x;
    if (slice >= N) { finalColor = vec4(0.0); return; }
    vec3 p = (vec3(float(wrapi(l.x - 1, N)), float(wrapi(l.y - 1, N)), float(slice)) + 0.5) / float(N);
    if (mode == 0) {
        float pn = 0.5 + 0.5 * (0.5 * perlin(p, 4, 1u) + 0.3 * perlin(p, 8, 2u) + 0.2 * perlin(p, 16, 3u));
        pn = clamp((pn - 0.25) / 0.5, 0.0, 1.0);
        float w = worley(p, 5, 4u);
        finalColor = vec4(w + pn * (1.0 - w), worley(p, 6, 5u), worley(p, 12, 6u), worley(p, 24, 7u));
    } else {
        finalColor = vec4(worley(p, 2, 8u), worley(p, 4, 9u), worley(p, 8, 10u), 1.0);
    }
}
)";

// ---- shared by the march and the ground-shadow shaders: the planet shell and the density.
const char* CLOUD_COMMON = R"(
uniform sampler2D shapeTex; uniform sampler2D detailTex; uniform sampler2D weatherTex;
uniform float planetR; uniform float cloudBase; uniform float cloudTop;
uniform float cover; uniform float densityScale; uniform float detailAmt; uniform int cloudType;
uniform vec2 windOffset;            // metres the pattern has moved (x, z)
uniform vec3 slide;                 // metres the shape noise has slid (evolution)
uniform vec2 seedOffset;            // where in the tiling maps this seed looks
uniform int extruded;               // 1: the old density (2D coverage extruded by a height profile), for comparison
uniform vec2 shearDir;              // the way towers lean (downwind)
uniform float anvilOn; uniform float towerVariety;     // Clouds::anvil (1: storm cells with an anvil), Clouds::towerVariety
uniform float shafts; uniform float shellBase;                                          // rain shafts under the storm cells (0: none) and the altitude the shell starts at
uniform int stormN; uniform vec4 stormD[12]; uniform float stormT[12];     // the storm cells in the pattern frame: x, z, radius, key; and their tops (fraction of the layer)
uniform float shapeSize; uniform float detailSize; uniform float weatherSize;
const float SIGMA = 0.045;          // extinction a metre at density 1
const float MAXD = 250000.0;

uniform float nearestZ;                  // comparison switch: the nearest slice instead of the lerp of the two round
vec4 atlasTap(sampler2D tex, vec3 c, float N, float T, float gx, vec2 asz) {
    vec3 q = fract(c) * N;
    float zf = q.z - 0.5, z0 = floor(zf), zt = nearestZ > 0.5 ? floor(zf - z0 + 0.5) : zf - z0;
    float s0 = mod(z0, N), s1 = mod(z0 + 1.0, N);
    vec2 xy = q.xy + 1.0;
    vec2 o0 = vec2(mod(s0, gx), floor(s0 / gx)) * T, o1 = vec2(mod(s1, gx), floor(s1 / gx)) * T;
    return mix(textureLod(tex, (o0 + xy) / asz, 0.0), textureLod(tex, (o1 + xy) / asz, 0.0), zt);
}
float gNear = 0.0;              // 1 close to the camera: the detail gets a finer octave there (a cloud 1.5 km away shows lumps of 10 to 30 m)
float gLod = 0.0, gFar = 0.0;   // set by the march from its step: the coverage is read from a blurrier mip and the fine shape channels fade out where steps outgrow the features
// The coverage map at a level of detail, smooth. The hardware's bilinear blend has a kink on every texel edge,
// which far away (coarse levels, hundreds of metres a texel) shows as straight lines running to the vanishing
// point. Easing the fraction (smoothstep) removes the kinks but flattens the field on every texel centre line:
// those are world planes, and from a height they all pile onto the middle column of the frame, so cloud gaps ended
// along a vertical seam there. A cubic B-spline (four bilinear fetches) is smooth (C2) with no flat lines.
vec2 weatherLevel(vec2 uv, float lod) {
    float n = 256.0 / exp2(lod);
    vec2 x = uv * n - 0.5;
    vec2 i = floor(x), f = x - i;
    vec2 f2 = f * f, f3 = f2 * f, omf = 1.0 - f;
    vec2 w0 = omf * omf * omf / 6.0, w1 = (3.0 * f3 - 6.0 * f2 + 4.0) / 6.0, w2 = (-3.0 * f3 + 3.0 * f2 + 3.0 * f + 1.0) / 6.0, w3 = f3 / 6.0;
    vec2 g0 = w0 + w1, g1 = w2 + w3;
    vec2 c0 = (i - 1.0 + w1 / g0 + 0.5) / n, c1 = (i + 1.0 + w3 / g1 + 0.5) / n;
    vec2 a = g0.y * textureLod(weatherTex, vec2(c0.x, c0.y), lod).rg + g1.y * textureLod(weatherTex, vec2(c0.x, c1.y), lod).rg;
    vec2 b = g0.y * textureLod(weatherTex, vec2(c1.x, c0.y), lod).rg + g1.y * textureLod(weatherTex, vec2(c1.x, c1.y), lod).rg;
    return g0.x * a + g1.x * b;
}
vec2 weatherAt(vec2 uv) {
    float l0 = floor(gLod), fr = gLod - l0;
    vec2 a = weatherLevel(uv, l0);
    return fr < 0.01 ? a : mix(a, weatherLevel(uv, l0 + 1.0), fr);
}
// Height above the planet's surface, without the cancellation of |p| - R (R is 6 371 km).
float altOf(vec3 p) {
    float y = p.y;
    return (dot(p.xz, p.xz) + y * (y + 2.0 * planetR)) / (length(vec3(p.x, y + planetR, p.z)) + planetR);
}
// The two roots of the ray against the sphere of altitude h (stable form).
bool shell(vec3 o, vec3 d, float h, out float t0, out float t1) {
    float b = dot(o, d) + planetR * d.y;
    float c = dot(o, o) + 2.0 * planetR * (o.y - h) - h * h;
    float disc = b * b - c;
    t0 = 0.0; t1 = 0.0;
    if (disc < 0.0) return false;
    float sq = sqrt(disc);
    float q = -(b + (b >= 0.0 ? sq : -sq));
    float r1 = abs(q) > 1e-6 ? c / q : q;
    t0 = min(q, r1); t1 = max(q, r1);
    return true;
}
// The stretches of the ray inside the cloud shell (empty when .y <= .x): before the hollow under the
// layer, and after it (not if the planet is in the way).
void cloudSegments(vec3 o, vec3 d, out vec2 sa, out vec2 sb) {
    sa = vec2(0.0); sb = vec2(0.0);
    float a0, a1;
    if (!shell(o, d, cloudTop, a0, a1)) return;
    float t0 = max(a0, 0.0), t1 = min(a1, MAXD);
    if (t1 <= t0) return;
    float b0, b1;
    if (shell(o, d, shellBase, b0, b1) && b1 > 0.0) {
        sa = vec2(t0, min(t1, b0));
        float g0, g1;
        bool ground = shell(o, d, 0.0, g0, g1) && g1 > 1.0;
        if (!ground) sb = vec2(max(t0, b1), t1);
    } else sa = vec2(t0, t1);
}
// The shape of a plain 2D coverage map pushed straight up by a height gradient, kept as a comparison switch
// (Clouds::extruded): flat vertical sides, a hard surface, the same height everywhere. The tests prove the
// 3D shaping against it.
float cloudDensityOld(vec3 p, float alt, bool fine) {
    float hf = (alt - cloudBase) / (cloudTop - cloudBase);
    if (hf <= 0.0 || hf >= 1.0) return 0.0;
    vec2 wv = weatherAt((p.xz - windOffset) / weatherSize + seedOffset);
    float cov = (cloudType == 2 || cloudType == 0 ? 0.16 + 0.66 * cover + 0.18 * pow(cover, 6.0) : 0.10 + 0.68 * cover + 0.22 * pow(cover, 6.0)) * smoothstep(0.0, 0.04, cover);
    float m = clamp((cov * 1.1 - wv.r) / 0.1, 0.0, 1.0);
    float grad = smoothstep(0.0, 0.08, hf) * (1.0 - smoothstep(0.85, 1.0, hf));
    return clamp(m * grad * 3.0 - 1.0, 0.0, 1.0) * densityScale;
}

vec3 cbHash(ivec2 c) {
    uvec3 v = uvec3(uint(c.x + 100000), uint(c.y + 100000), 12345u);
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
    return vec3(v) * (1.0 / 4294967295.0);
}
// The distance, in tower radii, to the nearest cumulonimbus centre: one possible centre in each 9 km cell of a
// jittered grid (present with a probability that grows with the cover), each with its own radius of 1.2 to 2 km.
float cbRn(vec2 xz) {
    const float GS = 9000.0;
    vec2 u = xz / GS + seedOffset * 37.0;
    ivec2 i0 = ivec2(floor(u));
    float pr = clamp(cover * 3.5, 0.0, 1.0) * smoothstep(0.0, 0.04, cover), best = 9.0;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            ivec2 c = i0 + ivec2(i, j);
            vec3 h = cbHash(c);
            if (h.z > pr) continue;
            vec2 ctr = vec2(c) + 0.2 + 0.6 * h.xy;
            float r0 = 1200.0 + 800.0 * fract(h.x * 7.13 + h.y * 3.7);
            best = min(best, length((u - ctr) * GS) / r0);
        }
    return best;
}
)" R"(
// The storm cells (Clouds::anvil): few and big. Each is a broad lumpy column, about as wide as it is tall, that goes up to
// the tropopause (topHf) and there flattens into an anvil that grows OUT of the column's top: a wedge of cloud, thick against
// the tower (its underside low there, 0.66 of the height), thinner and higher downwind (0.90), fraying out along the wind
// and at its edges, mostly downwind and tens of km long. The column fills the height up to the anvil's top, so there is no
// stem and no gap. towerVariety makes the cells differ: radius, the anvil's length and width, how far it turns off the wind,
// the lumps' phase and where each looks into the shape noise. Returns the gate 0..1; the cell's noise offset, the anvil's
// axis and centre, how much of the gate is anvil, and the cell's top are returned too.
float cbShape(vec2 xz, float hf, out vec3 noff, out vec2 axis, out vec2 ctrOut, out float plateW, out float topOut) {
    float best = 0.0, v = towerVariety;
    noff = vec3(0.0); axis = shearDir; ctrOut = xz; plateW = 0.0; topOut = 0.95;
    for (int i = 0; i < 12; i++) {
        if (i >= stormN) break;
        vec4 S = stormD[i];
        vec2 d = xz - S.xy;
        float key = S.w;
        vec3 h1 = cbHash(ivec2(int(key), 31)), h2 = cbHash(ivec2(int(key), 77));
        float R = S.z * mix(1.0, 0.7 + 0.6 * h1.x, v);
        if (length(d) > R * 14.0) continue;
        float topHf = stormT[i];
        float lenK = mix(1.0, 0.55 + 1.0 * h1.y, v), widK = mix(1.0, 0.6 + 0.9 * h1.z, v), rot = (h2.x - 0.5) * 1.0 * v;
        float rn = length(d) / R;
        float lumps = 1.0 + 0.10 * v * sin(atan(d.y, d.x) * 5.0 + h2.y * 6.283 + hf * 4.0) + 0.06 * sin(atan(d.y, d.x) * 9.0 + h2.z * 6.283 - hf * 6.0);
        float tr = (1.0 + 0.2 * smoothstep(0.0, 0.3, hf)) * (1.0 - 0.38 * smoothstep(0.3, 0.9, hf)) * (lumps + 0.07 * sin(hf * 17.0 + h2.y * 6.283 + atan(d.y, d.x) * 2.0));      // a broad column that narrows up, in billows
        float gT = clamp((tr - rn) / 0.6, 0.0, 1.0);
        float ca = cos(rot), sa = sin(rot);
        vec2 ax = normalize(vec2(shearDir.x * ca - shearDir.y * sa, shearDir.x * sa + shearDir.y * ca));
        float al = dot(d, ax) / R, cr = dot(d, vec2(-ax.y, ax.x)) / R;
        float L = 4.5 * lenK;
        float t = clamp(al / L, 0.0, 1.0);
        float under = mix(0.66, 0.90, t * t * (3.0 - 2.0 * t));                           // thick against the tower, thinner and higher downwind
        float top = topHf - 0.03 * t;
        float vert = smoothstep(under - 0.05 + 0.025 * sin(al * 8.0 + 1.3) * sin(cr * 9.0), under + 0.09, hf) * (1.0 - smoothstep(top - 0.03, top + 0.01, hf));
        float wp = 1.15 + 1.2 * widK * smoothstep(0.0, 0.6 * L, al);                        // a wedge: it widens downwind
        float lat = 1.0 - smoothstep(0.45, 1.0, abs(cr) / wp);
        float lon = smoothstep(-1.0, -0.2, al) * (1.0 - smoothstep(0.45, 1.0, t));       // and frays out towards its far end
        float gP = vert * lat * lon * (1.0 - 0.45 * t);
        float g = max(gT, gP);
        if (g > best) {
            best = g; axis = ax; ctrOut = S.xy; plateW = gP > gT ? gP : 0.0; topOut = topHf;
            noff = (h2 * 2.0 - 1.0) * (0.5 * v);
        }
    }
    return best;
}
// Half the width of a cumulonimbus at height hf (as a multiple of the tower footprint): a broad base, a tower
// narrowing slowly, a flare into the anvil that spreads from about 0.75 of the height and flattens into a plate,
// and the top. Continuous in hf, so there is no gap between the tower and the plate.
float cbRadius(float hf) {
    float r = mix(1.3, 0.95, smoothstep(0.03, 0.72, hf));                                                      // base to tower
    r = mix(r, 2.1, smoothstep(0.68, 0.83, hf));                                                              // the flare into the anvil
    return r * (1.0 - smoothstep(0.955, 0.995, hf));                                                         // the plate's top
}
// ---- the shaping in 3D (Schneider's scheme): the coverage map only gates where clouds may form; the
// low-frequency Perlin-Worley shape, remapped by the local coverage and a height profile, draws the boundary.
float profileOf(float hf, float depthv, float wg) {
    if (cloudType == 1) return smoothstep(0.0, 0.10, hf) * (1.0 - smoothstep(0.16, 0.30, hf));                // stratus: a thin sheet by the base
    if (cloudType == 2) {                                                                                      // cumulonimbus: one tower and its anvil, a vertical envelope; the width is in cbRadius
        return smoothstep(0.0, 0.05, hf) * (1.0 - smoothstep(0.95, 1.0, hf));
    }
    if (cloudType == 3) return smoothstep(0.0, 0.14, hf) * (1.0 - smoothstep(0.28, 0.50, hf));                // deck: flat top
    float ht = 0.3 + 0.7 * depthv * (0.7 + 0.3 * wg);                                                          // cumulus: taller in the middle,
    float r = clamp(1.0 - hf / ht, 0.0, 1.0);
    return smoothstep(0.0, 0.12, hf) * r * (2.0 - r);                                                        // a soft base and a rounded top (finite slope at the top: no hard rim)
}
// The shape noise, remapped by a gate, a height profile and the cloud's own coverage, then eroded; shared by the
// cumulonimbus storm cells and the cumulus between them.
float cloudBody(vec3 pos, float hf, float covL, float prof, float plate, float core, float gainK, bool fine, float rag) {
    vec3 pq = pos + slide;
    if (fine && detailAmt > 0.0) {                                                                             // curl: billows
        vec3 cv = atlasTap(detailTex, (pos + slide * 2.5) / (detailSize * 4.0) + vec3(0.21, 0.64, 0.05), 32.0, 34.0, 8.0, vec2(272.0, 136.0)).rgb - 0.5;
        pq += cv * vec3(1.0, 0.4, 1.0) * (600.0 * detailAmt);
    }
    vec4 s = atlasTap(shapeTex, pq / shapeSize, 128.0, 130.0, 16.0, vec2(2080.0, 1040.0));
    float fbm = mix(s.g * 0.625 + s.b * 0.25 + s.a * 0.125, 0.4, gFar);                                       // far away the fine channels average out
    float base = clamp((s.r + 1.0 - fbm) / (2.0 - fbm), 0.0, 1.0);
    base = mix(mix(base, 0.6, plate * 0.55), 0.82, core);                                                      // the anvil is smoother than the billows; a storm cell is solid inside, ragged only at its rim
    float lo = 1.0 - covL * 0.95;
    float d = clamp((base * prof - lo) / (1.0 - lo), 0.0, 1.0) * covL;                                         // remap(shape x profile, 1 - coverage, 1, 0, 1)
    if (fine && d > 0.0) {
        vec3 ds = atlasTap(detailTex, (pos + slide * 2.5) / detailSize + vec3(0.37, 0.11, 0.73), 32.0, 34.0, 8.0, vec2(272.0, 136.0)).rgb;
        vec3 d2 = atlasTap(detailTex, (pos + slide * 2.5) / (detailSize * 0.3) + vec3(0.61, 0.83, 0.29), 32.0, 34.0, 8.0, vec2(272.0, 136.0)).rgb;   // a finer octave: billows close up
        float df = mix(ds.r * 0.625 + ds.g * 0.25 + ds.b * 0.125, d2.r * 0.6 + d2.g * 0.4, 0.4);
        df = clamp(0.55 + (df - 0.55) * mix(1.0, 5.0, gNear), 0.0, 1.0);                                       // close up the erosion needs contrast: the sum of Worley octaves is a flat grey, and a cloud 1.5 km away would show none of it
        float e = mix(df, 1.0 - df, clamp(hf * 4.0, 0.0, 1.0)) * detailAmt * 0.85 * (1.0 - 0.6 * plate) * (1.0 + (rag > 0.0 ? 1.1 * (1.0 - smoothstep(0.0, 450.0, hf * rag)) : 0.0));       // wispy bottoms: inverted near the base, and strongest there (the base is eaten into shreds and lumps)
        e = min(e, 0.92);
        d = clamp((d - e) / (1.0 - e), 0.0, 1.0);
    }
    d = d * d * (2.5 - 1.5 * d);                                                                              // a toe with no slope at the boundary: the edge fades over metres, not a pixel
    return d * densityScale * gainK;
}
)" R"(
// How far the base of a cumulus is lifted at this place, in metres (Schneider's height gradient, modulated by noise): the
// condensation level is level, but the cloud above it is eroded unevenly, so the base is a field of low lumps and high bays, not a plane. The
// height gradient then starts at the lift rather than at 0, and the profile's soft toe makes the edge of the base a ramp over tens of metres.
float baseBay(vec3 pos, float scale) {
    vec4 s = atlasTap(shapeTex, vec3(pos.x + slide.x, 0.0, pos.z + slide.z) / (shapeSize * scale) + vec3(0.13, 0.5, 0.71), 128.0, 130.0, 16.0, vec2(2080.0, 1040.0));
    float lump = s.r * 0.45 + s.g * 0.35 + s.b * 0.2;                                                          // 0..1, mean about 0.5
    return smoothstep(0.47, 0.75, lump);                                                                       // 0 in the pendant lumps, 1 in the bays
}
float baseLift(vec3 pos, float h) {
    if (h > 700.0) return 0.0;
    float bay = baseBay(pos, 0.55);
    return 100.0 + 200.0 * bay + 70.0 * baseBay(pos, 0.22);                                                   // lumps, and an undulation a kilometre across that still shows at range
}
// The light of a cumulus underside, mottled: the pendant lumps hang in their own shade, the bays between them catch the light of the sky,
// and a finer pattern breaks up each. 1 above 700 m over the base.
float baseShade(vec3 pos, float h) {
    if (h > 700.0) return 1.0;
    float m = mix(0.45, 1.2, baseBay(pos, 0.55)) * mix(0.75, 1.1, baseBay(pos, 0.22)) * mix(0.74, 1.1, baseBay(pos + vec3(311.0, 0.0, 97.0), 0.13));
    return mix(m, 1.0, smoothstep(150.0, 700.0, h));
}
// The base of a storm cell is lowered and lumpy: lumps of hundreds of metres, in metres above the layer's base.
float cbLift(vec3 pos) {
    return 120.0 + 330.0 * baseBay(pos, 0.30) + 70.0 * baseBay(pos + vec3(57.0, 0.0, 301.0), 0.17);
}
float hash1(float n) { return fract(sin(n * 12.9898) * 43758.5453); }
// Rain shafts: below a storm cell's base, one or two columns of fine rain, slanted downwind, streaky, thinning to the ground. `q` is in the
// pattern's frame. Extinction is small (a veil, not a wall), and the sun and the sky dim it by the cloud above.
float rainShaft(vec2 q, float alt) {
    float v = cloudBase - alt, sum = 0.0;
    for (int i = 0; i < 12; i++) {
        if (i >= stormN) break;
        vec4 S = stormD[i];
        for (int k = 0; k < 2; k++) {
            float a = 6.2831853 * hash1(S.w + 7.31 * float(k)), rr = S.z * (0.15 + 0.35 * hash1(S.w * 1.7 + 3.0 * float(k)));
            vec2 c = S.xy + vec2(cos(a), sin(a)) * rr + shearDir * (v * 0.35);
            float rs = S.z * (0.22 + 0.12 * hash1(S.w * 2.3 + float(k)));
            float w = 1.0 - smoothstep(0.25 * rs, rs, length(q - c));
            sum += w * (0.45 + 0.9 * baseBay(vec3(q.x, 0.0, q.y), 0.045));
        }
    }
    return sum * shafts * 0.03 * mix(1.0, 0.45, smoothstep(0.0, 900.0, v)) * smoothstep(0.0, 40.0, alt);
}
// Scud, fractus and pannus: torn puffs in the gap under a lifted base, from 3D noise sampled near-isotropically (the height is stretched
// so a puff is flat) and long along the wind, at varied heights, with gaps through which the darker base shows. `gate` is the cloud above.
float scudBelow(vec3 pb, float hm, float lift, float gate, float strength) {
    if (hm > lift + 120.0 || gate <= 0.0) return 0.0;
    vec2 wd = shearDir, pd = vec2(-wd.y, wd.x);
    vec3 pq = vec3(dot(pb.xz, wd) * 0.5, pb.y * 1.6, dot(pb.xz, pd) * 0.8);
    vec3 dq = atlasTap(detailTex, (pq + slide * 2.5) / (detailSize * 1.7) + vec3(0.2, 0.7, 0.4), 32.0, 34.0, 8.0, vec2(272.0, 136.0)).rgb;
    float gap = baseBay(pb, 0.2), thr = 0.50 + 0.14 * (1.0 - gap);
    float wisp = smoothstep(thr, thr + 0.2, dq.r * 0.6 + dq.g * 0.4);
    float env = smoothstep(0.0, 40.0, hm) * (1.0 - smoothstep(0.25 * lift, lift + 120.0, hm));
    return gate * env * wisp * strength;
}
// Density 0..1 (times densityScale) at p, altitude alt. `fine` adds the curl and the detail erosion.
float cloudDensity(vec3 p, float alt, bool fine) {
    float thickM = cloudTop - cloudBase;
    if (extruded == 1) return cloudDensityOld(p, alt, fine);
    float hf = (alt - cloudBase) / (cloudTop - cloudBase);
    if (hf <= 0.0 && shafts > 0.0 && cloudType == 2 && anvilOn > 0.5 && alt > 0.0) return rainShaft(p.xz - windOffset, alt);
    if (hf <= 0.0 || hf >= 1.0) return 0.0;
    bool storms = cloudType == 2 && anvilOn > 0.5;
    vec2 lean = cloudType == 2 ? shearDir * (hf * hf * (storms ? 0.0 : 0.25) * (cloudTop - cloudBase)) : vec2(0.0);   // towers lean downwind
    vec2 q2 = p.xz - lean;
    vec2 wv = weatherAt((q2 - windOffset) / weatherSize * (cloudType == 2 ? 0.9 : 1.0) + seedOffset);        // a cumulonimbus is a few big cells
    float soft = cloudType == 2 ? 0.16 : 0.55;                                                                                   // wide: the gate is not the boundary
    float cov = (cloudType == 2 || cloudType == 0 ? 0.16 + 0.66 * cover + 0.18 * pow(cover, 6.0) : 0.10 + 0.68 * cover + 0.22 * pow(cover, 6.0)) * smoothstep(0.0, 0.04, cover);       // fitted: the share of the sky seen under cloud from below equals cover (weather_tests)
    float g = clamp((cov * (1.0 + soft) - wv.r) / soft, 0.0, 1.0);
    float depthv = clamp((cov - wv.r) / max(cov, 0.05), 0.0, 1.0);
    float gain = 1.0, narrow = 0.45;
    if (cloudType == 1) { gain = 0.55; narrow = 0.0; }
    if (cloudType == 3) { narrow = 0.0; }
    if (storms) {
        // A few storm cells (sparse, from the CPU) with the ordinary cumulus between them, which is what a storm's cover is.
        vec3 noff; vec2 axis, ctr; float plate, topHf;
        float gs = cbShape(q2 - windOffset, hf, noff, axis, ctr, plate, topHf);
        float dS = 0.0;
        if (gs > 0.02) {
            vec3 pb = vec3(q2.x - windOffset.x, alt, q2.y - windOffset.y);
            float lift = hf * thickM < 1500.0 ? cbLift(pb) * (1.0 - 0.25 * gFar) : 0.0;
            float prof = smoothstep(0.0, 0.05, hf - lift / thickM) * (1.0 - smoothstep(topHf - 0.05 - 0.10 * (1.0 - plate) * clamp(length(q2 - windOffset - ctr) / 7000.0, 0.0, 1.0), topHf, hf));   // a domed top
            vec3 pos = vec3(q2.x - windOffset.x, alt, q2.y - windOffset.y) + noff * shapeSize;
            vec2 rel = pos.xz - ctr;
            pos.xz -= axis * dot(rel, axis) * (0.62 * plate);                                                  // the anvil's texture is stretched downwind: fibres
            dS = cloudBody(pos, hf, gs, prof, plate, (1.0 - plate) * smoothstep(0.55, 1.0, gs) * 0.8, 1.5, fine, plate < 0.5 ? thickM * 0.12 : 0.0);
            float hm = hf * thickM;
            if (plate < 0.5) dS = max(dS, scudBelow(pb, hm, lift, gs, 0.4 * densityScale));      // scud and fractus under the lifted base
        }
        float hfc = hf / 0.22, dF = 0.0;                                                                       // the cumulus fill: the lowest fifth of the layer
        if (hfc < 1.0) {
            float fcov = clamp(0.05 + 1.6 * cover, 0.0, 0.6) * smoothstep(0.0, 0.03, cover);
            float gF = clamp((fcov * 1.55 - wv.r) / 0.55, 0.0, 1.0);
            float dpF = clamp((fcov - wv.r) / max(fcov, 0.05), 0.0, 1.0);
            float ht = 0.3 + 0.7 * dpF * (0.7 + 0.3 * wv.g), r = clamp(1.0 - hfc / ht, 0.0, 1.0);
            float hfl = hfc;
            float profF = smoothstep(0.0, 0.12, hfl) * r * (2.0 - r);
            float covF = gF * (1.0 - 0.45 * smoothstep(0.15, 1.0, hfc));
            if (covF > 0.02 && profF > 0.0) dF = cloudBody(vec3(q2.x - windOffset.x, alt, q2.y - windOffset.y), hfc, covF, profF, 0.0, 0.0, 0.8, fine, thickM * 0.22);
        }
        return max(dS, dF);
    }
    float hfp = hf;
    if (cloudType == 0) hfp = hf - baseLift(vec3(q2.x - windOffset.x, alt, q2.y - windOffset.y), hf * thickM) * (1.0 - 0.25 * gFar) / thickM;   // a ragged base (far away the lumps are sub-pixel)
    float stratLift = 0.0, scud = 0.0;
    vec3 pw = vec3(q2.x - windOffset.x, alt, q2.y - windOffset.y);
    if (cloudType == 1) {
        // A stratus or nimbostratus sheet is not even: soft cells and rolls (long along the wind), so the thickness and the light under it vary
        // and show in perspective; the base is relieved; pannus (scud) hangs under it.
        vec2 wd = shearDir, pd = vec2(-wd.y, wd.x);
        float cells = baseBay(vec3(dot(pw.xz, wd) * 0.3, 0.0, dot(pw.xz, pd)), 0.45);
        float finer = baseBay(pw + vec3(713.0, 0.0, 211.0), 0.12);
        gain *= mix(0.45, 1.3, cells) * mix(0.8, 1.12, finer);
        stratLift = (30.0 + 170.0 * baseBay(pw, 0.3)) * (1.0 - 0.25 * gFar);
        hfp = hf - stratLift / thickM;
        scud = scudBelow(pw, hf * thickM, stratLift, g, 0.5 * densityScale);
    }
    float prof = profileOf(hfp, depthv, wv.g);
    float covL = g * (1.0 - narrow * smoothstep(0.15, 1.0, hf)) * (cloudType == 3 ? 0.92 : 1.0);              // the higher, the narrower: rounded tops
    if (cloudType == 2) {
        gain = 1.5; narrow = 0.0;
        float rn = cbRn(q2 - windOffset);
        covL = clamp((cbRadius(hf) - rn) / 0.9, 0.0, 1.0);
    }
    if (covL <= 0.02 || prof <= 0.0) return scud;
    return max(scud, cloudBody(pw, hf, covL, prof, 0.0, 0.0, gain, fine, cloudType == 0 ? thickM : 0.0));
}
float ign(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }
)";

// ---- the march: one pixel of the low-resolution cloud image. Output: premultiplied linear radiance
// (rgb) and 1 - transmittance (a).
const char* CLOUD_MARCH_FS = R"(#version 330
out vec4 finalColor;
uniform vec2 lowSize;
uniform vec3 camPos; uniform vec3 camFwd; uniform vec3 camRight; uniform vec3 camUp; uniform vec2 tanHalf;
uniform sampler2D depthTex; uniform vec2 vpOrigin; uniform vec2 vpSize; uniform vec2 zNF;
uniform vec3 sunDir; uniform vec3 sunE; uniform vec3 ambTop; uniform vec3 ambBot; uniform vec3 horizonCol;
uniform vec3 hazeCol; uniform float hazeAmount; uniform float airK;
uniform int steps; uniform int lightSteps; uniform float jitterOn; uniform float denoiseOn; uniform sampler2D blueTex;
uniform float waterLevel; uniform int waterOn;
uniform vec3 flashPos; uniform vec3 flashCol; uniform float flashRadius;
uniform vec3 boltA; uniform vec3 boltB; uniform vec3 boltCol; uniform float boltRadius;     // a lightning channel inside the cloud: its ends and its light (0: none)
COMMON
float hg(float c, float g) { return (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * c, 1.5); }   // 1 = isotropic
float lightDepth(vec3 p, vec3 s) {                  // optical depth from p to the sun
    float tau = 0.0, t = 0.0, dt = (cloudTop - cloudBase) * 0.03;
    for (int i = 0; i < 8; i++) {
        if (i >= lightSteps) break;
        t += dt * 0.5;
        vec3 q = p + s * t;
        tau += cloudDensity(q, altOf(q), i < 3) * SIGMA * dt;     // the first three with the detail, so crevices shade
        t += dt * 0.5;
        dt *= 2.1;
    }
    return tau;
}
float upDepth(vec3 p, float alt) {                  // optical depth straight up through the whole column above p (sky light)
    vec3 up = normalize(vec3(p.x, p.y + planetR, p.z));
    float rem = max(cloudTop - alt, 1.0), tau = 0.0, prev = 0.0;
    for (int i = 0; i < 4; i++) {
        float fr = i == 0 ? 0.08 : (i == 1 ? 0.25 : (i == 2 ? 0.55 : 1.0));
        float mid = 0.5 * (prev + fr);
        tau += cloudDensity(p + up * (rem * mid), alt + rem * mid, false) * SIGMA * rem * (fr - prev);
        prev = fr;
    }
    return tau;
}
// Two rates: coarse steps (growing with distance) through the empty parts, fine steps (0.35 of them)
// once a sample finds cloud, back to coarse after six empty ones. The coarse phase is jittered per pixel.
void march(vec3 o, vec3 d, vec2 seg, float jit, float cosT, inout vec3 L, inout float T, inout float wsum, inout float wdist) {
    if (seg.y <= seg.x) return;
    float thick = cloudTop - cloudBase;
    float dt0 = thick / (float(steps) * 0.3);
    float t = seg.x - (1.0 - jit) * max(dt0, min(0.05 * seg.x, 1500.0));   // the jitter spans the coarse step where the ray starts (it was dt0, 70 m, against steps of 1.5 km far away: every pixel of a row sampled the same lattice, and the edges combed into whiskers)
    bool fine = false;
    float tLast = seg.x;                                  // the last empty coarse sample
    int zeros = 0, cap = int(float(steps) * 3.0);
    float rhoPrev = 1.0;
    float ph = mix(hg(cosT, -0.25), hg(cosT, 0.85), 0.35);
    for (int i = 0; i < 700; i++) {
        if (i >= cap || T < 0.01 || t >= seg.y) break;
        float dtc = max(dt0, min(0.05 * t, 1500.0));
        float dt = fine ? clamp(0.6 / (SIGMA * max(rhoPrev, 0.05)), 5.0, dtc * 0.35) : dtc;   // fine: about half an optical depth a step
        float rr = jitterOn < 0.5 ? 0.5 : fract(jit + 0.6180339 * float(i));       // each fine step samples at its own place, so the lattice does not ring
        float tm = t + dt * (fine ? rr : 0.5);
        gLod = clamp(log2(max(dt, 1.0) / 94.0), 0.0, 2.0);
        gFar = smoothstep(12000.0, 45000.0, tm);
        gNear = 1.0 - smoothstep(2500.0, 7000.0, tm);
        if (tm < seg.x) { t += dt; continue; }
        float tEnd = min(t + dt, seg.y);
        vec3 p = o + d * tm;
        float alt = altOf(p);
        float rho = cloudDensity(p, alt, tm < 40000.0);
        if (!fine) {
            if (rho > 0.002) {
                // Find where the cloud begins between the last empty sample and this one (bisection), and
                // start the fine steps there, their phase jittered: the entry is not quantised to a ring.
                float lo = tLast, hi = tm;
                for (int k = 0; k < 5; k++) {
                    if (jitterOn < 0.5) break;                     // the plain lattice (jitter off): no refinement, so steps band
                    float mid = 0.5 * (lo + hi);
                    vec3 q = o + d * mid;
                    if (cloudDensity(q, altOf(q), false) > 0.002) hi = mid; else lo = mid;
                }

                t = hi - (1.0 - jit) * 13.0;                       // the first fine step is 13 m (half an optical depth)
                rhoPrev = 1.0;
                fine = true; zeros = 0;
                continue;
            }
            tLast = tm;
            t += dt;
            continue;
        }
        if (rho <= 0.002) { if (++zeros >= 6) { fine = false; tLast = tm; } t += dt; continue; }
        zeros = 0;
        float hf = clamp((alt - cloudBase) / thick, 0.0, 1.0);
        float sigT = rho * SIGMA;
        float tauL = lightDepth(p, sunDir);
        float sunSum = 0.0, a = 1.0, b = 1.0, c = 1.0;
        for (int k = 0; k < 3; k++) {                                                   // octaves of multiple scattering
            float phk = k == 0 ? ph : mix(hg(cosT, -0.25 * c), hg(cosT, 0.85 * c), 0.35);
            sunSum += a * exp(-tauL * b) * phk;
            a *= 0.5; b *= 0.5; c *= 0.5;
        }
        float powder = 1.0 - exp(-rho * 7.0);
        sunSum *= mix(1.0, powder, 0.55 * (1.0 - max(cosT, 0.0)));
        float tauU = upDepth(p, alt);
        vec3 amb = mix(ambBot, ambTop, smoothstep(0.0, 1.0, pow(hf, 0.7))) * (0.07 + 0.83 * exp(-tauU * 0.9) + 0.14 * exp(-tauU * 0.14));   // dark under the thick middle (much cloud above), light where it thins: two rates, so a base is not one grey
        amb *= 1.0 + 0.9 * exp(-rho * 3.0);                                              // thin spots let the sky light through: a lumpy base is lighter where it thins
        vec3 S = sunE * sunSum + amb;
        if (cloudType == 0) S *= baseShade(vec3(p.x - windOffset.x, 0.0, p.z - windOffset.y), alt - cloudBase);
        if (flashCol.r + flashCol.g + flashCol.b > 0.0) {
            vec3 dp = p - flashPos;
            S += flashCol / (1.0 + dot(dp, dp) / (flashRadius * flashRadius));
        }
        if (boltCol.r + boltCol.g + boltCol.b > 0.0) {
            // The channel lights the cloud from inside: each sample takes the light of the nearest point of the channel,
            // dimmed by the cloud between them (four density taps), so a cloud shines round the bolt and its thin edges glow.
            vec3 ab = boltB - boltA;
            vec3 q = boltA + ab * clamp(dot(p - boltA, ab) / max(dot(ab, ab), 1.0), 0.0, 1.0);
            vec3 dq = q - p;
            float dd = length(dq);
            if (dd < boltRadius * 6.0) {
                float tauB = 0.0, dsb = dd * 0.25;
                for (int k = 0; k < 4; k++) {
                    vec3 pk = p + dq * ((float(k) + 0.5) * 0.25);
                    tauB += cloudDensity(pk, altOf(pk), false) * SIGMA * dsb;
                }
                float att = exp(-tauB * 0.10) + 0.35 * exp(-tauB * 0.03);               // light scattered round the dense parts as well
                S += boltCol * att / (1.0 + dd * dd / (boltRadius * boltRadius));
            }
        }
        float tr = exp(-sigT * (tEnd - max(t, seg.x)));
        float w = T * (1.0 - tr);
        L += S * w;
        wsum += w; wdist += w * tm;
        T *= tr;
        rhoPrev = rho;
        t += dt;
    }
}
void main() {
    vec2 uv = gl_FragCoord.xy / lowSize, ndc = uv * 2.0 - 1.0;
    vec3 d = normalize(camFwd + camRight * (ndc.x * tanHalf.x) + camUp * (ndc.y * tanHalf.y));
    float dep = texelFetch(depthTex, ivec2(vpOrigin + uv * vpSize), 0).r;
    float tMax = 1e9;
    if (dep < 0.99999) {
        float z = 2.0 * zNF.x * zNF.y / (zNF.y + zNF.x - (dep * 2.0 - 1.0) * (zNF.y - zNF.x));
        tMax = z / dot(d, camFwd);
    }
    if (waterOn == 1 && camPos.y > waterLevel && d.y < 0.0) tMax = min(tMax, (waterLevel - camPos.y) / d.y);
    vec2 sa, sb;
    cloudSegments(camPos, d, sa, sb);
    sa.y = min(sa.y, tMax); sb.y = min(sb.y, tMax);
    float la = max(sa.y - sa.x, 0.0), lb = max(sb.y - sb.x, 0.0);
    float T = 1.0, wsum = 0.0, wdist = 0.0;
    vec3 L = vec3(0.0);
    if (la + lb > 0.0) {
        float jit = jitterOn < 0.5 ? 0.5 : (denoiseOn > 0.5 ? (texelFetch(blueTex, ivec2(gl_FragCoord.xy) & 63, 0).r * 255.0 + 0.5) / 256.0 : ign(gl_FragCoord.xy));
        float cosT = dot(d, sunDir);
        if (la > 0.0) march(camPos, d, sa, jit, cosT, L, T, wsum, wdist);
        if (lb > 0.0) march(camPos, d, sb, jit, cosT, L, T, wsum, wdist);
        float dist = wsum > 0.0 ? wdist / wsum : 0.0;
        float f = 1.0 - exp(-dist * airK);
        vec3 side = normalize(vec3(d.x, 0.0, d.z) + 1e-5);
        float sideAmt = pow(max(dot(side, normalize(vec3(sunDir.x, 0.0, sunDir.z) + 1e-5)), 0.0), 3.0);
        vec3 fogCol = mix(horizonCol, hazeCol, hazeAmount * sideAmt * 0.6);
        L = L * (1.0 - f) + fogCol * f * (1.0 - T);
    }
    finalColor = vec4(L, 1.0 - T);
}
)";

// ---- the ground shadow map: the sun's transmittance from each point of the plane y = shY.
const char* CLOUD_SHADOW_FS = R"(#version 330
out vec4 finalColor;
uniform vec2 shSize; uniform vec2 shCentre; uniform float shExtent; uniform float shY;
uniform vec3 sunDir; uniform int shSteps;
COMMON
void main() {
    vec2 uv = gl_FragCoord.xy / shSize;
    vec3 o = vec3(shCentre.x + (uv.x - 0.5) * shExtent, shY, shCentre.y + (uv.y - 0.5) * shExtent);
    vec2 sa, sb;
    cloudSegments(o, sunDir, sa, sb);
    float tau = 0.0, jit = ign(gl_FragCoord.xy);
    float la = max(sa.y - sa.x, 0.0), lb = max(sb.y - sb.x, 0.0);          // from the ground the layer is the second stretch
    for (int k = 0; k < 2; k++) {
        vec2 sg = k == 0 ? sa : sb;
        float len = k == 0 ? la : lb;
        if (len <= 0.0) continue;
        float dt = len / float(shSteps);
        for (int i = 0; i < 40; i++) {
            if (i >= shSteps) break;
            vec3 p = o + sunDir * (sg.x + (float(i) + jit) * dt);
            tau += cloudDensity(p, altOf(p), false) * SIGMA * dt;
        }
    }
    float T = exp(-tau * 0.8);
    finalColor = vec4(T, T, T, 1.0);
}
)";

// ---- the depth-aware upsample: four low-resolution taps weighted by bilinear weight over the
// relative difference between their depth and this pixel's. Output: straight sRGB colour and alpha.
const char* CLOUD_UP_FS = R"(#version 330
out vec4 finalColor;
uniform sampler2D lowTex; uniform sampler2D depthTex;
uniform vec2 lowSize; uniform vec2 vpOrigin; uniform vec2 vpSize; uniform vec2 zNF;
float linz(float d) {
    if (d >= 0.99999) return 1e7;
    return 2.0 * zNF.x * zNF.y / (zNF.y + zNF.x - (d * 2.0 - 1.0) * (zNF.y - zNF.x));
}
uniform float denoiseOn;
// A 5 x 5 bilateral filter over the march's pixels round this one: weighted by distance, by how close the
// opaque depth under each tap is to this pixel's (so a cloud never leaks over a silhouette) and by how close
// its alpha is (so the filter does not wash a thin wisp over empty sky). It averages away the per-pixel
// noise of the march and nothing else: there is no temporal accumulation. A pixel with no cloud in its
// 3 x 3 neighbourhood stays exactly empty, so the sun and the sky are never touched.
void main() {
    vec2 uv = gl_FragCoord.xy / vpSize;
    ivec2 n = ivec2(clamp(floor(uv * lowSize), vec2(0.0), lowSize - 1.0));
    vec4 cC = texelFetch(lowTex, n, 0);
    vec4 c = cC;
    if (denoiseOn > 0.5) {
        float zf = linz(texelFetch(depthTex, ivec2(vpOrigin + gl_FragCoord.xy), 0).r);
        vec4 acc = vec4(0.0); float wsum = 0.0, maxA = 0.0;
        for (int j = -2; j <= 2; j++)
            for (int i = -2; i <= 2; i++) {
                ivec2 ij = clamp(n + ivec2(i, j), ivec2(0), ivec2(lowSize) - 1);
                vec4 t = texelFetch(lowTex, ij, 0);
                if (abs(i) <= 1 && abs(j) <= 1) maxA = max(maxA, t.a);
                float zl = linz(texelFetch(depthTex, ivec2(vpOrigin + (vec2(ij) + 0.5) / lowSize * vpSize), 0).r);
                float rel = min(abs(zf - zl) / max(min(zf, zl), 1.0), 4.0);
                float w = exp(-float(i * i + j * j) / 3.4) * exp(-abs(t.a - cC.a) * 0.7) / (0.02 + rel);
                acc += t * w;
                wsum += w;
            }
        c = acc / wsum;
        if (cC.a < 1e-4 && maxA < 0.02) c = vec4(0.0);
    }
    if (c.a < 1e-4) { finalColor = vec4(0.0); return; }
    vec3 straight = clamp(c.rgb / c.a, 0.0, 1.0);
    finalColor = vec4(pow(straight, vec3(1.0 / 2.2)), min(c.a, 1.0));
}
)";

} // namespace
} // namespace g3d
} // namespace rmtn
