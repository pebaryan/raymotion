// filters.cpp -- layer image filters for illustration looks (posterize, halftone, paper,
// ink edges, watercolour), tilt-shift and a film response (bloom, filmic curve, grade), one
// full-target pass each over premultiplied content (film: a half-size bright pass, then one).
#include "rmtn/render.hpp"
#include <rlgl.h>
#include <algorithm>
#include <cmath>

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
uniform float position; uniform float band; uniform float falloff;
uniform sampler2D glowTex; uniform vec2 glowTexel; uniform float glowLod0; uniform float glowLods;
uniform float exposure; uniform float bloom; uniform vec4 glow; uniform float contrast; uniform float saturation;
uniform vec4 shadows; uniform vec4 highlights; uniform float toning; uniform float vignette;
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
// Clamped to the target, so taps beyond the frame repeat its edge rather than wrap round.
vec4 at(vec2 d) { return texture(texture0, clamp(fragTexCoord + d * pxScale * texel, 0.5 * texel, 1.0 - 0.5 * texel)); }

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

// Film: linear light, and how far above the knee it is (0 below `threshold`, 1 at white).
vec3 linear(vec3 u) { return pow(max(u, 0.0), vec3(2.2)) * exp2(exposure); }
float overKnee(vec3 lin) { return clamp((luma(lin) - threshold) / max(1.0 - threshold, 0.05), 0.0, 1.0); }
// A 3x3 tent of one mip level of the bright pass, so each level is smooth, not blocky.
vec3 tent(float lod) {
    vec2 d = glowTexel * exp2(lod);
    vec3 s = 4.0 * textureLod(glowTex, fragTexCoord, lod).rgb;
    s += 2.0 * (textureLod(glowTex, fragTexCoord + vec2(d.x, 0), lod).rgb + textureLod(glowTex, fragTexCoord - vec2(d.x, 0), lod).rgb
              + textureLod(glowTex, fragTexCoord + vec2(0, d.y), lod).rgb + textureLod(glowTex, fragTexCoord - vec2(0, d.y), lod).rgb);
    s += textureLod(glowTex, fragTexCoord + d, lod).rgb + textureLod(glowTex, fragTexCoord - d, lod).rgb
       + textureLod(glowTex, fragTexCoord + vec2(d.x, -d.y), lod).rgb + textureLod(glowTex, fragTexCoord + vec2(-d.x, d.y), lod).rgb;
    return s / 16.0;
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
    } else if (mode == 4) {                                           // watercolour
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
    } else if (mode == 5) {                                           // tilt-shift
        // Distance from the focus line (through the centre at `position` from the top, at
        // `angle`), as a fraction of the height; blur grows from the band's edge.
        vec2 dims = 1.0 / (texel * pxScale);
        vec2 mid = vec2(dims.x * 0.5, dims.y * (1.0 - position));
        float ang = radians(angle);
        float dist = abs(dot(p - mid, vec2(-sin(ang), cos(ang)))) / dims.y;
        float r = size * smoothstep(band, band + max(falloff, 1e-4), dist);
        vec4 sum = c;
        float n = 1.0;
        if (r > 0.25) {
            for (int i = 0; i < 64; i++) {                            // a golden-angle disc
                float rr = r * sqrt((float(i) + 0.5) / 64.0), a = float(i) * 2.39996323;
                sum += at(vec2(cos(a), sin(a)) * rr);
                n += 1.0;
            }
        }
        vec4 b = sum / n;
        vec3 u = unpre(b);
        u = clamp(mix(vec3(luma(u)), u, 1.0 + strength), 0.0, 1.0);
        finalColor = vec4(u * b.a, b.a);
    } else if (mode == 7) {                                           // film, pass 1: the light above the knee
        vec3 lin = linear(unpre(c));
        float k = overKnee(lin);
        finalColor = vec4(lin * k * k, 1.0);
    } else if (mode == 6) {                                           // film, pass 2
        vec3 lin = linear(unpre(c));
        float k = overKnee(lin);
        lin *= 1.0 + 3.0 * k * k * bloom;                             // what clipped at white was brighter than white
        vec3 g = vec3(0.0);
        float n = 0.0;
        for (int i = 0; i < 10; i++) {                                // every octave up to the radius, equally weighted
            float lod = glowLod0 + float(i);                          // level 0 here is 2 composition px at any supersampling
            if (lod > glowLods) break;
            g += tent(lod);
            n += 1.0;
        }
        vec3 add = bloom * glow.rgb * 2.0 * g / max(n, 1.0);
        lin += add;
        vec3 m = clamp(lin * (2.51 * lin + 0.03) / (lin * (2.43 * lin + 0.59) + 0.14), 0.0, 1.0);   // ACES (Narkowicz)
        vec3 s = pow(m, vec3(1.0 / 2.2));
        s = contrast >= 1.0 ? mix(s, s * s * (3.0 - 2.0 * s), min(contrast - 1.0, 1.0)) : mix(vec3(0.5), s, contrast);
        s = mix(vec3(luma(s)), s, saturation);
        float l = clamp(luma(s), 0.0, 1.0);
        s += toning * ((shadows.rgb - 0.5) * (1.0 - l) * (1.0 - l) + (highlights.rgb - 0.5) * l * l) * 2.0;
        vec2 dims = 1.0 / texel;
        vec2 q = (fragTexCoord - 0.5) * vec2(dims.x / dims.y, 1.0);
        s *= 1.0 - vignette * smoothstep(0.25, 1.0, length(q) * 1.1);
        s += (hash(gl_FragCoord.xy) + hash(gl_FragCoord.xy + 0.37) - 1.0) / 255.0;   // dither: no banding in the sky
        float a = max(c.a, clamp(luma(add), 0.0, 1.0));
        finalColor = vec4(clamp(s, 0.0, 1.0) * a, a);
    }
}
)";

