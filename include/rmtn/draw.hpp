// rmtn/draw.hpp -- procedural drawing primitives. Everything here is generated from code;
// points go through the node transform ctx (origin, scale, rotation, accumulated alpha).
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "timeline.hpp"

namespace rmtn {
namespace dr {

using V2 = Vector2;

static inline Color TA(const Ctx& c, Color col) { return withAlpha(col, c.a); }

static inline void quad(const Ctx& c, V2 p0, V2 p1, V2 p2, V2 p3, Color col) {
    Color k = TA(c, col);
    Vector2 a = mapP(c, p0), b = mapP(c, p1), d = mapP(c, p2), e = mapP(c, p3);
    DrawTriangleGradient({ a.x,a.y }, { b.x,b.y }, { d.x,d.y }, k, k, k);
    DrawTriangleGradient({ a.x,a.y }, { d.x,d.y }, { e.x,e.y }, k, k, k);
}
static inline void rect(const Ctx& c, float x, float y, float w, float h, Color col) {
    quad(c, { x, y }, { x+w, y }, { x+w, y+h }, { x, y+h }, col);
}
// four-corner gradient quad
static inline void rectGrad4(const Ctx& c, float x, float y, float w, float h,
                             Color tl, Color tr, Color br, Color bl) {
    Vector2 a = mapP(c,{x,y}), b = mapP(c,{x+w,y}), d = mapP(c,{x+w,y+h}), e = mapP(c,{x,y+h});
    Color A = TA(c,tl), B = TA(c,tr), D = TA(c,br), E = TA(c,bl);
    DrawTriangleGradient(a, b, d, A, B, D);
    DrawTriangleGradient(a, d, e, A, D, E);
}
// Any simple polygon, convex or not (stars, blobs), via ear clipping. Triangles do not
// overlap, so translucent fills stay uniform. Self-intersecting input degrades to a fan.
static inline void fillPoly(const Ctx& c, const V2* pts, int n, Color col) {
    if (n < 3) return;
    Color k = TA(c, col);
    static thread_local std::vector<Vector2> m;
    static thread_local std::vector<int> idx;
    m.resize(n); idx.resize(n);
    double area = 0;
    for (int i = 0; i < n; i++) { m[i] = mapP(c, pts[i]); idx[i] = i; }
    for (int i = 0; i < n; i++) area += (double)m[i].x * m[(i+1)%n].y - (double)m[(i+1)%n].x * m[i].y;
    const double sgn = area >= 0 ? 1.0 : -1.0;
    auto cross = [](Vector2 a, Vector2 b, Vector2 p) {
        return ((double)b.x - a.x) * ((double)p.y - a.y) - ((double)b.y - a.y) * ((double)p.x - a.x);
    };
    size_t i = 0, misses = 0;
    while (idx.size() > 3 && misses <= idx.size()) {
        size_t sz = idx.size();
        int ia = idx[(i + sz - 1) % sz], ib = idx[i % sz], ic = idx[(i + 1) % sz];
        double turn = cross(m[ia], m[ib], m[ic]) * sgn;
        bool ear = turn > 0;
        for (size_t j = 0; ear && j < sz; j++) {
            int q = idx[j];
            if (q == ia || q == ib || q == ic) continue;
            ear = !(cross(m[ia], m[ib], m[q]) * sgn >= 0 && cross(m[ib], m[ic], m[q]) * sgn >= 0 &&
                    cross(m[ic], m[ia], m[q]) * sgn >= 0);
        }
        if (ear || std::fabs(turn) < 1e-9) {          // collinear vertices are simply dropped
            if (ear) DrawTriangle(m[ia], m[ib], m[ic], k);
            idx.erase(idx.begin() + (long)(i % sz));
            misses = 0;
        } else { i++; misses++; }
        i %= idx.size();
    }
    for (size_t j = 1; j + 1 < idx.size(); j++) DrawTriangle(m[idx[0]], m[idx[j]], m[idx[j+1]], k);
}
static inline void strokeSeg(const Ctx& c, V2 p0, V2 p1, float w, Color col) {
    V2 a = mapP(c, p0), b = mapP(c, p1);
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = std::sqrt(dx*dx + dy*dy); if (len < 1e-6f) len = 1e-6f;
    float ux = -dy/len * (w*c.s*0.5f), uy = dx/len * (w*c.s*0.5f);
    Color k = TA(c, col);
    DrawTriangleGradient({ a.x+ux, a.y+uy }, { b.x+ux, b.y+uy }, { b.x-ux, b.y-uy }, k,k,k);
    DrawTriangleGradient({ a.x+ux, a.y+uy }, { b.x-ux, b.y-uy }, { a.x-ux, a.y-uy }, k,k,k);
}
static inline void polyStroke(const Ctx& c, const V2* pts, int n, bool closed, float w, Color col) {
    int last = closed ? n : n - 1;
    for (int i = 0; i < last; i++) strokeSeg(c, pts[i], pts[(i+1)%n], w, col);
}
static inline void circle(const Ctx& c, V2 ctr, float r, Color col) {
    DrawCircleV(mapP(c, ctr), r * c.s, TA(c, col));
}
static inline void ring(const Ctx& c, V2 ctr, float r, float w, Color col) {
    DrawCircleLinesEx(mapP(c, ctr), r * c.s, std::max(1.0f, w * c.s), TA(c, col));
}
static inline void arc(const Ctx& c, V2 ctr, float r, float w, float a0deg, float a1deg, Color col) {
    DrawCircleSectorLinesEx(mapP(c, ctr), r * c.s, a0deg, a1deg, 64,
                            std::max(1.0f, w * c.s), TA(c, col));
}
static inline void sector(const Ctx& c, V2 ctr, float r, float a0deg, float a1deg, Color col) {
    DrawCircleSector(mapP(c, ctr), r * c.s, a0deg, a1deg, 64, TA(c, col));
}
// rounded rectangle: sampled arcs around 4 corner centers
static inline void roundRect(const Ctx& c, float x, float y, float w, float h, float rad, Color col) {
    if (rad <= 0) return rect(c, x, y, w, h, col);
    if (rad > w/2) rad = w/2; if (rad > h/2) rad = h/2;
    struct CR { float cx, cy, a0, a1; } corners[4] = {
        { x+rad, y+rad, 180.f, 270.f }, { x+w-rad, y+rad, 270.f, 360.f },
        { x+w-rad, y+h-rad, 0.f, 90.f }, { x+rad, y+h-rad, 90.f, 180.f } };
    std::vector<V2> pts;
    for (auto& q : corners) {
        for (int i = 0; i <= 8; i++) {
            double th = (double)(q.a0 + (q.a1 - q.a0) * i / 8.0) * (PI / 180.0);
            pts.push_back({ q.cx + rad*(float)std::cos(th), q.cy + rad*(float)std::sin(th) });
        }
    }
    fillPoly(c, pts.data(), (int)pts.size(), col);
}
// organic blob: r(theta) = base + amp*sin(freq*theta + phase)
static inline void blob(const Ctx& c, V2 ctr, float base, float amp, int freq, float phaseDeg, Color col) {
    const int N = 96;
    static thread_local std::vector<V2> buf; buf.resize(N);
    float ph = phaseDeg * (float)PI / 180.0f;
    for (int i = 0; i < N; i++) {
        double th = 2.0 * PI * i / N;
        float r = base + amp * std::sin(freq * th + ph);
        buf[i] = { ctr.x + r*(float)std::cos(th), ctr.y + r*(float)std::sin(th) };
    }
    fillPoly(c, buf.data(), N, col);
}
static inline void star(const Ctx& c, V2 ctr, float rOut, float rIn, int points, float rotOffDeg, Color col) {
    std::vector<V2> pts; pts.reserve(points*2);
    for (int i = 0; i < points*2; i++) {
        double th = (PI / points) * i + rotOffDeg * (PI / 180.0);
        float r = (i % 2 == 0) ? rOut : rIn;
        pts.push_back({ ctr.x + r*(float)std::cos(th), ctr.y + r*(float)std::sin(th) });
    }
    fillPoly(c, pts.data(), (int)pts.size(), col);
}
// text: rotation follows accumulated ctx rotation; fontSize/spacing scale with ctx.
// `anchor`: 0 = top-left as raylib, 0.5 = center-ish on X (uses MeasureTextEx).
static void text(Font font, const Ctx& c, const char* str, V2 pos, float fontSize, float spacing,
                 Color col, double anchorX = 0.0, double anchorY = 0.0) {
    Vector2 m = MeasureTextEx(font, str, fontSize * c.s, spacing * c.s);
    Vector2 at = mapP(c, pos);
    // measure is in already-scaled px; unscale the anchor offset back into screen space
    at.x -= (float)anchorX * m.x;
    at.y -= (float)anchorY * m.y;
    DrawTextPro(font, str, at, { 0, 0 }, -c.rot, fontSize * c.s, spacing * c.s, TA(c, col));
}
static Vector2 measure(Font font, const char* str, float fontSize, float spacing) {
    return MeasureTextEx(font, str, fontSize, spacing);
}

} } // rmtn::dr
