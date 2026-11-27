// wet_glsl.hpp -- the lit shader's share of Scene::ground (wet surfaces, puddles, ripple rings, snow cover).
// Included by scene3d.cpp only; spliced into LIT_FS at //@RIPPLE_FS@ (before the water, which ripples too) and
// //@WET_FS@ (after it, which needs its sky and depth helpers). Ground::rings() in weather.cpp is the CPU twin of
// rippleSlope(): keep the two in step (the tests compare them).
#pragma once

namespace rmtn {
namespace g3d {
namespace {

const char* RIPPLE_FS = R"(
// Rain rings on still water. Cells of RIPPLE_CELL metres, six slots a cell; a slot drops one ring every
// RIPPLE_PERIOD seconds, at a hashed place in its cell and its own phase, kept when its hash is under rate / max.
const float RIPPLE_CELL = 0.75, RIPPLE_PERIOD = 1.1, RIPPLE_SPEED = 0.4, RIPPLE_WAVE = 0.05, RIPPLE_WIDTH = 0.06;
const float RIPPLE_MAX = 6.0 / (RIPPLE_PERIOD * RIPPLE_CELL * RIPPLE_CELL);
uniform float rippleRate; uniform float rippleTime; uniform int rippleSeed;
uint rhsh(uint x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
float rrnd(uint x) { return float(rhsh(x) >> 8) / 16777216.0; }
// The slope (d height / dx, dz) of the rings at ground point p. `foot` is the metres one pixel covers there: a ring
// whose wavelength is under a few pixels fades out rather than alias.
vec2 rippleSlope(vec2 p, float foot) {
    if (rippleRate <= 0.0) return vec2(0.0);
    float band = smoothstep(2.0, 6.0, RIPPLE_WAVE / max(foot, 1e-5));
    if (band <= 0.0) return vec2(0.0);
    float act = clamp(rippleRate / RIPPLE_MAX, 0.0, 1.0);
    ivec2 c0 = ivec2(floor(p / RIPPLE_CELL));
    vec2 g = vec2(0.0);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            ivec2 c = c0 + ivec2(dx, dz);
            for (int s = 0; s < 6; s++) {
                uint ck = rhsh(uint(c.x) * 73856093u ^ uint(c.y) * 19349663u ^ uint(s) * 83492791u ^ uint(rippleSeed) * 2654435761u);
                float phase = rrnd(ck) * RIPPLE_PERIOD;
                float n = floor((rippleTime - phase) / RIPPLE_PERIOD);
                float age = rippleTime - phase - n * RIPPLE_PERIOD;
                uint h = rhsh(uint(int(n)) * 2654435769u ^ ck);
                if (rrnd(h) >= act) continue;
                vec2 d = p - (vec2(c) + vec2(rrnd(h + 1u), rrnd(h + 2u))) * RIPPLE_CELL;
                float r = length(d), x = r - RIPPLE_SPEED * age;
                if (abs(x) > 2.5 * RIPPLE_WIDTH) continue;
                float life = 1.0 - age / RIPPLE_PERIOD;
                float amp = 0.5 * life * sqrt(life) * sqrt(0.05 / (r + 0.05));
                g += d / max(r, 1e-4) * (amp * exp(-(x * x) / (RIPPLE_WIDTH * RIPPLE_WIDTH)) * cos(6.2831853 * x / RIPPLE_WAVE));
            }
        }
    return g * band;
}
)";

const char* WET_FS = R"(
// ---- Scene::ground: wet surfaces, puddles and snow. All of it is behind wetOn (a uniform), so a dry frame runs none.
uniform int wetOn; uniform int wetMat;                         // the frame has a ground state; this material takes it
uniform float wetLevel; uniform float snowDepth;                // 0..1, and metres
uniform float wetPuddles; uniform float wetPuddleSize; uniform int wetSeed;
uniform int wetGrass; uniform int wetDebug;
uniform int wetOccOn; uniform mat4 wetOccVP; uniform float wetOccRange; uniform sampler2D wetOcc;   // the rain's depth map from above
const int SHELTER_TAPS = 16;
uniform int ssrPass;                                            // 1: the second draw, which blends in the scene's reflection
float gWet = 0.0, gPuddle = 0.0, gSnow = 0.0, gFilm = 0.0;     // what weatherSurface() found at this fragment

float wlat(ivec2 c) { return rrnd(rhsh(uint(c.x) * 73856093u ^ uint(c.y) * 19349663u ^ uint(wetSeed) * 2654435761u)); }
float wnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    ivec2 c = ivec2(i);
    return mix(mix(wlat(c), wlat(c + ivec2(1, 0)), f.x), mix(wlat(c + ivec2(0, 1)), wlat(c + ivec2(1, 1)), f.x), f.y);
}
float wfbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * wnoise(p); p = p * 2.03 + vec2(17.1, 9.7); a *= 0.5; }
    return s / 0.9375;
}
// 0 in the open, 1 under a roof or a tree: the rain's depth map from above, along the fall. Sixteen taps over a disc of
// radius R metres round the point, so the edge of a shelter is a ramp about 2 R wide and the map's texels do not show.
float wetShelter(vec3 p, float R) {
    if (wetOccOn == 0) return 0.0;
    float acc = 0.0;
    for (int i = 0; i < SHELTER_TAPS; i++) {
        float r = R * sqrt((float(i) + 0.5) / float(SHELTER_TAPS)), a = float(i) * 2.39996323;
        vec4 c = wetOccVP * vec4(p + vec3(cos(a) * r, 0.0, sin(a) * r), 1.0);
        vec2 uv = c.xy * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) continue;
        float zt = c.z * 0.5 + 0.5;
        ivec2 sz = textureSize(wetOcc, 0);
        vec2 ft = uv * vec2(sz) - 0.5;
        ivec2 i0 = ivec2(floor(ft));
        vec2 fr = fract(ft);
        float o00 = smoothstep(0.04, 0.15, (zt - texelFetch(wetOcc, clamp(i0, ivec2(0), sz - 1), 0).r) * wetOccRange);
        float o10 = smoothstep(0.04, 0.15, (zt - texelFetch(wetOcc, clamp(i0 + ivec2(1, 0), ivec2(0), sz - 1), 0).r) * wetOccRange);
        float o01 = smoothstep(0.04, 0.15, (zt - texelFetch(wetOcc, clamp(i0 + ivec2(0, 1), ivec2(0), sz - 1), 0).r) * wetOccRange);
        float o11 = smoothstep(0.04, 0.15, (zt - texelFetch(wetOcc, clamp(i0 + ivec2(1, 1), ivec2(0), sz - 1), 0).r) * wetOccRange);
        acc += mix(mix(o00, o10, fr.x), mix(o01, o11, fr.x), fr.y);
    }
    return acc / float(SHELTER_TAPS);
}
vec3 wetSky(vec3 r, float lod) {
    if (hasEnv == 1) return env(r, lod) * backdropScale;
    return mix(groundColor, skyColor, clamp(r.y * 0.5 + 0.5, 0.0, 1.0));
}
// The water film's Fresnel: 0.02 head on for a thin film, 0.05 for a puddle, 1 at grazing.
float wetFresnel(float c, float puddle) {
    float f0 = mix(0.02, 0.05, puddle);
    return f0 + (1.0 - f0) * pow(1.0 - c, 5.0);
}

