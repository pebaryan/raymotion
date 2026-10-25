// filters.cpp -- layer image filters for illustration looks (posterize, halftone, paper,
// ink edges, watercolour), one full-target pass each over premultiplied content.
#include "rmtn/render.hpp"
#include <rlgl.h>
#include <algorithm>

namespace rmtn {
namespace {

// Pattern positions are composition px (gl_FragCoord / pxScale), so they hold at any
// supersampling; `at` samples the content `d` composition px away.
const char* FILTER_FS = R"(#version 330
in vec2 fragTexCoord; in vec4 fragColor;
uniform sampler2D texture0;
uniform int mode; uniform vec2 texel; uniform float pxScale;
uniform int levels; uniform float size; uniform float angle; uniform int colored;
uniform float threshold; uniform float strength; uniform float granulation;
uniform vec4 ink; uniform vec4 paper;
out vec4 finalColor;

vec3 unpre(vec4 c) { return c.a > 0.0 ? c.rgb / c.a : vec3(0.0); }
float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { s += a * vnoise(p); p = p * 2.03 + 17.1; a *= 0.5; }
    return s / 0.97;
}
vec4 at(vec2 d) { return texture(texture0, fragTexCoord + d * pxScale * texel); }

// Coverage of one halftone screen at `ang` (radians) for ink amount k: dot area ~ k.
float screen(vec2 p, float ang, float k) {
    if (k <= 0.0) return 0.0;
    float c = cos(ang), s = sin(ang);
    vec2 q = mat2(c, s, -s, c) * p / size;
    float d = length(fract(q) - 0.5);
    float r = 0.564 * sqrt(clamp(k, 0.0, 1.0)) * (1.0 + 0.33 * k * k);   // 0.75 at k = 1: cells fill
    float aa = 0.75 / (size * pxScale);
    return 1.0 - smoothstep(r - aa, r + aa, d);
}

void main() {
    vec2 p = gl_FragCoord.xy / pxScale;
    vec4 c = texture(texture0, fragTexCoord);
    if (mode == 0) {                                                  // posterize: brightness steps, hue kept
        vec3 u = unpre(c);
        float v = max(max(u.r, u.g), u.b);
        float l = float(max(levels, 1));
        float q = v > 0.0 ? ceil(v * l - 0.25) / l : 0.0;
        finalColor = vec4(u * (v > 0.0 ? max(q, 1.0 / l) / v : 0.0) * c.a, c.a);
    } else if (mode == 1) {                                           // halftone
        vec3 u = unpre(c);
        float base = radians(angle);
        vec3 col;
        if (colored == 1) {
            float k = 1.0 - max(max(u.r, u.g), u.b);
            vec3 cmy = k < 1.0 ? (1.0 - u - k) / (1.0 - k) : vec3(0.0);
            col = paper.rgb;
            col *= 1.0 - screen(p, base - radians(30.0), cmy.x) * vec3(1.0, 0.0, 0.0);
            col *= 1.0 - screen(p, base + radians(30.0), cmy.y) * vec3(0.0, 1.0, 0.0);
            col *= 1.0 - screen(p, base - radians(45.0), cmy.z) * vec3(0.0, 0.0, 1.0);
            col = mix(col, ink.rgb, screen(p, base, k));
        } else col = mix(paper.rgb, ink.rgb, screen(p, base, 1.0 - luma(u)));
        finalColor = vec4(col * c.a, c.a);
    } else if (mode == 2) {                                           // paper
        vec2 q = p / max(size, 0.05);
        float tex = 0.55 * fbm(q / 7.0) + 0.15 * vnoise(vec2(q.x * 0.35, q.y * 0.05)) + 0.3 * vnoise(q * 0.8);
        finalColor = vec4(c.rgb * (1.0 - strength * 0.45 * tex), c.a);
    } else if (mode == 3) {                                           // ink edges (Sobel)
        float h = size * 0.5;
        vec4 tl = at(vec2(-h, -h)), tc = at(vec2(0, -h)), tr = at(vec2(h, -h));
        vec4 ml = at(vec2(-h, 0)), mr = at(vec2(h, 0));
        vec4 bl = at(vec2(-h, h)), bc = at(vec2(0, h)), br = at(vec2(h, h));
        vec4 gx = (tr + 2.0 * mr + br) - (tl + 2.0 * ml + bl);
        vec4 gy = (bl + 2.0 * bc + br) - (tl + 2.0 * tc + tr);
        float mag = max(length(vec2(length(gx.rgb), length(gy.rgb))) / 1.732, length(vec2(gx.a, gy.a))) / 4.0;
        float e = smoothstep(threshold, threshold * 1.5 + 0.02, mag) * ink.a;
        finalColor = c * (1.0 - e) + vec4(ink.rgb, 1.0) * e;
    } else {                                                          // watercolour
        vec2 w = vec2(fbm(p / (size * 5.0)), fbm(p / (size * 5.0) + 31.7)) - 0.5;
        vec2 off = w * size * 2.0;                                    // bleed: wander the edges
        vec4 s = at(off);
        vec4 m = vec4(0.0);
        for (int i = 0; i < 12; i++) {
            float a = float(i) * 2.39996, r = size * sqrt((float(i) + 0.5) / 12.0);
            m += at(off + vec2(cos(a), sin(a)) * r);
        }
        m /= 12.0;
        vec3 u = unpre(s), mu = unpre(m);
        float edge = clamp((s.a - m.a) * 2.0 + length(u - mu) * 1.5, 0.0, 1.0);
        float gran = (0.5 * vnoise(p / 1.5) + 0.5 * fbm(p / 3.0) - 0.5) * granulation;
        float wash = (fbm(p / (size * 12.0)) - 0.5) * 0.3 * granulation;
        float d = 1.0 + strength * edge + gran + wash;                // pigment density
        vec3 col = clamp(u - (u - u * u) * (d - 1.0), 0.0, 1.0);      // Bousseau et al.'s density model
        finalColor = vec4(col * s.a, s.a);
    }
}
)";

