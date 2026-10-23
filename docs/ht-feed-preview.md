# Animated Here-and-There feed preview

## Live display only

    build/raymotion.exe preview here-there-feed --draft

The native raylib window starts at half of the 1080x1350 canvas dimensions and can be resized. The canvas is fitted without stretching or cropping. The animation lasts 88 seconds and loops. This composition has no audio.

This preview command does not export PNGs, video or audio. The earlier rendered explanations are separate files and are not updated by previewing this composition.

Controls:

- Space: pause/resume.
- Hold Left: slow playback.
- Hold Right: fast playback.
- Escape: close.

## Story and illustrations

- The exact opening question is: “If nothing contradicts a claim, should you believe it?”
- A proposed smoke claim appears inside a GUESS bubble, with an empty given-facts area.
- The stage unfolds into amber THERE (our guess) and turquoise HERE (what we keep), both at the same moment.
- A claim can leave HERE; a claim absent from THERE cannot enter HERE.
- The fire-to-smoke rule is checked in both illustrated stages.
- The smoke-only THERE guess is pinned while smoke is removed from HERE. The unchanged rule still works, showing why this candidate is not backed up under the additional selection test.
- A GIVEN: FIRE card changes the theory. Removing fire breaks the fact; removing smoke breaks the rule. Both must stay.
- The closing question returns: “Nothing says it's wrong. But what says it's right?”

These are code-drawn paper-cut islands, flames, curling smoke and rule ribbons. Segoe UI Regular/Bold is loaded from Windows. There are no stock images or browser dependencies.

## Inspect the illustrations without the long captions

From the project's bash terminal:

    RAYMOTION_FEED_LABELS_ONLY=1 ./build/raymotion.exe preview here-there-feed --draft

This keeps short object/rule labels and hides the long lesson copy. It is intended for checking whether movement and staging carry the explanation. Do not run this at the same time as the normal preview unless two windows are desired.

## No-export tests

    cmake --build build
    ctest --test-dir build -R 'renderer_regressions|ht_semantics|ht_plain_layout|ht_feed' --output-on-failure

The feed tests use real GPU rendering and read image data in memory; they do not save screenshots. They check scene registration, semantic states, subset constraints, fixed THERE during removal, text bounds and frame-order determinism. Human review of clarity and engagement is still required.

Do not run `cli_regressions`, `still`, or `render` when the session is restricted to display only: those paths write media.

## Source and scope

This is an independent, informal introduction based on Pedro Cabalar's *Temporal ASP: from logical foundations to practical use with telingo*, section 2:
https://www.dc.fi.udc.es/~cabalar/RW2021.pdf

HT supplies the two-component semantics. The further no-smaller-H selection test belongs to equilibrium logic. The visual metaphor is not a proof algorithm or a method for proving physical smoke impossible. The temporal extension, negation and full formal definitions are outside this short lesson.

Approved editorial and animation plan:
`.hermes/plans/2026-10-23_205529-here-there-hook-to-tool.md`.
