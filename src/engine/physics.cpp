#include "rmtn/physics.hpp"
#include "rmtn/timeline.hpp"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <cstdlib>
#include <map>
#include <tuple>
#include <utility>

namespace rmtn {
namespace physics {
namespace {

struct Link {
    int a = 0, b = 0;
    float rest = 0, weight = 1;
    bool live = true;
};

struct Sim {
    Params params;
    int count = 0, columns = 0, rows = 0;
    std::vector<Vector3> positions, previous;
    std::vector<Link> structural, shear, bending;
    std::vector<Pin> pins;
    mutable std::vector<char> pinMask;   // which particles are pinned, built when the simulation starts
    std::vector<Collider> colliders;
    mutable std::deque<std::vector<Vector3>> cache;   // a deque: frame() hands out references that must survive later growth
    float stretch = 1, bend = 1, tearRatio = 0;
    Wind wind;
    bool cloth = false, selfCollision = false;
};

Vector3 add(Vector3 a, Vector3 b) { return Vector3Add(a, b); }
Vector3 sub(Vector3 a, Vector3 b) { return Vector3Subtract(a, b); }
Vector3 scale(Vector3 a, float k) { return Vector3Scale(a, k); }
float dot(Vector3 a, Vector3 b) { return Vector3DotProduct(a, b); }
float magnitude(Vector3 a) { return Vector3Length(a); }
Vector3 unit(Vector3 a) { const float n = magnitude(a); return n > 1e-8f ? scale(a, 1 / n) : Vector3{ 0, 1, 0 }; }
double seconds(const Sim& s, int frame) { return frame / (double)std::max(1.0f, s.params.fps); }
bool pinned(const Sim& s, int index) {
    return index >= 0 && index < (int)s.pinMask.size() && s.pinMask[(size_t)index];
}

Vector3 colliderPosition(const Collider& c, double t) { return c.drive ? c.drive(t) : c.a; }

// Pushes `p` out of the collider. Returns true on contact and sets `normal` to the way out.
bool projectCollider(Vector3& p, const Collider& c, double t, float thickness, Vector3& normal) {
    const float r = c.radius + thickness;
    if (c.kind == Collider::Plane) {
        const Vector3 n = unit(c.b);
        const float d = dot(sub(p, c.a), n);
        if (d >= thickness) return false;
        p = add(p, scale(n, thickness - d));
        normal = n;
        return true;
    }
    if (c.kind == Collider::Sphere || c.kind == Collider::Capsule) {
        Vector3 centre = colliderPosition(c, t);
        if (c.kind == Collider::Capsule) {
            const Vector3 ab = sub(c.b, centre);
            const float u = std::clamp(dot(sub(p, centre), ab) / std::max(dot(ab, ab), 1e-8f), 0.0f, 1.0f);
            centre = add(centre, scale(ab, u));
        }
        const Vector3 delta = sub(p, centre);
        if (magnitude(delta) >= r) return false;
        normal = unit(delta);
        p = add(centre, scale(normal, r));
        return true;
    }
    // Box: leave through the nearest face.
    const Vector3 centre = colliderPosition(c, t), q = sub(p, centre);
    const float x = c.half.x + thickness - std::fabs(q.x);
    const float y = c.half.y + thickness - std::fabs(q.y);
    const float z = c.half.z + thickness - std::fabs(q.z);
    if (x <= 0 || y <= 0 || z <= 0) return false;
    if (x < y && x < z) {
        const float side = q.x < 0 ? -1.0f : 1.0f;
        p.x = centre.x + side * (c.half.x + thickness);
        normal = { side, 0, 0 };
    } else if (y < z) {
        const float side = q.y < 0 ? -1.0f : 1.0f;
        p.y = centre.y + side * (c.half.y + thickness);
        normal = { 0, side, 0 };
    } else {
        const float side = q.z < 0 ? -1.0f : 1.0f;
        p.z = centre.z + side * (c.half.z + thickness);
        normal = { 0, 0, side };
    }
    return true;
}

void applyPins(Sim& s, double t) {
    for (const Pin& pin : s.pins) {
        if (pin.index < 0 || pin.index >= s.count || !pin.at) continue;
        const Vector3 p = pin.at(t);
        s.positions[pin.index] = p;
        s.previous[pin.index] = p;
    }
}

void solveLinks(Sim& s, std::vector<Link>& links, float stiffness) {
    for (Link& link : links) {
        if (!link.live) continue;
        const Vector3 delta = sub(s.positions[link.b], s.positions[link.a]);
        const float distance = magnitude(delta);
        if (distance < 1e-8f) continue;
        const Vector3 correction = scale(delta, ((distance - link.rest) / distance) * stiffness * link.weight);
        const bool pinA = pinned(s, link.a), pinB = pinned(s, link.b);
        if (!pinA && !pinB) {
            s.positions[link.a] = add(s.positions[link.a], scale(correction, 0.5f));
            s.positions[link.b] = sub(s.positions[link.b], scale(correction, 0.5f));
        } else if (pinA && !pinB) s.positions[link.b] = sub(s.positions[link.b], correction);
        else if (pinB && !pinA) s.positions[link.a] = add(s.positions[link.a], correction);
        if (s.tearRatio > 0 && distance / link.rest > s.tearRatio) link.live = false;
    }
}

// Hash cells keep self collision local; adjacent cells cover every possible overlap.
using Cell = std::tuple<int, int, int>;
Cell cellFor(Vector3 p, float size) {
    return { (int)std::floor(p.x / size), (int)std::floor(p.y / size), (int)std::floor(p.z / size) };
}
void solveSelfCollision(Sim& s) {
    if (!s.selfCollision || !s.cloth || s.params.thickness <= 0) return;
    const float diameter = s.params.thickness * 2;
    std::map<Cell, std::vector<int>> grid;
    for (int i = 0; i < s.count; ++i) grid[cellFor(s.positions[i], diameter)].push_back(i);
    for (int i = 0; i < s.count; ++i) {
        const auto [cx, cy, cz] = cellFor(s.positions[i], diameter);
        for (int z = cz - 1; z <= cz + 1; ++z)
            for (int y = cy - 1; y <= cy + 1; ++y)
                for (int x = cx - 1; x <= cx + 1; ++x) {
                    const auto found = grid.find({ x, y, z });
                    if (found == grid.end()) continue;
                    for (int j : found->second) {
                        if (j <= i || std::abs(j % s.columns - i % s.columns) <= 1 && std::abs(j / s.columns - i / s.columns) <= 1) continue;
                        Vector3 d = sub(s.positions[j], s.positions[i]);
                        const float n = magnitude(d);
                        if (n > 1e-8f && n < diameter) {
                            const Vector3 correction = scale(d, (diameter - n) / n * 0.5f);
                            if (!pinned(s, i)) s.positions[i] = sub(s.positions[i], correction);
                            if (!pinned(s, j)) s.positions[j] = add(s.positions[j], correction);
                        }
                    }
                }
    }
}

void step(Sim& s, int frame) {
    const double t = seconds(s, frame);
    const float substeps = (float)std::max(1, s.params.substeps);
    const float dt = 1 / (std::max(1.0f, s.params.fps) * substeps);
    for (int substep = 0; substep < (int)substeps; ++substep) {
        const double stepTime = (frame - 1 + (substep + 1) / substeps) / std::max(1.0f, s.params.fps);
        applyPins(s, stepTime);
        for (int i = 0; i < s.count; ++i) {
            if (pinned(s, i)) continue;
            Vector3 velocity = scale(sub(s.positions[i], s.previous[i]), 1 - std::clamp(s.params.damping, 0.0f, 1.0f));
            s.previous[i] = s.positions[i];
            Vector3 force = s.params.gravity;
            if (s.wind.strength != 0) {
                const double phase = rrange(s.wind.seed, (unsigned long long)i, 0, 6.283185307179586);
                const float gust = 1 + s.wind.gust * std::sin((float)(stepTime * s.wind.gustHz * 6.2831853) + (float)phase);
                force = add(force, scale(unit(s.wind.direction), s.wind.strength * gust));
            }
            s.positions[i] = add(add(s.positions[i], velocity), scale(force, dt * dt));
        }
        for (int iteration = 0; iteration < std::max(1, s.params.iterations); ++iteration) {
            solveLinks(s, s.structural, s.stretch);
            solveLinks(s, s.shear, s.stretch);
            solveLinks(s, s.bending, s.bend);
            applyPins(s, stepTime);
            // Friction is a share of the tangential motion removed at each contact. The solver
            // runs `iterations` times a substep, so each pass takes the root of the total.
            const float friction = 1 - std::pow(1 - std::clamp(s.params.friction, 0.0f, 1.0f),
                                                1.0f / std::max(1, s.params.iterations));
            for (int i = 0; i < s.count; ++i) {
                if (pinned(s, i)) continue;
                for (const Collider& c : s.colliders) {
                    Vector3 normal{ 0, 1, 0 };
                    if (!projectCollider(s.positions[i], c, stepTime, s.params.thickness, normal)) continue;
                    const Vector3 velocity = sub(s.positions[i], s.previous[i]);
                    const Vector3 tangential = sub(velocity, scale(normal, dot(velocity, normal)));
                    s.previous[i] = add(s.previous[i], scale(tangential, friction));
                }
            }
            solveSelfCollision(s);
        }
    }
    applyPins(s, t);
}

void ensure(Sim& s, int frame) {
    if (s.cache.empty()) {
        s.pinMask.assign((size_t)s.count, 0);
        for (const Pin& pin : s.pins)
            if (pin.at && pin.index >= 0 && pin.index < s.count) s.pinMask[(size_t)pin.index] = 1;
        applyPins(s, 0);
        s.cache.push_back(s.positions);
    }
    while ((int)s.cache.size() <= frame) {
        step(s, (int)s.cache.size());
        s.cache.push_back(s.positions);
    }
}

Mesh allocateMesh(int vertices, int indices) {
    Mesh mesh{};
    mesh.vertexCount = vertices;
    mesh.triangleCount = indices / 3;
    mesh.vertices = (float*)std::calloc((size_t)vertices * 3, sizeof(float));
    mesh.normals = (float*)std::calloc((size_t)vertices * 3, sizeof(float));
    mesh.texcoords = (float*)std::calloc((size_t)vertices * 2, sizeof(float));
    mesh.indices = (unsigned short*)std::calloc((size_t)indices, sizeof(unsigned short));
    return mesh;
}
void put(Mesh& mesh, int i, Vector3 p, Vector3 n, float u, float v) {
    mesh.vertices[i * 3] = p.x; mesh.vertices[i * 3 + 1] = p.y; mesh.vertices[i * 3 + 2] = p.z;
    mesh.normals[i * 3] = n.x; mesh.normals[i * 3 + 1] = n.y; mesh.normals[i * 3 + 2] = n.z;
    mesh.texcoords[i * 2] = u; mesh.texcoords[i * 2 + 1] = v;
}
} // namespace

Collider Collider::sphere(Vector3 centre, float r) {
    Collider c;
    c.kind = Sphere;
    c.a = centre;
    c.radius = r;
    return c;
}

Collider Collider::plane(Vector3 point, Vector3 normal) {
    Collider c;
    c.kind = Plane;
    c.a = point;
    c.b = normal;
    return c;
}

Collider Collider::capsule(Vector3 from, Vector3 to, float r) {
    Collider c;
    c.kind = Capsule;
    c.a = from;
    c.b = to;
    c.radius = r;
    return c;
}

Collider Collider::box(Vector3 centre, Vector3 halfExtents) {
    Collider c;
    c.kind = Box;
    c.a = centre;
    c.half = halfExtents;
    return c;
}

struct Rope::State : Sim {};
struct Cloth::State : Sim {};

Rope::Rope(Vector3 from, Vector3 to, int segments, Params params) : s_(std::make_shared<State>()) {
    Sim& s = *s_;
    s.params = params;
    s.count = std::max(1, segments) + 1;
    s.positions.resize(s.count);
    s.previous.resize(s.count);
    for (int i = 0; i < s.count; ++i)
        s.positions[i] = s.previous[i] = Vector3Lerp(from, to, (float)i / (s.count - 1));
    const float rest = magnitude(sub(to, from)) / (s.count - 1);
    for (int i = 0; i + 1 < s.count; ++i) s.structural.push_back({ i, i + 1, rest });
}

Rope& Rope::pin(int i, std::function<Vector3(double)> at) {
    s_->pins.push_back({ i, std::move(at) });
    return *this;
}

Rope& Rope::pinEnd(std::function<Vector3(double)> at) { return pin(s_->count - 1, std::move(at)); }

// Holds the last particle where it starts. Pin before the first frame() call.
Rope& Rope::pinEnd() {
    const Vector3 p = s_->positions.back();
    return pinEnd([p](double) { return p; });
}

Rope& Rope::collide(Collider c) {
    s_->colliders.push_back(std::move(c));
    return *this;
}

Rope& Rope::bend(float stiffness) {
    s_->bend = std::clamp(stiffness, 0.0f, 1.0f);
    if (s_->bending.empty()) {
        for (int i = 0; i + 2 < s_->count; ++i)
            s_->bending.push_back({ i, i + 2, magnitude(sub(s_->positions[i + 2], s_->positions[i])) });
    }
    return *this;
}

const std::vector<Vector3>& Rope::frame(int f) const {
    ensure(*s_, std::max(0, f));
    return s_->cache[(size_t)std::max(0, f)];
}

float Rope::length() const {
    float total = 0;
    for (size_t i = 1; i < s_->positions.size(); ++i)
        total += magnitude(sub(s_->positions[i], s_->positions[i - 1]));
    return total;
}

Cloth::Cloth(int cols, int rows, float spacing, Vector3 origin, Vector3 axisU, Vector3 axisV, Params params)
    : s_(std::make_shared<State>()) {
    Sim& s = *s_;
    s.params = params;
    s.cloth = true;
    s.columns = std::max(2, cols);
    s.rows = std::max(2, rows);
    s.count = s.columns * s.rows;
    s.positions.resize(s.count);
    s.previous.resize(s.count);
    for (int y = 0; y < s.rows; ++y) {
        for (int x = 0; x < s.columns; ++x) {
            const int i = y * s.columns + x;
            s.positions[i] = s.previous[i] = add(add(origin, scale(axisU, x * spacing)), scale(axisV, y * spacing));
        }
    }
    auto link = [&](std::vector<Link>& list, int a, int b, float weight = 1) {
        list.push_back({ a, b, magnitude(sub(s.positions[b], s.positions[a])), weight });
    };
    for (int y = 0; y < s.rows; ++y) {
        for (int x = 0; x < s.columns; ++x) {
            const int i = y * s.columns + x;
            if (x + 1 < s.columns) link(s.structural, i, i + 1);
            if (y + 1 < s.rows) link(s.structural, i, i + s.columns);
            if (x + 1 < s.columns && y + 1 < s.rows) {
                link(s.shear, i, i + s.columns + 1);
                link(s.shear, i + 1, i + s.columns);
            }
            if (x + 2 < s.columns) link(s.bending, i, i + 2);
            if (y + 2 < s.rows) link(s.bending, i, i + 2 * s.columns);
        }
    }
}

Cloth& Cloth::pin(int i, std::function<Vector3(double)> at) {
    s_->pins.push_back({ i, std::move(at) });
    return *this;
}

// The corner and row pins hold particles where they start. They are copied by value: a lambda
// holding the state itself would be a reference cycle. Pin before the first frame() call.
Cloth& Cloth::pinCorners() {
    for (int i : { 0, s_->columns - 1 }) {
        const Vector3 p = s_->positions[(size_t)i];
        pin(i, [p](double) { return p; });
    }
    return *this;
}

Cloth& Cloth::pinRow(int row) {
    const int y = std::clamp(row, 0, s_->rows - 1);
    for (int x = 0; x < s_->columns; ++x) {
        const int i = y * s_->columns + x;
        const Vector3 p = s_->positions[(size_t)i];
        pin(i, [p](double) { return p; });
    }
    return *this;
}

Cloth& Cloth::collide(Collider c) {
    s_->colliders.push_back(std::move(c));
    return *this;
}

Cloth& Cloth::stretch(float k) {
    s_->stretch = std::clamp(k, 0.0f, 1.0f);
    return *this;
}

Cloth& Cloth::bend(float k) {
    s_->bend = std::clamp(k, 0.0f, 1.0f);
    return *this;
}

Cloth& Cloth::wind(Wind w) {
    s_->wind = w;
    return *this;
}

Cloth& Cloth::tear(float ratio) {
    s_->tearRatio = std::max(0.0f, ratio);
    return *this;
}

Cloth& Cloth::selfCollision(bool enabled) {
    s_->selfCollision = enabled;
    return *this;
}

const std::vector<Vector3>& Cloth::frame(int f) const {
    ensure(*s_, std::max(0, f));
    return s_->cache[(size_t)std::max(0, f)];
}

int Cloth::liveConstraintCount(int f) const {
    ensure(*s_, std::max(0, f));
    int live = 0;
    for (const std::vector<Link>* links : { &s_->structural, &s_->shear, &s_->bending })
        live += (int)std::count_if(links->begin(), links->end(), [](const Link& l) { return l.live; });
    return live;
}

int Cloth::columns() const { return s_->columns; }
int Cloth::rows() const { return s_->rows; }

Mesh ropeMesh(const Rope& rope, int frame, float radius, int sides) {
    const auto& p = rope.frame(frame); sides = std::max(3, sides);
    Mesh mesh = allocateMesh((int)p.size() * sides + 2, ((int)p.size() - 1) * sides * 6 + sides * 6);
    Vector3 tangent = unit(sub(p[1], p[0]));
    Vector3 u = unit(Vector3CrossProduct(tangent, std::fabs(tangent.y) < 0.9f ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 }));
    Vector3 v = unit(Vector3CrossProduct(tangent, u));
    for (size_t i = 0; i < p.size(); ++i) {
        if (i > 0) { tangent = unit(sub(p[i], p[i - 1])); u = unit(Vector3CrossProduct(tangent, v)); v = unit(Vector3CrossProduct(u, tangent)); }
        for (int j = 0; j < sides; ++j) {
            const float a = 2 * PI * j / sides;
            const Vector3 n = add(scale(u, std::cos(a)), scale(v, std::sin(a)));
            put(mesh, (int)i * sides + j, add(p[i], scale(n, radius)), n, (float)i / (p.size() - 1), (float)j / sides);
        }
    }
    int k = 0;
    for (size_t i = 0; i + 1 < p.size(); ++i) for (int j = 0; j < sides; ++j) {
        const int a = (int)i * sides + j, b = (int)i * sides + (j + 1) % sides, c = (int)(i + 1) * sides + j, d = (int)(i + 1) * sides + (j + 1) % sides;
        mesh.indices[k++] = (unsigned short)a; mesh.indices[k++] = (unsigned short)b; mesh.indices[k++] = (unsigned short)c;
        mesh.indices[k++] = (unsigned short)b; mesh.indices[k++] = (unsigned short)d; mesh.indices[k++] = (unsigned short)c;
    }
    const int start = (int)p.size() * sides, end = start + 1;
    put(mesh, start, p.front(), scale(tangent, -1), 0.5f, 0.5f);
    put(mesh, end, p.back(), tangent, 0.5f, 0.5f);
    for (int j = 0; j < sides; ++j) {
        const int a = (j + 1) % sides, b = j;
        mesh.indices[k++] = (unsigned short)start; mesh.indices[k++] = (unsigned short)a; mesh.indices[k++] = (unsigned short)b;
        const int c = ((int)p.size() - 1) * sides + j, d = ((int)p.size() - 1) * sides + (j + 1) % sides;
        mesh.indices[k++] = (unsigned short)end; mesh.indices[k++] = (unsigned short)c; mesh.indices[k++] = (unsigned short)d;
    }
    return mesh;
}

Mesh clothMesh(const Cloth& cloth, int frame, bool doubleSided) {
    const auto& p = cloth.frame(frame); const int cols = cloth.columns(), rows = cloth.rows();
    const int faces = (cols - 1) * (rows - 1) * 6;
    Mesh mesh = allocateMesh((int)p.size() * (doubleSided ? 2 : 1), faces * (doubleSided ? 2 : 1));
    for (int y = 0; y < rows; ++y) for (int x = 0; x < cols; ++x) {
        const int i = y * cols + x;
        const Vector3 dx = sub(p[y * cols + std::min(x + 1, cols - 1)], p[y * cols + std::max(x - 1, 0)]);
        const Vector3 dy = sub(p[std::min(y + 1, rows - 1) * cols + x], p[std::max(y - 1, 0) * cols + x]);
        const Vector3 n = unit(Vector3CrossProduct(dx, dy));
        put(mesh, i, p[i], n, (float)x / (cols - 1), (float)y / (rows - 1));
        if (doubleSided) put(mesh, (int)p.size() + i, p[i], scale(n, -1), (float)x / (cols - 1), (float)y / (rows - 1));
    }
    int k = 0;
    for (int y = 0; y < rows - 1; ++y) for (int x = 0; x < cols - 1; ++x) {
        const int a = y * cols + x, b = a + 1, c = a + cols, d = c + 1;
        const int front[6] = { a, c, b, b, c, d };
        for (int idx : front) mesh.indices[k++] = (unsigned short)idx;
        if (doubleSided) for (int idx : { a, b, c, b, d, c }) mesh.indices[k++] = (unsigned short)(idx + p.size());
    }
    return mesh;
}

g3d::Object3D* ropeObject(const Rope& rope, float radius, g3d::Material material) {
    auto geometry = std::make_shared<g3d::Geometry>([rope, radius] { Mesh m = ropeMesh(rope, 0, radius); UploadMesh(&m, true); return m; });
    auto* object = new g3d::Object3D(); object->geometry = geometry; object->material = material;
    object->up = [rope, geometry, radius](int frame, g3d::Object3D&) {
        Mesh next = ropeMesh(rope, frame, radius);
        std::vector<float> vertices(next.vertices, next.vertices + next.vertexCount * 3);
        std::vector<float> normals(next.normals, next.normals + next.vertexCount * 3);
        geometry->update([vertices = std::move(vertices), normals = std::move(normals)](Mesh& mesh) {
            std::copy(vertices.begin(), vertices.end(), mesh.vertices);
            std::copy(normals.begin(), normals.end(), mesh.normals);
        });
        UnloadMesh(next);
    };
    return object;
}
g3d::Object3D* clothObject(const Cloth& cloth, g3d::Material material) {
    auto geometry = std::make_shared<g3d::Geometry>([cloth] { Mesh m = clothMesh(cloth, 0); UploadMesh(&m, true); return m; });
    auto* object = new g3d::Object3D(); object->geometry = geometry; object->material = material;
    object->up = [cloth, geometry](int frame, g3d::Object3D&) {
        Mesh next = clothMesh(cloth, frame);
        std::vector<float> vertices(next.vertices, next.vertices + next.vertexCount * 3);
        std::vector<float> normals(next.normals, next.normals + next.vertexCount * 3);
        geometry->update([vertices = std::move(vertices), normals = std::move(normals)](Mesh& mesh) {
            std::copy(vertices.begin(), vertices.end(), mesh.vertices);
            std::copy(normals.begin(), normals.end(), mesh.normals);
        });
        UnloadMesh(next);
    };
    return object;
}

} // namespace physics
} // namespace rmtn