Shader shader{};
int L_mode = -1, L_texel, L_pxScale, L_levels, L_size, L_angle, L_colored, L_threshold, L_strength, L_granulation,
    L_ink, L_paper;

void ensureShader() {
    if (shader.id) return;
    shader = LoadShaderFromMemory(nullptr, FILTER_FS);
    auto loc = [](const char* n) { return GetShaderLocation(shader, n); };
    L_mode = loc("mode"); L_texel = loc("texel"); L_pxScale = loc("pxScale"); L_levels = loc("levels");
    L_size = loc("size"); L_angle = loc("angle"); L_colored = loc("colored"); L_threshold = loc("threshold");
    L_strength = loc("strength"); L_granulation = loc("granulation"); L_ink = loc("ink"); L_paper = loc("paper");
    onGraphicsShutdown([] { UnloadShader(shader); shader = Shader{}; });
}

Vector4 rgba(Color c) { return { c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f }; }

} // namespace

void applyFilter(const Filter& f, const RenderTexture& src, const RenderTexture& dst, float scale) {
    ensureShader();
    const float w = (float)src.texture.width, h = (float)src.texture.height;
    const int mode = (int)f.type, colored = f.colored ? 1 : 0;
    const float size = std::max(0.25f, f.size);
    const Vector2 texel{ 1.f / w, 1.f / h };
    const Vector4 ink = rgba(f.ink), paper = rgba(f.paper);
    SetShaderValue(shader, L_mode, &mode, SHADER_UNIFORM_INT);
    SetShaderValue(shader, L_texel, &texel, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, L_pxScale, &scale, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_levels, &f.levels, SHADER_UNIFORM_INT);
    SetShaderValue(shader, L_size, &size, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_angle, &f.angle, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_colored, &colored, SHADER_UNIFORM_INT);
    SetShaderValue(shader, L_threshold, &f.threshold, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_strength, &f.strength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_granulation, &f.granulation, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_ink, &ink, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, L_paper, &paper, SHADER_UNIFORM_VEC4);
    SetTextureWrap(src.texture, TEXTURE_WRAP_CLAMP);          // neighbours past the edge repeat the edge
    BeginTextureMode(dst);
    rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);           // replace: the pass covers every pixel
    BeginBlendMode(BLEND_CUSTOM);
    BeginShaderMode(shader);
    DrawTexturePro(src.texture, { 0, 0, w, -h }, { 0, 0, w, h }, { 0, 0 }, 0, WHITE);
    EndShaderMode();
    EndBlendMode();
    EndTextureMode();
}

} // namespace rmtn