// The share of a noise octave of this wavelength (metres) that a pixel of `foot` metres can resolve: 1 above six pixels
// a wavelength, 0 under two. What an octave loses goes into `lost` as variance, to widen the threshold it feeds.
float wband(float wavelength, float foot) { return smoothstep(2.0, 6.0, wavelength / foot); }
float wnoiseB(vec2 p, float wavelength, float foot, float amp, inout float lost) {
    float b = wband(wavelength, foot);
    lost += amp * amp * 0.05 * (1.0 - b);
    return mix(0.5, wnoise(p), b);
}

// Applies the ground to a surface point. `ng` is its geometric normal (facing the viewer), `n` the shading normal
// (a puddle's goes to the ring slope), `albedo` linear. Sets gWet, gPuddle, gSnow (each 0..1) and gFilm, the share of
// the sky a film of water mirrors. `foot`: metres a pixel covers on the ground here; every noise term is cut off by it
// and its variance goes into a wider threshold, so the far ground shows the mean cover instead of aliasing.
void weatherSurface(vec3 ng, inout vec3 n, inout vec3 albedo, float foot) {
    foot = max(foot, 1e-5);
    float R = fieldOn == 1 ? 0.3 : mix(0.15, 0.8, smoothstep(0.93, 0.99, ng.y));
    float sh = wetShelter(fragPos, R);
    float open = 1.0 - sh;
    float snow = 0.0;
    if (snowDepth > 0.0) {
        float d = snowDepth / 0.05;                               // whole at 5 cm
        if (fieldOn == 1) {                                       // grass: the tips whiten first, then the blades down
            if (wetGrass == 1) {
                float ds = clamp(snowDepth / 0.06, 0.0, 1.0);
                float tip = fragTexCoord.y;
                snow = smoothstep(1.0 - 0.95 * ds - 0.2, 1.0 - 0.95 * ds + 0.1, tip) * smoothstep(0.0, 0.2, ds);
            }
            snow *= 1.0 - 0.9 * sh;
        } else {
            float up = smoothstep(0.5, 0.9, n.y);
            vec2 q = fragPos.xz + vec2(0.5 * fragPos.y);
            q += 0.7 * (vec2(wnoise(q * 0.8 + 3.0), wnoise(q * 0.8 + 7.0)) - 0.5);    // the lattice of the noise does not show
            float lost = 0.0;
            float nz = 0.7 * wnoiseB(q * 1.7, 0.59, foot, 0.7, lost) + 0.3 * wnoiseB(q * 9.0, 0.11, foot, 0.3, lost);
            float hw = max(max(0.15, 1.7 * sqrt(lost)), 0.15 + 0.3 * sh);
            float de = d * (1.0 - 0.9 * sh);                          // under a tree or a roof: a thinner dusting
            snow = up * smoothstep(nz - hw, nz + hw, de);
        }
    }
    float s = 1.0 - ng.y;                                          // 0 flat to 1 a wall: slopes dry first
    float wE = clamp((wetLevel - 0.6 * s) / (1.0 - 0.6 * s), 0.0, 1.0) * open * (1.0 - snow);
    float pud = 0.0;
    if (wetPuddles > 0.0 && wetLevel > 0.3 && fieldOn == 0) {
        float fill = clamp((wetLevel - 0.3) / 0.7, 0.0, 1.0);
        vec2 q = fragPos.xz / wetPuddleSize;
        float lost = 0.0, dummy = 0.0;
        vec2 warp = vec2(wnoiseB(q * 2.3 + 5.0, wetPuddleSize / 2.3, foot, 0.0, dummy), wnoiseB(q * 2.3 + 9.0, wetPuddleSize / 2.3, foot, 0.0, dummy)) - 0.5;
        vec2 w = q + warp * 0.8;
        float f = 0.0, a = 0.5, k = 1.0;
        for (int i = 0; i < 4; i++) {
            f += a * wnoiseB(w, wetPuddleSize / k, foot, 0.8 * a / 0.9375, lost);
            w = w * 2.03 + vec2(17.1, 9.7); k *= 2.03; a *= 0.5;
        }
        float h = 0.8 * f / 0.9375 + 0.2 * wnoiseB(q * 7.0, wetPuddleSize / 7.0, foot, 0.2, lost);
        float hw = max(0.04, 1.7 * sqrt(lost));
        float L = 0.42 * fill * wetPuddles;                         // the level the water stands at: it rises with the rain
        pud = (1.0 - smoothstep(L - hw, L + hw, h)) * smoothstep(0.95, 0.985, ng.y) * open * (1.0 - snow);
    }
    gWet = wE; gPuddle = pud; gSnow = snow;
    gFilm = max(0.35 * wE, pud);
    albedo *= mix(1.0, 0.55, wE);                                  // damp ground is darker
    albedo *= mix(1.0, 0.65, pud);                                 // and darker still under standing water
    albedo = mix(albedo, vec3(0.80, 0.84, 0.90), snow);
    if (pud > 0.0) {
        vec2 g = rippleSlope(fragPos.xz, foot);
        n = normalize(mix(n, normalize(vec3(-g.x, 1.0, -g.y)), pud));
    }
}

