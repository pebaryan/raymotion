#include "rmtn/aircraft3d.hpp"
#include <raymath.h>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace rmtn::aircraft3d {
namespace {

g3d::Material pbr(Color c, float rough, float metal = 0, float opacity = 1) {
    g3d::Material m;
    m.color = c; m.roughness = rough; m.metalness = metal; m.opacity = opacity;
    m.shading = g3d::Shading::Standard;
    return m;
}
const auto paint = pbr({225,232,231,255},.38f,.18f);
const auto wingPaint = pbr({198,211,216,255},.42f,.31f);
const auto navy = pbr({32,67,96,255},.34f,.08f);
const auto glass = pbr({32,75,105,255},.15f,.05f,.78f);
const auto silver = pbr({151,167,176,255},.29f,.78f);
const auto dark = pbr({42,48,55,255},.68f,.12f);
const auto rubber = pbr({31,36,41,255},.89f,.01f);
const auto red = pbr({211,69,58,255},.42f,.08f);
const auto green = pbr({67,171,113,255},.42f,.08f);

g3d::Object3D* part(g3d::Object3D& root, const std::string& name,
                    g3d::GeometryPtr geo, const g3d::Material& material, Vector3 at) {
    auto* n=root.add(std::move(geo),material);
    n->name=name; n->position=at;
    return n;
}
g3d::Object3D* box(g3d::Object3D& root, const std::string& name,
                   float w,float h,float d,const g3d::Material& mat,Vector3 at) {
    return part(root,name,g3d::Geometry::box(w,h,d),mat,at);
}
g3d::Object3D* cyl(g3d::Object3D& root,const std::string& name,float r,float h,
                   const g3d::Material& mat,Vector3 at,int slices=32) {
    return part(root,name,g3d::Geometry::cylinder(r,h,slices),mat,at);
}

// Generates a closed, flat-sided wing or fin from an outline in the x/z plane.
g3d::GeometryPtr slab(std::vector<Vector2> outline,float thickness) {
    return std::make_shared<g3d::Geometry>([outline=std::move(outline),thickness]() {
        std::vector<Vector3> vertices, normals;
        auto tri=[&](Vector3 a,Vector3 b,Vector3 c) {
            Vector3 n=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b,a),Vector3Subtract(c,a)));
            for(Vector3 v:{a,b,c}) { vertices.push_back(v); normals.push_back(n); }
        };
        const float y=thickness*.5f;
        for(size_t i=1;i+1<outline.size();++i) {
            Vector3 a{outline[0].x,y,outline[0].y}, b{outline[i].x,y,outline[i].y};
            Vector3 c{outline[i+1].x,y,outline[i+1].y};
            tri(a,b,c); tri({a.x,-y,a.z},{c.x,-y,c.z},{b.x,-y,b.z});
        }
        for(size_t i=0;i<outline.size();++i) {
            auto a=outline[i],b=outline[(i+1)%outline.size()];
            Vector3 at{a.x,y,a.y},bt{b.x,y,b.y},ab{a.x,-y,a.y},bb{b.x,-y,b.y};
            tri(at,ab,bb); tri(at,bb,bt);
        }
        Mesh m{}; m.vertexCount=(int)vertices.size(); m.triangleCount=m.vertexCount/3;
        m.vertices=(float*)MemAlloc((unsigned)(vertices.size()*3*sizeof(float)));
        m.normals=(float*)MemAlloc((unsigned)(vertices.size()*3*sizeof(float)));
        m.texcoords=(float*)MemAlloc((unsigned)(vertices.size()*2*sizeof(float)));
        for(size_t i=0;i<vertices.size();++i) {
            m.vertices[3*i]=vertices[i].x; m.vertices[3*i+1]=vertices[i].y; m.vertices[3*i+2]=vertices[i].z;
            m.normals[3*i]=normals[i].x; m.normals[3*i+1]=normals[i].y; m.normals[3*i+2]=normals[i].z;
            m.texcoords[2*i]=vertices[i].x*.05f; m.texcoords[2*i+1]=vertices[i].z*.05f;
        }
        UploadMesh(&m,false);
        return m;
    });
}

// Eight stations make the tube, rounded nose and tapered tail one continuous shell.
g3d::GeometryPtr fuselage() {
    return std::make_shared<g3d::Geometry>([]() {
        struct Ring {float z,rx,ry,cy;};
        constexpr std::array<Ring,9> rings{{
            {-16.2f,.08f,.12f,3.18f},{-15.1f,.82f,.90f,3.15f},
            {-12.8f,1.48f,1.48f,3.12f},{-9.0f,1.82f,1.76f,3.10f},
            {2.0f,1.88f,1.80f,3.12f},{9.4f,1.82f,1.75f,3.15f},
            {12.5f,1.55f,1.48f,3.13f},{15.4f,.75f,.67f,3.08f},
            {17.0f,.04f,.05f,3.04f}}};
        constexpr int S=32;
        Mesh m{}; m.triangleCount=(int)(rings.size()-1)*S*2; m.vertexCount=m.triangleCount*3;
        m.vertices=(float*)MemAlloc((unsigned)(m.vertexCount*3*sizeof(float)));
        m.normals=(float*)MemAlloc((unsigned)(m.vertexCount*3*sizeof(float)));
        m.texcoords=(float*)MemAlloc((unsigned)(m.vertexCount*2*sizeof(float)));
        int k=0;
        auto put=[&](const Ring& r,int j) {
            float a=2.f*PI*j/S, x=std::cos(a), y=std::sin(a);
            m.vertices[3*k]=r.rx*x; m.vertices[3*k+1]=r.cy+r.ry*y; m.vertices[3*k+2]=r.z;
            m.normals[3*k]=x; m.normals[3*k+1]=y; m.normals[3*k+2]=0;
            m.texcoords[2*k]=j/(float)S; m.texcoords[2*k+1]=(r.z+17.f)/34.f;
            ++k;
        };
        for(size_t i=0;i+1<rings.size();++i) for(int j=0;j<S;++j) {
            put(rings[i],j); put(rings[i+1],j); put(rings[i+1],j+1);
            put(rings[i],j); put(rings[i+1],j+1); put(rings[i],j+1);
        }
        UploadMesh(&m,false);
        return m;
    });
}

