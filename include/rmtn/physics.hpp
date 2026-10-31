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
// Wind. By default a push on every particle: `strength` m/s^2 along `direction`, varied by `gust`
// (0..1) at `gustHz`, each particle on its own phase. With `drag` > 0 (cloth only) it is
// aerodynamic instead: `strength` is the wind's speed (m/s) and each triangle is pushed along its
// normal by `drag` (1/s) times the wind it meets, relative to its own motion -- so a cloth edge-on
// to the wind is left alone and one facing it billows. With `wavelength` > 0 the gusts travel
// downwind as waves that long (metres), which ripples a flag from the pole to its free edge.
struct Wind { Vector3 direction{1,0,0}; float strength=0, gust=0, gustHz=0.5f; unsigned seed=1; float drag=0, wavelength=0; };
// An anchor that follows `at(seconds)`. With `until` >= 0 it lets go at that time (an unrolling banner).
struct Pin { int index=0; std::function<Vector3(double)> at; double until=-1; };
class Rope {
public:
    Rope(Vector3 from, Vector3 to, int segments, Params params={});
    Rope& pin(int index, std::function<Vector3(double)> at, double until=-1); Rope& pinEnd(std::function<Vector3(double)> at); Rope& pinEnd();
    Rope& collide(Collider c); Rope& bend(float stiffness); const std::vector<Vector3>& frame(int f) const; float length() const;
private: struct State; std::shared_ptr<State> s_;
};
class Cloth {
public:
    Cloth(int cols,int rows,float spacing,Vector3 origin,Vector3 axisU,Vector3 axisV,Params params={});
    Cloth& pin(int index,std::function<Vector3(double)> at, double until=-1); Cloth& pinCorners(); Cloth& pinRow(int row);
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
