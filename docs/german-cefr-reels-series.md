# German CEFR reel series - voice-free template batch

Status: **template-grade batch**, not a release. This turns all 144 supplied A1-C2
concept briefs into 30-second, 9:16 caption-led reels with BGM and SFX only.

## Scope

- 144 reels: 6 levels (A1-C2) x 3 tracks x 8 briefs.
- Runtime: 30 s / 900 frames at 30 fps, 1080 x 1920.
- No voiceover. On-screen captions, German examples, practice prompt and recap.
- Shared BGM (`assets/sfx/bgm.wav`) and sparse SFX.
- Source data: `assets/data/german-cefr/reels.json`.
- Composition: `examples/showcase/a1_german_reels.cpp`.
- Deliverables: `output/german-cefr/*.mp4` and `manifest.json`.

## Three visual grammars

| Track | Proof object | Layout |
| --- | --- | --- |
| Grammar | example phrases | numbered phrase cards with a hook and practice prompt |
| Vocabulary | word/phrase cards | cream word cards with a track-coloured top rule |
| Situations | a short exchange or sign set | alternating speech bubbles and a practice prompt |

## Explicit limitations

- This is a **template-grade series shell**, not 144 individually authored films.
- The source briefs are concept-level; examples are not independently source-checked.
- The recap caption is generic: “Practice this with a new example.”
- No language-editor sign-off, SRT sidecars, thumbnails or per-reel QC.
- One shared BGM bed; no bespoke score, ducking or voice.

## Release gate

Do not call this a finished series. Before release, each reel needs its own proof object,
language review, caption timing, still review and a final QC pass. This batch is a
consistent silent baseline for reviewing the 144-title curriculum.
