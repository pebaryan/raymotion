// wake_glsl.hpp -- the lit shader's share of Scene::wakes and Scene::ripples (Kelvin wakes with foam, rings from
// dropped stones). Included by scene3d.cpp only; spliced into LIT_FS at //@WAKE_FS@ (after the water's helpers,
// vnoise among them, before the water's surface, which calls wakeField and rippleField). WakeField::sum and
// Ripple::radial in wake.cpp are the CPU twins of wakeField and rippleField: keep the two in step (the tests
// compare them).
#pragma once

namespace rmtn {
namespace g3d {
namespace {

const char* WAKE_FS = R"(
// ---- wakes and ripples. All of it is behind wakeCount / rippleCount (uniforms), so a frame without them runs none.
const int WAKE_MAX = 4, WAKE_N = 64, WK_RIPPLES = 16;
const float WK_G = 9.81, WK_SIGMA = 7.28e-5, WK_SLOWEST = 144.38;
uniform int wakeCount;
uniform vec4 wakeA[WAKE_MAX];            // hull length, beam, speed, wave amplitude
uniform vec4 wakeB[WAKE_MAX];            // life (s), foam, trail (s), 0
uniform vec4 wakeBoat[WAKE_MAX];         // the boat now: x, z, heading x, heading z (0, 0: not under way)
uniform vec4 wakeE[WAKE_MAX * WAKE_N];   // the emission points: x, z, heading x, heading z (0, 0: no boat then)
uniform int rippleCount;
uniform vec4 rippleA[WK_RIPPLES];        // x, z, seconds since the drop, strength
uniform vec4 rippleB[WK_RIPPLES];        // size, life, 0, 0

// Noise of zero mean in three bands (1.8, 0.7, 0.3 m), each dropped when it is under a few pixels.
float wkNoise(vec2 q, float foot) {
    float n = 0.0, L = 1.8, a = 1.0;
    mat2 turn = mat2(0.8, 0.6, -0.6, 0.8);          // each band turned off the last, so the lattice does not show
    for (int o = 0; o < 3; o++) {
        float bl = foot > 0.0 ? smoothstep(2.0, 6.0, L / foot) : 1.0;
        n += bl * a * (vnoise(q / L + float(o) * 7.3) - 0.5);
        q = turn * q;
        L *= 0.4; a *= 0.6;
    }
    return 0.5 + n * 1.1;
}

// The slope of every wake's waves is added to g; the foam (0..1) is raised to the wake's, if more.
void wakeField(vec2 p, float foot, inout vec2 g, inout float foam) {
    for (int w = 0; w < WAKE_MAX; w++) {
        if (w >= wakeCount) break;
        vec4 A = wakeA[w], B = wakeB[w], boat = wakeBoat[w];
        float life = B.x, U = A.z, beam = A.y;
        float eps = 0.5 * beam + 0.3, eps2 = eps * eps, k0 = WK_G / (U * U);
        vec2 gs = vec2(0.0);
        float trail = 0.0, bestS = 0.0, bestB = 0.0;
        // Twice as many emission points as the table has (the middle of two neighbours: positions and headings averaged),
        // so the sum stays smooth where the phase moves a radian or more from one table entry to the next.
        for (int jj = 0; jj < 2 * WAKE_N - 1; jj++) {
            int j = jj / 2;
            vec4 e = wakeE[w * WAKE_N + j];
            if (e.z == 0.0 && e.w == 0.0) continue;
            if ((jj & 1) == 1) {
                vec4 nx = wakeE[w * WAKE_N + j + 1];
                if (nx.z == 0.0 && nx.w == 0.0) continue;
                vec2 hh = e.zw + nx.zw;
                e = vec4(0.5 * (e.xy + nx.xy), hh / max(length(hh), 1e-6));
            }
            float pos = 0.5 * float(jj), lastj = float(WAKE_N - 1);
            float uj = pos / lastj, un = min(pos + 0.5, lastj) / lastj, up = max(pos - 0.5, 0.0) / lastj;
            float T = life * uj * uj;
            float dtau = 0.5 * life * (un * un - up * up);
            vec2 d = p - e.xy;
            float a = dot(d, e.zw);
            float D2 = dot(d, d) + eps2, D = sqrt(D2);
            float k = WK_G * T * T / (4.0 * D2), phase = k * D;
            float dphi = WK_G * T * (U * T * a - 2.0 * D2) / (4.0 * D2 * D);
            float stp = dphi * dtau;
            float c = A.w * exp(-2.8 * stp * stp) * dtau / D;
            float kb = k * beam / 12.0;
            c *= exp(-kb * kb) * exp(-3.0 * T / life);
            c *= smoothstep(0.6 * k0, 0.95 * k0, k);
            if (foot > 0.0) c *= smoothstep(3.0, 12.0, 6.2831853 / max(k * foot, 1e-6));
            gs += (c * k * sin(phase) / D) * d;
            if (B.y > 0.0 && T < B.z) {             // the propwash: a blob at each emission point, about a beam wide, that fades
                float b = d.x * e.w - d.y * e.z;
                float sig = 0.45 * beam + 0.03 * T;
                float siga = max(sig, 0.9 * U * dtau);
                float blob = exp(-0.5 * (b * b / (sig * sig) + a * a / (siga * siga))) * pow(1.0 - T / B.z, 2.0);
                if (blob > trail) { trail = blob; bestS = dot(p, e.zw); bestB = b; }
            }
        }
        g += gs;
        if (B.y <= 0.0) continue;
        float n = wkNoise(p, foot), n2 = wkNoise(p + vec2(17.0, 5.0), foot);
        // Streaks along the track: noise stretched seven times along it, in the frame of the strongest blob.
        float sn = 0.6 * vnoise(vec2(bestS * 0.11, bestB * 1.0 + 3.1)) + 0.4 * vnoise(vec2(bestS * 0.33, bestB * 2.4 + 11.7));
        float f = 0.5 * smoothstep(0.35, 0.85, trail * 2.0 * sn);
        if (boat.z != 0.0 || boat.w != 0.0) {
            vec2 db = p - boat.xy;
            float a = dot(db, boat.zw), b = db.x * boat.w - db.y * boat.z, L = A.x;
            float s = 0.5 * L - a;                    // metres behind the bow
            float sp = smoothstep(0.4, 2.2, U);
            // The bow wave's breaking lines: short lines off the bow at about 19 degrees, torn into dashes.
            float xw = 0.3 * beam + 0.34 * max(s, 0.0);
            float th = 0.12 * beam + 0.07 * max(s, 0.0);
            float arm = (1.0 - smoothstep(0.4, 1.0, abs(abs(b) - xw) / th)) * smoothstep(-0.1 * L, 0.1 * L, s) * (1.0 - smoothstep(0.4 * L, 1.6 * L, s));
            arm *= smoothstep(0.35, 0.6, n2) * clamp(2.5 * th / max(foot, 1e-4), 0.0, 1.0);
            float da = (a - 0.42 * L) / (0.12 * L + 0.15), dbn = b / (0.35 * beam + 0.15);
            float bow = exp(-0.5 * (da * da + dbn * dbn)) * (0.7 + 0.5 * (n - 0.5));
            // The first crests of the waves, where they are steepest and near the hull, break into patches.
            float crest = smoothstep(0.14 * A.w, 0.26 * A.w, length(gs)) * exp(-length(db) / (2.0 * L)) * smoothstep(0.5, 0.68, n2);
            f = max(f, sp * clamp(max(max(arm, bow), 0.6 * crest), 0.0, 1.0));
        }
        foam = max(foam, clamp(f, 0.0, 1.0) * B.y);
    }
}

// The slope of every ripple's rings (see Ripple in scene3d.hpp), added up. Each ring is a sum of two wave trains, one
// on each branch of the capillary-gravity dispersion, found by the stationary phase of the Hankel integral of a Gaussian dip.
float rkOmega(float k) { return sqrt(WK_G * k + WK_SIGMA * k * k * k); }
float rkGroup(float k) { return (WK_G + 3.0 * WK_SIGMA * k * k) / (2.0 * rkOmega(k)); }
float rkGroupSlope(float k) {
    float w = rkOmega(k), top = WK_G + 3.0 * WK_SIGMA * k * k;
    return 3.0 * WK_SIGMA * k / w - top * top / (4.0 * w * w * w);
}
float rkRoot(float v, bool cap) {
    float lo = cap ? WK_SLOWEST : 0.02, hi = cap ? 4000.0 : WK_SLOWEST;
    if (!cap && v <= rkGroup(hi)) return 0.0;
    if (cap && v <= rkGroup(lo)) return 0.0;
    if (!cap && v >= rkGroup(lo)) return lo;
    if (cap && v >= rkGroup(hi)) return hi;
    for (int i = 0; i < 18; i++) {
        float mid = sqrt(lo * hi);
        bool above = rkGroup(mid) > v;
        if (cap ? above : !above) hi = mid; else lo = mid;
    }
    return sqrt(lo * hi);
}
vec2 rippleField(vec2 p, float foot) {
    vec2 g = vec2(0.0);
    for (int i = 0; i < WK_RIPPLES; i++) {
        if (i >= rippleCount) break;
        vec4 A = rippleA[i], Bv = rippleB[i];
        float T = A.z;
        if (T <= 0.0 || T > 3.0 * Bv.y) continue;
        vec2 d = p - A.xy;
        float dist = length(d), a = max(Bv.x, 0.005);
        float D = sqrt(dist * dist + 0.25 * a * a), v = D / T;
        float acc = 0.0;
        for (int br = 0; br < 2; br++) {
            float k = rkRoot(v, br == 1);
            if (k <= 0.0) continue;
            float S = exp(-0.5 * k * a * k * a);
            if (S < 1e-4) continue;
            float q = abs(rkGroupSlope(k)) * T + 0.5 * a * a;
            float amp = S * sqrt(k / (D * q));
            if (foot > 0.0) amp *= smoothstep(3.0, 12.0, 6.2831853 / max(k * foot, 1e-6));
            float phase = k * D - rkOmega(k) * T - 0.7853982 + (br == 0 ? 0.7853982 : -0.7853982);
            acc += -k * amp * sin(phase);
        }
        float fade = exp(-2.0 * T / max(Bv.y, 0.1)) * smoothstep(0.0, 0.15, T);
        g += d / max(dist, 1e-4) * (A.w * a * a * fade * acc);
    }
    return g;
}
)";

} // namespace
} // namespace g3d
} // namespace rmtn
