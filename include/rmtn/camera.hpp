// rmtn/camera.hpp -- cam:: a 2D camera that pans, zooms and turns over a canvas bigger than
// the frame. Everything under a cam::world node moves with the camera (shapes, text,
// paths, plots, diagrams, images); nodes outside it stay put, like a HUD.
//
//   cam::Track track;
//   track.start(cam::Shot::fit({ 0, 0, 1920, 1080 }))                // the whole map
//        .to(90, cam::Shot{ { 3200, 1400 }, 2.5f })                  // glide in on a detail
//        .fly(200, cam::Shot::fit({ 5200, 300, 900, 500 }))           // zoom out, across and in
//        .to(300, cam::Shot::fit({ 0, 0, 7000, 2600 }), ease::inOut(ease::expo));
//   TNode* world = cam::world(c.roots, 0, 300, track);                  // add the canvas under `world`
//   world->add(0, 300)->fn = [](const Ctx& cx, int t) { ... canvas coordinates ... };
//
// A Shot names the canvas point at the middle of the frame, the zoom (frame px per canvas
// px) and the turn. Moves are pure functions of the frame, like everything else.
#pragma once
#include <functional>
#include <vector>
#include "anim.hpp"
#include "timeline.hpp"

namespace rmtn {
namespace cam {

struct Shot {
    Vector2 center{ 960, 540 };       // canvas point shown at the middle of the frame
    float zoom = 1;                   // frame px per canvas px
    float rot = 0;                    // degrees; the view turns, so the canvas turns the other way
    // The zoom and centre that fit `r` in a frame of `frame` px with `margin` (a fraction of
    // the frame) round it.
    static Shot fit(Rectangle r, Vector2 frame = { 1920, 1080 }, float margin = 0.06f, float rot = 0);
    // Blends two shots: zoom geometrically and the centre so the motion on screen is steady.
    static Shot mix(const Shot& a, const Shot& b, float u);
    // The Van Wijk-Nuij smooth zoom: pulls back while it pans so a long move stays readable,
    // then closes in (d3.interpolateZoom). rho ~1.4 is the classic curve; larger pulls back more.
    static Shot fly(const Shot& a, const Shot& b, float u, Vector2 frame = { 1920, 1080 }, float rho = 1.414f);
};

// Where a canvas point lands in the frame, and back.
Vector2 toFrame(const Shot& s, Vector2 canvas, Vector2 frame = { 1920, 1080 });
Vector2 toCanvas(const Shot& s, Vector2 framePt, Vector2 frame = { 1920, 1080 });
// The canvas rectangle in view (its bounding box when turned).
Rectangle visible(const Shot& s, Vector2 frame = { 1920, 1080 });

// The node props that put a canvas under the shot. parallax 1: the canvas itself; 0: fixed
// to the frame; in between a layer that pans and zooms less (a distant background), above
// 1 one that moves more (a near foreground).
NodeProps props(const Shot& s, Vector2 frame = { 1920, 1080 }, float parallax = 1);

// A deterministic handheld wobble: up to `amount` frame px and amount/8 degrees at about
// `freq` cycles per second (30 fps), seeded.
Shot shake(Shot s, int t, float amount = 8, float freq = 1.2f, unsigned seed = 1);

// Keyframed camera moves.
class Track {
public:
    Track& start(const Shot& s);                     // the shot at frame 0
    // Arrive at `s` by frame `frame`, easing from wherever the previous key left off.
    Track& to(int frame, const Shot& s, Easing e = ease::inOut(ease::cubic));
    // As to(), along the smooth zoom-out-and-in path.
    Track& fly(int frame, const Shot& s, Easing e = ease::inOut(ease::cubic), float rho = 1.414f);
    Track& hold(int frame);                          // stay put until `frame`
    Shot at(double t) const;
    int end() const { return keys_.empty() ? 0 : keys_.back().frame; }
    Vector2 frame{ 1920, 1080 };
private:
    struct Key { int frame; Shot shot; Easing ease; bool fly; float rho; };
    std::vector<Key> keys_;
};

// A node whose props follow the camera: put the canvas under it. The function gets the
// node-local frame.
TNode* world(Nodes& list, int from, int duration, std::function<Shot(int t)> shot, Vector2 frame = { 1920, 1080 }, float parallax = 1);
TNode* world(Nodes& list, int from, int duration, const Track& track, float parallax = 1);
TNode* world(TNode* parent, int from, int duration, std::function<Shot(int t)> shot, Vector2 frame = { 1920, 1080 }, float parallax = 1);
TNode* world(TNode* parent, int from, int duration, const Track& track, float parallax = 1);

} } // rmtn::cam
