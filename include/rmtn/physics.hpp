#pragma once
#include "scene3d.hpp"
#include <memory>

namespace rmtn { namespace physics {
struct Collider {
    enum Kind { Sphere, Plane, Capsule, Box } kind = Plane;
    Vector3 a{0,0,0}, b{0,1,0}, half{1,1,1}; float radius = 1;
    std::function<Vector3(double)> drive;
    static Collider sphere(Vector3 centre, float radius);
    static Collider plane(Vector3 point, Vector3 normal);
    static Collider capsule(Vector3 a, Vector3 b, float radius);
    static Collider box(Vector3 centre, Vector3 halfExtents);
};
struct Params { Vector3 gravity{0,-9.81f,0}; float damping=0.01f; int substeps=4, iterations=8; float fps=30, friction=0.4f, thickness=0.02f; };
struct Wind { Vector3 direction{1,0,0}; float strength=0, gust=0, gustHz=0.5f; unsigned seed=1; };
struct Pin { int index=0; std::function<Vector3(double)> at; };
class Rope {
public:
    Rope(Vector3 from, Vector3 to, int segments, Params params={});
    Rope& pin(int index, std::function<Vector3(double)> at); Rope& pinEnd(std::function<Vector3(double)> at); Rope& pinEnd();
    Rope& collide(Collider c); Rope& bend(float stiffness); const std::vector<Vector3>& frame(int f) const; float length() const;
private: struct State; std::shared_ptr<State> s_;
};
class Cloth {
public:
    Cloth(int cols,int rows,float spacing,Vector3 origin,Vector3 axisU,Vector3 axisV,Params params={});
    Cloth& pin(int index,std::function<Vector3(double)> at); Cloth& pinCorners(); Cloth& pinRow(int row);
    Cloth& collide(Collider c); Cloth& stretch(float stiffness); Cloth& bend(float stiffness); Cloth& wind(Wind w);
    Cloth& tear(float ratio); Cloth& selfCollision(bool enabled=true);
    const std::vector<Vector3>& frame(int f) const; int liveConstraintCount(int f=0) const;
    int columns() const; int rows() const;
private: struct State; std::shared_ptr<State> s_;
};
Mesh ropeMesh(const Rope&,int frame,float radius,int sides=8);
Mesh clothMesh(const Cloth&,int frame,bool doubleSided=true);
// These objects keep fixed mesh topology. Per-frame CPU vertices are queued through
// Geometry::update; its draw path uploads the edit while the GL context is active.
g3d::Object3D* ropeObject(const Rope&,float radius,g3d::Material material={});
g3d::Object3D* clothObject(const Cloth&,g3d::Material material={});
}}