// A screen-space march of the ray o + r s through the opaque picture copy (refrColor, refrDepth, drawn before this
// pass). Steps grow by 18%, 32 of them (about 80 m); a ray point is a hit where it is just behind the picture's
// surface (within a thickness that grows with distance), then bisected. rgb is the colour (encoded) there, a how
// sure the hit is (0: leaves the picture, finds nothing, or lands on the sky).
vec4 ssrTrace(vec3 o, vec3 r) {
    float s = 0.0, st = 0.05, lo = 0.0, hi = 0.0;
    bool hit = false;
    vec2 uv = vec2(0.0);
    for (int i = 0; i < 32; i++) {
        float s1 = s + st;
        vec4 c = reflVP * vec4(o + r * s1, 1.0);
        vec2 u = c.xy / max(c.w, 1e-3) * 0.5 + 0.5;
        if (c.w < 1e-3 || u.x < 0.0 || u.x > 1.0 || u.y < 0.0 || u.y > 1.0) return vec4(0.0);
        float d = refrDepthAt(u);
        if (d < 0.99999) {
            float gap = c.w - (reflVP * vec4(refrWorld(u, d), 1.0)).w;      // > 0: the ray is behind what the picture shows
            if (gap > 0.0 && gap < 0.15 + 0.12 * c.w) { lo = s; hi = s1; uv = u; hit = true; break; }
        }
        s = s1; st *= 1.18;
    }
    if (!hit) return vec4(0.0);
    for (int i = 0; i < 5; i++) {
        float mid = 0.5 * (lo + hi);
        vec4 c = reflVP * vec4(o + r * mid, 1.0);
        vec2 u = c.xy / max(c.w, 1e-3) * 0.5 + 0.5;
        float d = refrDepthAt(u);
        if (d < 0.99999 && c.w > (reflVP * vec4(refrWorld(u, d), 1.0)).w) { hi = mid; uv = u; } else lo = mid;
    }
    vec4 sc = textureLod(refrColor, refrMap.xy + uv * refrMap.zw, 0.0);
    float edge = smoothstep(0.0, 0.08, min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y)));
    return vec4(sc.rgb / max(sc.a, 1e-3), sc.a * edge);
}
)";

} // namespace
} // namespace g3d
} // namespace rmtn
