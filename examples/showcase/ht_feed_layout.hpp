#pragma once
// Layout landmarks of the here-there-feed composition, shared with tests/ht_feed_tests.cpp
// so the test identifies shapes by name rather than by copied pixel values.
namespace rmtn { namespace feedLayout {
// Rejected entrance (34s-37s): the HERE boundary line and the flame bouncing outside it.
constexpr float REJECT_TOP=644, REJECT_BOTTOM=861;
constexpr float REJECT_FLAME_Y=797, REJECT_FLAME_SIZE=56;
// "GIVEN: FIRE" badge (70s onward).
constexpr float BADGE_X=254, BADGE_W=572, BADGE_H=54;
}}
