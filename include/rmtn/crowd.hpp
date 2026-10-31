// rmtn/crowd.hpp -- crowds, herds, flocks and schools (crowd::): groups of agents that steer
// as boids (Reynolds 1987, "Flocks, herds and schools"): apart from their neighbours, along
// with them, towards them, plus goals, obstacles, bounds and things to run from. The agents
// wear rig characters and animals whose gaits follow their speed, so feet stay planted.
//
//   crowd::World w;
//   crowd::Group people;  people.name = "people"; people.count = 30;
//   people.spawn = { 0, 0, -8 }; people.spawnSize = { 10, 0, 3 };
//   people.goal = [](double s, int i) { return Vector3{ (i % 5 - 2) * 2.f, 0, s < 6 ? 8.f : -8.f }; };
//   w.groups.push_back(people);
//   w.obstacles.push_back({ { 0, 0, 0 }, 1.5f });                    // a fountain
//   w.to = 15;
//   crowd::CrowdPtr c = crowd::bake(w);                               // once, in build
//   auto chars = crowd::cast(c, "people", [](int i) { return crowd::person(i); });
//   crowd::addAll(*scene, c, chars);                                  // 3D; or crowd::draw (2D)
//
// The simulation is stateful, but frames are not: bake() steps the whole world at a fixed
// dt once, deterministically (seeded, synchronous updates), and stores every agent's path.
// A frame only samples those paths, so any frame can be drawn alone, out of order or twice.
// Positions are metres in the scene's space; the ground is y = 0; times are seconds.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "rig.hpp"

