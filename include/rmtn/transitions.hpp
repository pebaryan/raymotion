// rmtn/transitions.hpp -- Remotion-style <TransitionSeries>: scenes back to back, with
// transitions that overlap neighbouring scenes.
//
//     TransitionSeries ts(c.roots, c.w, c.h);
//     ts.add(90)->fn = sceneA;
//     ts.transition(Transition::fade(20));
//     ts.add(90)->fn = sceneB;                // starts 20 frames before A ends
//
// Transitions drive the layer of the scene nodes they join (enabled only during the
// overlap); put any layer effects of your own on the scenes' children.
#pragma once
#include <algorithm>
#include <memory>
#include "anim.hpp"
#include "timeline.hpp"

namespace rmtn {

enum class Direction { FromLeft, FromRight, FromTop, FromBottom };

struct Transition {
    enum class Kind { Fade, Slide, Wipe };
    Kind kind = Kind::Fade;
    int frames = 15;
    Direction dir = Direction::FromRight;
    Easing easing = ease::inOut(ease::cubic);

    // Entering scene fades in over the exiting one.
    static Transition fade(int frames, Easing e = ease::inOut(ease::cubic)) {
        return { Kind::Fade, frames, Direction::FromRight, std::move(e) };
    }
    // Entering scene pushes the exiting one out, arriving from `dir`.
    static Transition slide(int frames, Direction dir = Direction::FromRight, Easing e = ease::inOut(ease::cubic)) {
        return { Kind::Slide, frames, dir, std::move(e) };
    }
    // Entering scene is revealed by an edge sweeping in from `dir`.
    static Transition wipe(int frames, Direction dir = Direction::FromLeft, Easing e = ease::inOut(ease::cubic)) {
        return { Kind::Wipe, frames, dir, std::move(e) };
    }
};

class TransitionSeries {
public:
    TransitionSeries(Nodes& list, float width, float height, int from = 0)
        : list_(list), w_(width), h_(height), end_(from) {}

    // The transition between the scene added last and the next add(). It overlaps both.
    TransitionSeries& transition(const Transition& t) { pending_ = std::make_unique<Transition>(t); return *this; }

    TNode* add(int duration) {
        int overlap = 0;
        if (pending_ && prev_) overlap = std::min({ pending_->frames, prev_->dur, duration });
        TNode* n = addRoot(list_, end_ - overlap, duration);
        if (overlap > 0) link(prev_, n, *pending_, overlap);
        pending_.reset();
        prev_ = n;
        end_ = n->start + duration;
        return n;
    }
    int end() const { return end_; }       // frame after the last scene

private:
    // Offset (one full frame) from the scene's rest position to where `d` enters from.
    Vector2 away(Direction d) const {
        switch (d) {
            case Direction::FromLeft:  return { -w_, 0 };
            case Direction::FromRight: return { w_, 0 };
            case Direction::FromTop:   return { 0, -h_ };
            default:                   return { 0, h_ };
        }
    }
    // Chains `extra` after any layer animation the node already has.
    static void chain(TNode* n, std::function<void(int, NodeProps&, Layer&)> extra) {
        n->isolate().enabled = false;
        auto before = n->layerUp;
        n->layerUp = [before, extra](int t, NodeProps& p, Layer& l) {
            l.enabled = false;
            if (before) before(t, p, l);
            extra(t, p, l);
        };
    }
    void link(TNode* exiting, TNode* entering, const Transition& tr, int overlap) {
        const Easing ease = tr.easing;
        auto progress = [ease, overlap](int t) { return ease(std::min(1.0, std::max(0.0, (double)t / overlap))); };
        const Vector2 from = away(tr.dir);
        const int exitStart = exiting->dur - overlap;
        switch (tr.kind) {
        case Transition::Kind::Fade:
            chain(entering, [=](int t, NodeProps&, Layer& l) {
                if (t >= overlap) return;
                l.enabled = true;
                l.opacity = (float)progress(t);
            });
            break;
        case Transition::Kind::Slide:
            chain(entering, [=](int t, NodeProps&, Layer& l) {
                if (t >= overlap) return;
                float k = 1.f - (float)progress(t);
                l.enabled = true;
                l.offset = { from.x * k, from.y * k };
            });
            chain(exiting, [=](int t, NodeProps&, Layer& l) {
                if (t < exitStart) return;
                float k = (float)progress(t - exitStart);
                l.enabled = true;
                l.offset = { -from.x * k, -from.y * k };
            });
            break;
        case Transition::Kind::Wipe: {
            auto shown = std::make_shared<float>(0.f);         // revealed fraction, set per frame
            const Direction dir = tr.dir;
            const float W = w_, H = h_;
            entering->isolate().mask = [shown, dir, W, H](const Ctx&, int) {
                const float k = *shown;                         // composition space, ignores the node ctx
                Rectangle r{ 0, 0, W, H };
                if (dir == Direction::FromLeft) r.width = W * k;
                else if (dir == Direction::FromRight) { r.x = W * (1 - k); r.width = W * k; }
                else if (dir == Direction::FromTop) r.height = H * k;
                else { r.y = H * (1 - k); r.height = H * k; }
                DrawRectangleRec(r, WHITE);
            };
            chain(entering, [=](int t, NodeProps&, Layer& l) {
                if (t >= overlap) return;
                l.enabled = true;
                *shown = (float)progress(t);
            });
            break;
        }
        }
    }

    Nodes& list_;
    float w_, h_;
    int end_;
    TNode* prev_ = nullptr;
    std::unique_ptr<Transition> pending_;
};

} // namespace rmtn