Shader shader{};
int L_mode = -1, L_texel, L_pxScale, L_levels, L_size, L_angle, L_colored, L_threshold, L_strength, L_granulation,
    L_ink, L_paper, L_position, L_band, L_falloff, L_glowTex, L_glowTexel, L_glowLod0, L_glowLods, L_exposure, L_bloom, L_glow,
    L_contrast, L_saturation, L_shadows, L_highlights, L_toning, L_vignette;
RenderTexture glowTarget{};    // film: the bright pass at half size, mipmapped

void ensureShader() {
    if (shader.id) return;
    shader = LoadShaderFromMemory(nullptr, FILTER_FS);
    auto loc = [](const char* n) { return GetShaderLocation(shader, n); };
    L_mode = loc("mode"); L_texel = loc("texel"); L_pxScale = loc("pxScale"); L_levels = loc("levels");
    L_size = loc("size"); L_angle = loc("angle"); L_colored = loc("colored"); L_threshold = loc("threshold");
    L_strength = loc("strength"); L_granulation = loc("granulation"); L_ink = loc("ink"); L_paper = loc("paper");
    L_position = loc("position"); L_band = loc("band"); L_falloff = loc("falloff");
    L_glowTex = loc("glowTex"); L_glowTexel = loc("glowTexel"); L_glowLod0 = loc("glowLod0"); L_glowLods = loc("glowLods");
    L_exposure = loc("exposure"); L_bloom = loc("bloom"); L_glow = loc("glow"); L_contrast = loc("contrast");
    L_saturation = loc("saturation"); L_shadows = loc("shadows"); L_highlights = loc("highlights");
    L_toning = loc("toning"); L_vignette = loc("vignette");
    onGraphicsShutdown([] {
        UnloadShader(shader); shader = Shader{};
        if (glowTarget.id) UnloadRenderTexture(glowTarget);
        glowTarget = RenderTexture{};
    });
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
    SetShaderValue(shader, L_position, &f.position, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_band, &f.band, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_falloff, &f.falloff, SHADER_UNIFORM_FLOAT);
    SetTextureWrap(src.texture, TEXTURE_WRAP_CLAMP);          // neighbours past the edge repeat the edge

    // One full-target pass of `pass` from src into `to` (w x h of it).
    auto run = [&](int pass, const RenderTexture& to, float tw, float th, const Texture2D* glowTex) {
        SetShaderValue(shader, L_mode, &pass, SHADER_UNIFORM_INT);
        BeginTextureMode(to);
        rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);       // replace: the pass covers every pixel
        BeginBlendMode(BLEND_CUSTOM);
        BeginShaderMode(shader);
        if (glowTex) SetShaderValueTexture(shader, L_glowTex, *glowTex);
        DrawTexturePro(src.texture, { 0, 0, w, -h }, { 0, 0, tw, th }, { 0, 0 }, 0, WHITE);
        EndShaderMode();
        EndBlendMode();
        EndTextureMode();
    };
    if (f.type != Filter::Type::Film) { run(mode, dst, w, h, nullptr); return; }

    // Film: the light above the knee at half size, mipmapped, so the compose pass can read a
    // glow of any width from a few levels of the chain.
    const int gw = std::max(1, (int)w / 2), gh = std::max(1, (int)h / 2);
    if (glowTarget.id && (glowTarget.texture.width != gw || glowTarget.texture.height != gh)) {
        UnloadRenderTexture(glowTarget);
        glowTarget = RenderTexture{};
    }
    if (!glowTarget.id) glowTarget = LoadRenderTexture(gw, gh);
    const Vector4 glow = rgba(f.glow), shadows = rgba(f.shadows), highlights = rgba(f.highlights);
    const Vector2 glowTexel{ 1.f / gw, 1.f / gh };
    const float glowLod0 = std::max(0.f, std::log2(scale));
    const float glowLods = glowLod0 + std::max(0.f, std::log2(std::max(1.f, f.size / 2.f)));
    SetShaderValue(shader, L_glowTexel, &glowTexel, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, L_glowLod0, &glowLod0, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_glowLods, &glowLods, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_exposure, &f.exposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_bloom, &f.bloom, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_glow, &glow, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, L_contrast, &f.contrast, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_saturation, &f.saturation, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_shadows, &shadows, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, L_highlights, &highlights, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, L_toning, &f.toning, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, L_vignette, &f.vignette, SHADER_UNIFORM_FLOAT);
    SetTextureFilter(src.texture, TEXTURE_FILTER_BILINEAR);  // the 2:1 reduction averages four texels
    run(7, glowTarget, (float)gw, (float)gh, nullptr);
    SetTextureFilter(src.texture, TEXTURE_FILTER_POINT);
    Texture2D chain = glowTarget.texture;
    GenTextureMipmaps(&chain);
    SetTextureFilter(chain, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(chain, TEXTURE_WRAP_CLAMP);
    run(6, dst, w, h, &chain);
}

} // namespace rmtn
