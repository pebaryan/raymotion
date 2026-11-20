// rmtn/flora2d.hpp -- 2D vegetation (flora2d::): grass along a ground line, tufts, a meadow
// filling a shape, shrubs and branching trees, drawn with the node's context so they work
// on a flat composition and on a g3d::Surface alike.
//
//   node->fn = [](const Ctx& c, int t) {
//       const auto line = flora2d::groundLine(0, 1920, 820, 30, 700, 4);
//       flora2d::ground(c, line, 1080, { 70, 92, 48, 255 });
//       flora2d::Wind w;
//       flora2d::grass(c, line, {}, t, 1, 1, w);
//       flora2d::tree(c, { 1300, flora2d::heightAt(line, 1300) }, {}, t, u, 7, w);   // u grows it
//   };
//
// Coordinates are the node's (composition px, y down); things grow up, toward -y. Every
// frame is a function of `t` (wind) and `u` (growth) alone: nothing is kept between frames.
// The wind reads time as frameTime(t), so under motion blur (Comp or Layer::motionBlur) blades
// are drawn at each sampled moment and their tips blur; without it the wind reads t itself.
// Growth follows the 3D convention: u = 0 draws nothing, u = 1 the whole plant; grass
// rises behind a front, a tree's wood runs out from the root over u 0..0.7, its branches
// thickening behind the front, and the leaves come last.
//
// Parallax: draw far layers first with `parallax` (a shifted context), `hazed` colours and,
// on the layer's node, `softness` (a blur filter); far layers want fewer, smaller blades.
#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include "paths.hpp"
#include "timeline.hpp"

