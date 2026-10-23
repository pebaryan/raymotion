#pragma once
// Procedural 3D for rmtn: mesh builders + perspective projection + painter-algorithm fills.
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include "raylib.h"

namespace rmtn {
namespace d3 {

struct V3 { float x, y, z; };
inline V3 operator+(V3 a, V3 b){ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline V3 operator-(V3 a, V3 b){ return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline V3 operator*(V3 a, float s){ return {a.x*s,a.y*s,a.z*s}; }
inline float dot(V3 a, V3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
inline V3 cross(V3 a, V3 b){ return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
inline float len(V3 a){ return std::sqrt(dot(a,a)); }
inline V3 nrm(V3 a){ float l=len(a); return l>1e-9f ? V3{a.x/l,a.y/l,a.z/l} : a; }

using Mat4 = std::array<float,16>;   // row-major
static inline Mat4 identM(){ Mat4 m{}; for(int i=0;i<4;i++) m[i*4+i]=1.f; return m; }
static inline void matMul(const Mat4&a,const Mat4&b,Mat4&o){
    for(int i=0;i<4;i++)for(int j=0;j<4;j++){float s=0;for(int k=0;k<4;k++)s+=a[i*4+k]*b[k*4+j];o[i*4+j]=s;}
}
static inline Mat4 mkT(V3 t){ Mat4 m=identM(); m[12]=t.x;m[13]=t.y;m[14]=t.z; return m; }   // row-vector affine: translation in last row (matches xf())
static inline Mat4 rotX(float r){ Mat4 m=identM(); float c=std::cos(r),s=std::sin(r); m[5]=c;m[6]=s;m[9]=-s;m[10]=c; return m; }
static inline Mat4 rotY(float r){ Mat4 m=identM(); float c=std::cos(r),s=std::sin(r); m[0]=c;m[2]=-s;m[8]=s;m[10]=c; return m; }
static inline Mat4 rotZ(float r){ Mat4 m=identM(); float c=std::cos(r),s=std::sin(r); m[0]=c;m[1]=s;m[4]=-s;m[5]=c; return m; }
static inline Mat4 mkS(float s){ Mat4 m{}; m[0]=s;m[5]=s;m[10]=s;m[15]=1.f; return m; }
static inline Mat4 trs(V3 pos, V3 eul, float sc){   // row-vector: v*M with M = S * Rx*Ry*Rz * T
    Mat4 m=mkS(sc), t{};
    matMul(m, rotX(eul.x), t); matMul(t, rotY(eul.y), m); matMul(m, rotZ(eul.z), t);
    matMul(t, mkT(pos), m);
    return m;
}
static inline V3 xf(const Mat4&m, V3 p){
    return { m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],
             m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],
             m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14] };
}

// ---- mesh containers: raw world-space faces (filled later w/ transform) ----
struct Face3 { std::array<V3,3> v; Color col; };
using Mesh = std::vector<Face3>;

static inline void meshAdd(Mesh&m, V3 a, V3 b, V3 c, Color col){ m.push_back({{a,b,c},col}); }

// convex-face extractor from vertices: all triples whose edges == min edge length (icosahedra/octahedra)
static inline Mesh polyHull(const std::vector<V3>&V, const Color&col, bool sub=true){
    float mn=1e9f;
    for(size_t i=0;i<V.size();i++)for(size_t j=i+1;j<V.size();j++){float d=len(V[i]-V[j]); if(d<mn)mn=d;}
    Mesh m;
    for(size_t i=0;i<V.size();i++)for(size_t j=i+1;j<V.size();j++)for(size_t k=j+1;k<V.size();k++){
        float dij=len(V[i]-V[j]),djk=len(V[j]-V[k]),dik=len(V[i]-V[k]);
        if(dij<mn*1.21f&&djk<mn*1.21f&&dik<mn*1.21f) meshAdd(m,V[i],V[j],V[k],col);
    }
    if(!sub) return m;
    Mesh subdiv;
    auto midN=[](V3 a, V3 b){ V3 m=(a+b)*0.5f; float l=len(m); return l>1e-9f ? m*((len(a)+len(b))*0.5f/l) : m; };
    for(auto&f:m){
        V3 m01=midN(f.v[0],f.v[1]),m12=midN(f.v[1],f.v[2]),m20=midN(f.v[2],f.v[0]);
        meshAdd(subdiv, nrm(f.v[0]), m01, m20, f.col);
        meshAdd(subdiv, m01, nrm(f.v[1]), m12, f.col);
        meshAdd(subdiv, m20, m12, nrm(f.v[2]), f.col);
        meshAdd(subdiv, m01, m12, m20, f.col);
    }
    return subdiv;
}
static inline Mesh icosphere(float R){
    float phi=(1.f+std::sqrt(5.f))/2.f;
    std::vector<V3> V={
      {-1,phi,0},{1,phi,0},{-1,-phi,0},{1,-phi,0},
      {0,-1,phi},{0,1,phi},{0,-1,-phi},{0,1,-phi},
      {phi,0,-1},{phi,0,1},{-phi,0,-1},{-phi,0,1}};
    for(auto&v:V) v=v*0.5773502692f;   // unit sphere
    return polyHull(V, WHITE);         // one geodesic level => ~80 faces
    (void)R;
}
static inline Mesh octahedron(float R){
    std::vector<V3> V{{R,0,0},{-R,0,0},{0,R,0},{0,-R,0},{0,0,R},{0,0,-R}};
    return polyHull(V, WHITE);         // subdivides to rounded bumps — kept small so it reads as crystal
}
static inline Mesh torusKnot(float sc, int steps, int ringSeg, float tube){
    Mesh m;
    const float P=2.f,Q=3.f,R=2.0f,r=0.9f;
    auto curve=[&](float t)->V3{
        float cq=std::cos(Q*t), sq=std::sin(Q*t);
        return { std::cos(P*t)*(R+r*cq)*sc, r*sq*sc, std::sin(P*t)*(R+r*cq)*sc };
    };
    V3 prevN, prevB; bool first=true;
    for(int i=0;i<steps;i++){
        float t0=6.2831853f*P*i/steps, t1=6.2831853f*P*(i+1)/steps;
        V3 p0=curve(t0), p1=curve(t1);
        V3 T=nrm(p1-p0);
        V3 N;
        if(first){ N=nrm(cross(T,std::fabs(T.y)<0.8f?V3{0,1,0}:V3{1,0,0})); first=false; }
        else { N=prevN-T*dot(prevN,T); N=len(N)>1e-6f?nrm(N):prevN; }
        V3 B=cross(T,N);
        prevN=N; prevB=B;
        for(int s0=0;s0<ringSeg;s0++){
            float a0=6.2831853f*s0/ringSeg, a1=6.2831853f*(s0+1)/ringSeg;
            auto ringPt=[&](V3 c,float a){ return c + (N*std::cos(a)+B*std::sin(a))*(tube*sc); };
            V3 A0=ringPt(p0,a0), A1=ringPt(p0,a1), B0=ringPt(p1,a0), B1=ringPt(p1,a1);
            meshAdd(m,A0,B0,B1, WHITE); meshAdd(m,A0,B1,A1, WHITE);
        }
    }
    return m;
}
static inline Mesh sphereMesh(float R, int lat, int lon){
    Mesh m;
    for(int i=0;i<lat;i++){
        float u0=3.1415926f*i/lat,u1=3.1415926f*(i+1)/lat;
        for(int j=0;j<lon;j++){
            float v0=6.2831853f*j/lon,v1=6.2831853f*(j+1)/lon;
            auto sp=[&](float u,float v){ return V3{ R*std::sin(u)*std::cos(v), R*std::cos(u), R*std::sin(u)*std::sin(v) }; };
            V3 a=sp(u0,v0),b=sp(u0,v1),c=sp(u1,v1),d=sp(u1,v0);
            meshAdd(m,a,b,c,WHITE); meshAdd(m,a,c,d,WHITE);
        }
    }
    return m;
}

// ---- camera + projection ----
struct Cam { V3 eye, look; float fovDeg; };
struct ScreenResult { bool ok; float x,y,z; };
static inline ScreenResult project(const Cam&cam, int W,int H, V3 p){
    V3 f=nrm(cam.look-cam.eye);
    V3 r=nrm(cross(f,{0,1,0}));
    V3 u=cross(r,f);
    V3 d=p-cam.eye;
    float zc=dot(d,f);                 // forward distance
    ScreenResult s{};
    s.ok = zc > 0.08f;
    if(!s.ok) return s;
    float focal = (W*0.5f)/std::tan((cam.fovDeg*0.5f)*0.0174532925f);
    s.x = W*0.5f - dot(d,r)*focal/zc;   // standard right-handed screen mapping
    s.y = H*0.5f - dot(d,u)*focal/zc;
    s.z = zc;
    return s;
}

struct TriG { std::array<float,6> p; double z; Color c; bool w; };
static std::vector<TriG> gbuf;
static inline void frameBegin(){ gbuf.clear(); }
static inline void frameEnd3D(int W,int H){
    (void)W;(void)H;
    std::sort(gbuf.begin(), gbuf.end(), [](const TriG&a,const TriG&b){ return a.z>b.z; });
    BeginBlendMode(BLEND_ALPHA);
    for(const auto&t:gbuf){
        DrawTriangleGradient({t.p[0],t.p[1]}, {t.p[2],t.p[3]}, {t.p[4],t.p[5]}, t.c,t.c,t.c);
        if(t.w){
            Color e=t.c; e.a=e.a/3+34;
            DrawLineV({t.p[0],t.p[1]},{t.p[2],t.p[3]},e);
            DrawLineV({t.p[2],t.p[3]},{t.p[4],t.p[5]},e);
            DrawLineV({t.p[4],t.p[5]},{t.p[0],t.p[1]},e);
        }
    }
    EndBlendMode();
}
struct DrawOpts { Color key{255,255,255,255}; float amb=0.22f; float diff=1.f; float spec=0.35f; bool rainbow=false; int alpha=255; bool wire=false; };
static inline void drawMesh(const Mesh& mesh, const Mat4&mtr, const Cam&cam, int W,int H, const DrawOpts&o){
    static const V3 L = nrm({-0.45f,-0.75f,-0.55f});
    for(auto&fc:mesh){
        std::array<V3,3> w{};
        for(int i2=0;i2<3;i2++) w[i2]=xf(mtr,fc.v[i2]);
        V3 n = nrm(cross(w[1]-w[0], w[2]-w[0]));
        V3 viewDir = nrm((xf(mtr,(fc.v[0]+fc.v[1]+fc.v[2])*0.33333f))-cam.eye);
        if(dot(n,viewDir)>0.f) n = nrm(V3{-n.x,-n.y,-n.z});   // two-sided shading
        ScreenResult s[3]; bool ok=true;
        for(int i3=0;i3<3;i3++){ s[i3]=project(cam,W,H,w[i3]); if(!s[i3].ok){ok=false;break;} }
        if(!ok) continue;
        float ndl=-dot(n,L); if(ndl<0)ndl=0;
        float rim = 1.f+std::fabs(dot(n,nrm(viewDir)));
        Color base = fc.col;
        if(o.rainbow){
            float hue = std::atan2(n.x, n.z)*57.2957795f + 180.f;
            base = ColorFromHSV(((int)(hue) % 360 + 360) % 360, 0.85f, 0.9f);
        }
        float hl = o.amb + o.diff*ndl + o.spec*std::pow(ndl,14.f)*0.5f + 0.05f*rim;
        if(hl<0)hl=0; if(hl>1.0f)hl=1.0f;
        Color kc{ (unsigned char)std::min(255.f,base.r*hl), (unsigned char)std::min(255.f,base.g*hl),
                  (unsigned char)std::min(255.f,base.b*hl), (unsigned char)o.alpha };
        gbuf.push_back(TriG{{ s[0].x,s[0].y, s[1].x,s[1].y, s[2].x,s[2].y }, (double)(s[0].z+s[1].z+s[2].z), kc, o.wire });
    }
}

static inline void grid3d(const Cam&cam,int W,int H,float half,int n,Color col,float fogK){
    auto seg=[&](V3 a,V3 b){
        ScreenResult sa=project(cam,W,H,a), sb=project(cam,W,H,b);
        if(!sa.ok||!sb.ok) return;
        float d=(sa.z+sb.z)*0.5f, f=fogK/std::max(1.2f,d);
        if(f<0.02f) return;
        int na=(int)(col.a*f); if(na>255)na=255; if(na<0)na=0;
        Color k=col; k.a=(unsigned char)na;
        DrawLineV({sa.x,sa.y},{sb.x,sb.y},k);
    };
    float h2=half/n;
    for(int i=0;i<=n;i++){
        float x=-half+i*h2;
        for(int j=0;j<n;j++){
            seg({x,0,-half+j*h2},{x,0,-half+(j+1)*h2});
            seg({-half+j*h2,0,x},{-half+(j+1)*h2,0,x});
        }
    }
}

}} // rmtn::d3