void wheel(g3d::Object3D& root,const std::string& name,Vector3 at) {
    auto* tire=cyl(root,name+"-tire",.52f,.29f,rubber,at,24); tire->rotation.z=90;
    auto* hub=cyl(root,name+"-hub",.24f,.31f,silver,at,24); hub->rotation.z=90;
}
void build(g3d::Object3D& g) {
    auto fus=paint; fus.doubleSided=true;
    part(g,"fuselage-shell",fuselage(),fus,{0,0,0});
    // The navy belt gives the long body a legible passenger-aircraft silhouette.
    for(int side:{-1,1}) {
        const std::string s=side<0?"left":"right";
        for(int i=0;i<16;++i) {
            const float z=-8.8f+i*1.23f;
            auto* n=part(g,s+"-cabin-window-"+std::to_string(i+1),
                         g3d::Geometry::sphere(.205f,10,14),glass,
                         {side*1.78f,3.58f,z});
            n->scale={.20f/.205f,.15f/.205f,.25f/.205f};
        }
        auto* cockpit=part(g,s+"-cockpit-glass",g3d::Geometry::sphere(.72f,16,24),
                           glass,{side*.67f,3.66f,14.02f});
        cockpit->scale={.59f,.43f,1.43f};
        auto wingMat=wingPaint; wingMat.doubleSided=true;
        const float f=(float)side;
        part(g,s+"-main-wing",slab({{f*1.35f,4.4f},{f*3.8f,4.0f},
             {f*17.4f,-2.0f},{f*17.2f,-4.1f},{f*5.4f,-3.9f},
             {f*1.48f,-4.7f}},.31f),wingMat,{0,3.12f,0});
        box(g,s+"-trailing-flap-seam",10.8f,.027f,.052f,silver,
            {f*11.25f,3.29f,-3.88f});
        auto* winglet=box(g,s+"-winglet",.16f,1.08f,.91f,wingPaint,
                           {f*17.25f,3.78f,-3.04f});
        winglet->rotation.z=f*14.f;
        part(g,s+"-horizontal-tail",slab({{f*.8f,-12.0f},{f*2.6f,-11.7f},
             {f*7.15f,-14.05f},{f*7.0f,-15.0f},{f*.85f,-15.15f}},.20f),
             wingMat,{0,4.38f,0});
        // One underwing turbofan per side: metallic lip, shadowed intake and spinner.
        const float ex=f*6.8f, ez=-.25f;
        auto* shell=cyl(g,s+"-engine-nacelle",.98f,3.75f,paint,{ex,1.63f,ez},32);
        shell->rotation.x=90;
        auto* intake=cyl(g,s+"-engine-intake",.83f,.08f,dark,{ex,1.63f,ez+1.92f},32);
        intake->rotation.x=90;
        auto* lip=cyl(g,s+"-engine-rim",.96f,.17f,silver,{ex,1.63f,ez+1.93f},32);
        lip->rotation.x=90;
        auto* spinner=part(g,s+"-engine-spinner",g3d::Geometry::cone(.22f,.52f,24),silver,
                          {ex,1.63f,ez+2.04f}); spinner->rotation.x=90;
        box(g,s+"-pylon",.38f,.84f,1.58f,wingPaint,{ex,2.51f,ez-.35f});
        box(g,s+"-main-strut",.18f,1.70f,.20f,silver,{f*4.25f,1.40f,-2.65f});
        wheel(g,s+"-main-wheel",{f*4.25f,.51f,-2.65f});
        part(g,s+"-wingtip-light",g3d::Geometry::sphere(.18f,12,16),
             side<0?red:green,{f*17.35f,3.15f,-3.05f});
    }
    // Upright fin: slim triangular surface along the rear fuselage.
    auto fin=wingPaint; fin.doubleSided=true;
    auto* tail=part(g,"vertical-tail",slab({{-2.15f,-14.8f},{-1.6f,-10.9f},
        {1.5f,-11.2f},{1.72f,-15.65f}},.18f),fin,{0,7.18f,0});
    tail->rotation.z=90; // upright: original x extent becomes height
    box(g,"fin-colour-flash",.08f,2.18f,2.35f,navy,{0,6.75f,-13.62f});
    box(g,"nose-strut",.17f,1.43f,.18f,silver,{0,1.24f,11.4f});
    wheel(g,"nose-wheel",{0,.50f,11.4f});
}

} // namespace

g3d::Object3D* addPassengerJet(g3d::Object3D& parent) {
    auto* root=parent.add(); root->name="passenger-jet"; build(*root); return root;
}
g3d::Object3D* addPassengerJet(g3d::Scene& scene) {
    auto* root=scene.add(); root->name="passenger-jet"; build(*root); return root;
}

} // namespace rmtn::aircraft3d