namespace rmtn {
namespace flora2d {

// Wind across the picture: a steady lean, a sway and gusts that travel along x.
struct Wind {
    float strength = 1;         // 0 = still
    float direction = 1;        // +1 blows toward +x, -1 toward -x
    float sway = 0.35f;         // sway cycles a second (0 = none)
    float gust = 0.6f;          // gust strength, 0..1
    float gustSpeed = 260;      // px a second the gusts travel
    float gustWidth = 420;      // px across a gust
    float fps = 30;             // frames a second (t is in frames)
};
// How far a plant at `x` bends at frame t, in radians for a fully flexible tip (pure in t).
float bendAt(const Wind& w, float x, int t, float phase = 0);

struct Grass {
    float density = 0.9f;       // blades per px of ground
    float height = 46;          // px
    float heightVariation = 0.45f;
    float width = 3.2f;         // at the root, px
    float lean = 0.18f;         // random lean, radians
    float flex = 1;             // how much the wind bends it
    Color root{ 52, 86, 40, 255 }, tip{ 150, 186, 86, 255 };
    float colorVariation = 0.14f;
    int segments = 4;           // per blade
};
Grass lawn();               // short and even
Grass meadow();             // the default: mixed heights
Grass tallGrass();          // long, soft, leaning
Grass dryGrass();           // straw
Grass grass(const char* name);   // "lawn", "meadow", "tall", "dry"; throws std::invalid_argument

// When each blade comes up as u runs 0..1: 0 first, 1 last (default: along the line / left to right).
using Arrival = std::function<float(Vector2 p)>;

// Blades along a ground polyline (left to right). u reveals them behind a front.
void grass(const Ctx& c, const std::vector<Vector2>& ground, const Grass& g, int t, float u = 1, uint32_t seed = 1,
           const Wind& w = {}, const Arrival& arrival = {});
// A tuft: `count` blades fanning from one point.
void tuft(const Ctx& c, Vector2 at, const Grass& g, int count, int t, float u = 1, uint32_t seed = 1, const Wind& w = {});
// A meadow filling a closed polygon: rows of blades, back (top) to front, smaller toward
// the back by `perspective` (0 = all the same size, 0.6 = the top row 40% of the front's).
void meadow(const Ctx& c, const std::vector<Vector2>& shape, const Grass& g, int t, float u = 1, uint32_t seed = 1,
            const Wind& w = {}, float perspective = 0.5f, const Arrival& arrival = {});
// The same filling a vector path: every closed contour (open ones close themselves) is
// flattened to within 0.1 px, so a root is never more than that outside a curved edge. `rule`
// is vg::fill's: with NonZero (the default) a hole is a contour wound the other way; with
// EvenOdd any inner contour is a hole. Holes get no blades.
void meadow(const Ctx& c, const vg::Path& shape, const Grass& g, int t, float u = 1, uint32_t seed = 1, const Wind& w = {},
            float perspective = 0.5f, const Arrival& arrival = {}, vg::FillRule rule = vg::FillRule::NonZero);
// Where a meadow's blades root (node space, back to front), for the same arguments as
// `meadow` (growth, wind and arrival don't move roots). The same shape and seed give the same roots.
std::vector<Vector2> meadowRoots(const std::vector<Vector2>& shape, const Grass& g, uint32_t seed = 1, float perspective = 0.5f);
std::vector<Vector2> meadowRoots(const vg::Path& shape, const Grass& g, uint32_t seed = 1, float perspective = 0.5f,
                                 vg::FillRule rule = vg::FillRule::NonZero);

struct Shrub {
    float size = 70;            // px across
    float height = 54;          // px
    int stems = 5;
    Color stem{ 74, 58, 40, 255 };
    Color dark{ 38, 70, 40, 255 }, mid{ 60, 104, 52, 255 }, light{ 110, 150, 72, 255 };
    float leafSize = 0.2f;      // of `size`
    int leaves = 34;            // leaf blobs
    float flex = 0.5f;
};
void shrub(const Ctx& c, Vector2 base, const Shrub& s, int t, float u = 1, uint32_t seed = 1, const Wind& w = {});

enum class TreeForm { Broadleaf, Conifer };
struct Tree {
    TreeForm form = TreeForm::Broadleaf;
    float height = 320;         // px, root to crown top
    float trunkWidth = 22;      // px at the root
    float spread = 0.55f;       // branch angle from the parent, radians
    int depth = 6;              // levels of branching (broadleaf)
    float branchLength = 0.72f; // each level's length, of its parent's
    float gnarl = 0.25f;        // irregularity, 0..1
    Color bark{ 70, 54, 42, 255 };
    Color dark{ 34, 66, 38, 255 }, mid{ 56, 98, 50, 255 }, light{ 104, 146, 70, 255 };
    float leafSize = 26;        // px, a leaf clump's radius
    float leafDensity = 1;      // clumps per twig
    float flex = 1;
    bool silhouette = false;    // one colour (`dark`) for far layers
};
Tree oak();
Tree birch();               // slender, pale bark, small light crown
Tree pine();                // conifer: tiers on a straight stem
Tree tree(const char* name);   // "oak", "birch", "pine"; throws std::invalid_argument
void tree(const Ctx& c, Vector2 base, const Tree& s, int t, float u = 1, uint32_t seed = 1, const Wind& w = {});

// ---- layers and ground
// A rolling ground line from x0 to x1 about y: `amplitude` px of hills `wavelength` px apart.
std::vector<Vector2> groundLine(float x0, float x1, float y, float amplitude, float wavelength, uint32_t seed, int points = 64);
// The y of a polyline at x (clamped to its ends).
float heightAt(const std::vector<Vector2>& line, float x);
// Fills under a ground line down to `bottom`.
void ground(const Ctx& c, const std::vector<Vector2>& line, float bottom, Color color);
// A colour seen through `depth` (0 near .. 1 far) of haze.
Color hazed(Color c, float depth, Color haze);
// The context for a layer at `depth` (0 moves with the camera's offset, 1 stays put).
Ctx parallax(const Ctx& c, float depth, Vector2 camera);
// A layer filter that blurs everything by `px` (for TNode::isolate().filters).
Filter softness(float px);

}  // namespace flora2d
}  // namespace rmtn