namespace rmtn {
namespace crowd {

// Ground agents walk on y = 0 and move only where they face (no sliding sideways); Air and
// Water agents move in 3D, never stop, and keep to their group's height band.
enum class Medium { Ground, Air, Water };

// Steering weights (dimensionless) and radii (metres).
struct Steer {
    float separation = 1.5f;                 // away from neighbours inside `personal`
    float alignment = 1;                     // turn to the neighbours' mean heading
    float cohesion = 1;                      // towards the neighbours' centre (own group)
    float goal = 1;                          // towards Group::goal, slowing on arrival
    float wander = 0.3f;                     // a slow random drift of heading
    float avoid = 3;                         // round obstacles, and past oncoming agents
    float flee = 4;                          // away from what the group flees
    float bounds = 2;                        // back inside the world's bounds
    float personal = 0.6f;                   // the space an agent keeps around itself
    float neighbour = 3;                     // how far it looks for flockmates: several times
                                             //   `personal`, or separation spreads the group
                                             //   past each other's sight and it splinters
    float fov = 270;                         // degrees of view (behind it is blind)
    float panic = 6;                         // how close a threat must be to be fled
};

struct Group {
    std::string name;
    Medium medium = Medium::Ground;
    int count = 10;
    Vector3 spawn{ 0, 0, 0 }, spawnSize{ 4, 0, 4 };    // the box the agents start in (centre, size)
    Vector3 heading0{ 0, 0, 1 };             // their first heading,
    float headingSpread = 40;                // jittered across this many degrees
    float cruise = 1.3f;                     // m/s when nothing hurries them
    float maxSpeed = 2.5f;                   // m/s when fleeing
    float maxAccel = 2;                      // m/s^2
    float maxTurn = 180;                     // degrees per second
    float speedJitter = 0.1f;                // each agent's cruise varies by this fraction
    float sizeJitter = 0.06f;                // and its size (crowd::cast scales the build)
    float height0 = 0, height1 = 0;          // Air/Water: the band they keep to (y, metres)
    float rest = 0;                          // 0..1: the share of time a calm agent stands still
    Steer steer;
    // Where agent i (0..count-1 in this group) heads at time s; empty: it wanders. A pure
    // function of time, so goals can change on cue.
    std::function<Vector3(double s, int i)> goal;
    std::vector<std::string> flees;          // group or predator names it runs from
    // A region the group keeps inside, such as a pond, a pen or a paddock: a vertical cylinder
    // round penAt (radius 0: none). A soft wall turns agents back before its edge, even in
    // flight from a threat; the edge itself is hard.
    Vector3 penAt{ 0, 0, 0 };
    float penRadius = 0;
};

struct Obstacle {                            // a vertical cylinder standing on the ground
    Vector3 at{ 0, 0, 0 };
    float radius = 1;
    float height = 1e9f;                     // Air/Water agents above it pass over
};
// A threat that is not an agent (a car, the camera, a thrown ball): where it is at time s.
struct Predator {
    std::string name;
    std::function<Vector3(double s)> at;
};

struct World {
    std::vector<Group> groups;
    std::vector<Obstacle> obstacles;
    std::vector<Predator> predators;
    Vector3 boundsMin{ -20, 0, -20 }, boundsMax{ 20, 20, 20 };
    double from = 0, to = 10;                // the time the bake covers (sampling clamps outside)
    double dt = 1.0 / 60;
    unsigned long long seed = 1;
};

// An agent at a moment. yaw: degrees about +y, 0 facing +z (as Object3D::rotation.y and
// rig characters); pitch: about x, negative nose up; bank: about z, negative rolls the left
// side (+x) down, into a left turn. Unwrapped: yaw keeps counting past 360.
struct State {
    Vector3 pos{ 0, 0, 0 }, vel{ 0, 0, 0 };
    float yaw = 0, pitch = 0, bank = 0, speed = 0;
};

class Crowd;
using CrowdPtr = std::shared_ptr<const Crowd>;

// Runs the simulation over [w.from, w.to]. Agents are numbered group by group in order.
CrowdPtr bake(const World& w);

class Crowd {
public:
    int size() const { return (int)agents_.size(); }
    const World& world() const { return world_; }
    int group(int agent) const { return agents_[(size_t)agent].group; }
    const Group& groupOf(int agent) const { return world_.groups[(size_t)group(agent)]; }
    int index(int agent) const { return agents_[(size_t)agent].index; }   // within its group
    int first(const std::string& group) const;          // first agent of a group (throws if unknown)
    float sizeFactor(int agent) const { return agents_[(size_t)agent].size; }
    State at(int agent, double s) const;                 // smooth between steps, clamped outside
    double travelled(int agent, double s) const;         // metres covered since `from`
    bool resting(int agent, double s) const;             // a calm agent's pause (Group::rest)

private:
    friend CrowdPtr bake(const World& w);
    struct Sample { Vector3 pos, vel; float yaw, pitch, bank; double dist; bool rest; };
    struct Agent {
        int group = 0, index = 0;
        float size = 1, cruise = 1;
        std::vector<Sample> path;
    };
    World world_;
    std::vector<Agent> agents_;
    int steps_ = 0;
};

// ------------------------------------------------------------------ wearing characters
// Fills `m` with the agent's locomotion: the gait its speed calls for (humanoids idle, walk,
// run; four legs stand, walk, trot, canter, gallop; two-legged animals walk and run; snakes
// and fish slither and swim; birds in the air flap and glide), crossfaded at each change.
// Each gait is time-warped by the ground covered, so the feet keep their stride at any speed.
void locomote(rig::Motion& m, CrowdPtr c, int agent, const rig::Build& b);

// One character per agent of `group`, wearing look(i) (i: the index within the group) with
// its build scaled by the agent's size and its motion from locomote(). Agents from `faces`
// on get no face: each face is a texture drawn every frame.
std::vector<rig::CharacterPtr> cast(CrowdPtr c, const std::string& group, std::function<rig::Look(int i)> look,
                                    int faces = 12);

// Varied looks from a small palette, so outfits are shared between agents.
rig::Look person(int i, unsigned long long seed = 7);
rig::Look herd(const std::string& species, int i, unsigned long long seed = 7);

// ---- 3D: the agent's character under a node that follows it (position, yaw; pitch and
// bank for Air and Water). Returns that node.
g3d::Object3D* add(g3d::Scene& scene, CrowdPtr c, int agent, rig::CharacterPtr ch, int fps = 30);
// Every character of cast(), in order: chars[k] wears agent first(group) + k.
void addAll(g3d::Scene& scene, CrowdPtr c, const std::string& group, const std::vector<rig::CharacterPtr>& chars,
            int fps = 30);

// ---- 2D: the group's characters as cut-outs, back to front. `view` is the shared camera:
// its origin is where the world's (0, 0, 0) lands; each agent is offset by its projected
// position and turned by its yaw.
void draw(const Ctx& cx, CrowdPtr c, const std::string& group, const std::vector<rig::CharacterPtr>& chars, double s,
          const rig::View2D& view);

} // namespace crowd
} // namespace rmtn
