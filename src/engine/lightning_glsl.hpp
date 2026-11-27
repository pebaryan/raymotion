// lightning_glsl.hpp -- the shaders of g3d::Lightning's ribbons and of the rainbow pass. Included by scene3d.cpp only.
#pragma once

namespace rmtn {
namespace g3d {
namespace {

// A bolt's ribbons. Raylib's batch attributes: vertexPosition, vertexTexCoord (the point's place in the ribbon, in glow
// half-widths: x across, y beyond the end of the piece, so the ends are rounded), vertexColor (r brightness, g the
// core's share of the width, b the glow's strength). Drawn with a MAX blend: the brightest of the scene and the bolt.
const char* BOLT_VS = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec4 fragColor;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";
const char* BOLT_FS = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform vec3 glowCol;
uniform vec3 coreCol;
void main() {
    float r = length(fragTexCoord);
    if (r >= 1.0) discard;
    float b = fragColor.r, c = max(fragColor.g, 1e-3), gs = fragColor.b;
    float glow = exp(-r * r * 6.0) * (1.0 - smoothstep(0.55, 1.0, r));
    float halo = exp(-r * 2.2) * (1.0 - smoothstep(0.35, 1.0, r));
    float core = 1.0 - smoothstep(c * 0.3, c, r);
    vec3 col = glowCol * (glow * 0.9 + halo * 0.35) * gs * b + coreCol * core * b;
    finalColor = vec4(pow(clamp(col, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);
}
)";

// The rainbow, a pass over the picture after the clouds. pass 0 multiplies and darkens (the sky inside the bow
// brighter, Alexander's band darker; blended as dst x rgb + dst x (1 - a)), pass 1 adds the bow's own light.
const char* RAINBOW_FS = R"(#version 330
out vec4 finalColor;
uniform vec3 camPos; uniform vec3 camFwd; uniform vec3 camRight; uniform vec3 camUp; uniform vec2 tanHalf;
uniform vec2 vpOrigin; uniform vec2 vpSize; uniform vec2 zNF;
uniform sampler2D depthTex;
uniform vec3 antiSolar;
uniform float strength;
uniform float rainDist;
uniform float waterLevel; uniform int waterOn;
uniform int pass;
uniform float gain;

float lobe(float x, float mu, float s1, float s2) { float t = (x - mu) / (x < mu ? s1 : s2); return exp(-0.5 * t * t); }
// CIE 1931 colour matching functions (Wyman, Sloan, Shirley 2013), then XYZ to linear sRGB.
vec3 wavelengthRGB(float l) {
    float x = 1.056 * lobe(l, 599.8, 37.9, 31.0) + 0.362 * lobe(l, 442.0, 16.0, 26.7) - 0.065 * lobe(l, 501.1, 20.4, 26.2);
    float y = 0.821 * lobe(l, 568.8, 46.9, 40.5) + 0.286 * lobe(l, 530.9, 16.3, 31.1);
    float z = 1.217 * lobe(l, 437.0, 11.8, 36.0) + 0.681 * lobe(l, 459.0, 26.0, 13.8);
    vec3 rgb = vec3(3.2406 * x - 1.5372 * y - 0.4986 * z, -0.9689 * x + 1.8758 * y + 0.0415 * z, 0.0557 * x - 0.2040 * y + 1.0570 * z);
    return max(rgb, vec3(0.0));
}
// Descartes: the angle from the anti-solar point of the order-1 or order-2 bow at wavelength l (nm).
float bowAngle(float l, int order) {
    float um = l * 1e-3;
    float n = 1.3248 + 0.00316 / (um * um);
    float k = order == 2 ? 2.0 : 1.0;
    float i = acos(sqrt((n * n - 1.0) / (k * (k + 2.0))));
    float r = asin(sin(i) / n);
    return order == 2 ? 180.0 + degrees(2.0 * i - 6.0 * r) : degrees(4.0 * r - 2.0 * i);
}
void main() {
    vec2 uv = (gl_FragCoord.xy - vpOrigin) / vpSize, ndc = uv * 2.0 - 1.0;
    vec3 d = normalize(camFwd + camRight * (ndc.x * tanHalf.x) + camUp * (ndc.y * tanHalf.y));
    float dep = texelFetch(depthTex, ivec2(gl_FragCoord.xy), 0).r;
    float dist = 1e9;
    if (dep < 0.99999) {
        float z = 2.0 * zNF.x * zNF.y / (zNF.y + zNF.x - (dep * 2.0 - 1.0) * (zNF.y - zNF.x));
        dist = z / dot(d, camFwd);
    }
    if (waterOn == 1 && camPos.y > waterLevel && d.y < 0.0) dist = min(dist, (waterLevel - camPos.y) / d.y);
    float seen = smoothstep(0.33 * rainDist, rainDist, dist);              // the rain is out there: nearer things stand in front of it
    float th = degrees(acos(clamp(dot(d, antiSolar), -1.0, 1.0)));
    if (seen <= 0.0 || th > 62.0) { finalColor = pass == 0 ? vec4(0.0, 0.0, 0.0, 0.0) : vec4(0.0); return; }
    vec3 sumP = vec3(0.0), sumS = vec3(0.0), norm = vec3(0.0);
    for (int i = 0; i < 12; i++) {
        float l = 400.0 + 27.2727 * float(i);
        vec3 c = wavelengthRGB(l);
        norm += c;
        float a1 = bowAngle(l, 1), a2 = bowAngle(l, 2);
        float u1 = (a1 - th) / 0.38;                                      // > 0 inside the primary: the lit side, falling slowly
        float p1 = u1 > 0.0 ? exp(-u1 / 2.6) : exp(-u1 * u1 * 1.6);
        float u2 = (th - a2) / 0.55;                                      // > 0 outside the secondary: lit
        float p2 = u2 > 0.0 ? exp(-u2 / 2.2) : exp(-u2 * u2 * 1.3);
        sumP += c * p1; sumS += c * p2;
    }
    vec3 P = sumP / max(norm, vec3(1e-3)), S = sumS / max(norm, vec3(1e-3));
    P = mix(vec3(dot(P, vec3(0.3, 0.59, 0.11))), P, 0.75);                  // a translucent veil, not a paint: the colours are pale
    S = mix(vec3(dot(S, vec3(0.3, 0.59, 0.11))), S, 0.45);                  // and the secondary is fainter and much less saturated
    vec3 bow = P + 0.4 * S;
    float inside = 1.0 - smoothstep(37.0, 41.0, th);                      // the sky inside the primary is lighter
    float band = smoothstep(42.6, 44.2, th) * (1.0 - smoothstep(48.4, 50.2, th));      // Alexander's dark band
    float vis = strength * seen;
    if (pass == 0) {
        finalColor = vec4(0.0, 0.0, 0.0, clamp(band * 0.22 * vis, 0.0, 0.9));
    } else {
        finalColor = vec4((bow * gain + vec3(inside * 0.08)) * vis, 0.0);
    }
}
)";

} // namespace
} // namespace g3d
} // namespace rmtn
