# bundaran-hi-banner — authoring brief

> A short illustrated film (not a documentary) set at the Selamat Datang Monument, Bundaran HI.
> Status: `approved` by the owner (answers below). Composition code may start (AGENTS.md).

| | |
| --- | --- |
| Composition name | `bundaran-hi-banner` |
| Story mode | `narrative` (one scene, one act) with promo packaging (hashtag end card) |
| Owner / agent | pebaryan / Claude |
| Status | `approved` |
| Runtime target | `24 s` |
| Platform / aspect | `1920x1080, 30 fps` (a vertical `1080x1920` cut is a re-frame, see 0.1) |
| Voice | none; crowd bed and a procedural score. Must work muted: the hashtag is on screen. |
| Audience | the viewers of the `#IndonesiaSadar` campaign. After watching they can say: "Two people hung the word where everyone could read it." |
| Engine features it proves | `crowd::` (students, police), `physics::Cloth` (the banner), `rig::` characters, water and fountains at the roundabout |

## 0. Open decisions for the owner

1. **Aspect.** The sister reel `indonesia-sadar` is 9:16. A monument, a crowd and a banner
   that falls the full height of the plinth are all vertical-friendly, but the crowd-versus-line
   standoff is wide. Default here: **16:9 master, 9:16 re-frame as a second render**. Say if
   you want 9:16 only.
2. **Scale and license of the monument.** Published heights disagree (section 5). The film
   labels itself *ilustrasi* and uses one fixed scale (figures about 5 m head to foot, plinth
   about 10 m). Fine?
3. **Depiction, not documentary.** The scene is staged: no real unit insignia, no named
   university logo, no real person, no violence (a tense standoff, nobody is hurt), and the
   climb is shown as an act of the story, not as advice. The on-screen label `ILUSTRASI` stays
   for the whole film. Anything you want stronger or softer?
4. **Who the film is from.** The end card carries `#INDONESIASADAR` and the repo line only,
   like `zombie-horde` and `voxel-splats`. No organisation's logo.
5. **Relation to the existing reel.** `docs/indonesia-sadar-brief.md` is a data-evidence
   reel on the same hashtag, made in the other session. This is a separate piece. Do they
   share an end card, or stay apart?

### Owner's answers (recorded)

1. Aspect: owner said "choose any for now": **16:9 master**, 9:16 re-frame later.
2. Scale: illustrative scale accepted.
3. Staged, labelled `ILUSTRASI`, no logos or named campus or unit insignia, no violence, no
   real people: accepted.
4. End card: stays separate from the `indonesia-sadar` reel for now.

## 1b. Mode contract

| | |
| --- | --- |
| Mode | narrative |
| Goal | show one gesture: the word unrolls down the monument above a standoff |
| Proof object | the **banner**: a physics cloth hung from the plinth's top ledge, unrolling downward row by row until the text reads |
| Required evidence | none beyond the source ledger (section 5); the scene is labelled illustrative |
| Text | `ILUSTRASI` corner tag; the banner text `#INDONESIASADAR`; end-card line |
| House grammar | narrative: place first, tension second, the one gesture third |
| Packaging | thumbnail candidate = the banner half unrolled over the two climbers; end card with hashtag and repo line |

## 1c. Reel / feed cut

`n/a` for the master. For the vertical re-frame: hook = frame 0 already shows the monument
with the two small figures at its foot; the banner unrolls in the last third; safe zones as
in `skills/raymotion/references/reels.md`.

## 1. One sentence

> Two students climb the Selamat Datang Monument above a crowd and a police shield line, and
> unroll a vertical banner that spells the hashtag the crowd below is chanting.

## 2. One-line argument

A banner is a statement everyone can read at once; the film is the time it takes to hang one.

## 3. Chapters

| # | Time | Chapter | Proof object | What the viewer learns |
| --- | --- | --- | --- | --- |
| 1 | 0.0-4.5 s | **The place** | the monument and its pool at dusk-gold light, the roundabout around it | where we are; the two bronze figures wave from the top |
| 2 | 4.5-10.0 s | **The standoff** | the **gap**: students in yellow and blue jackets on one side, a barricade and a shield line on the other, a clear strip of road between | who is facing whom, and that it is calm but tight |
| 3 | 10.0-16.5 s | **The climb** | the two climbers on the plinth, their rope paying out from the ledge (physics rope) | someone is going up while everyone watches |
| 4 | 16.5-21.5 s | **The unroll** | the **banner** unrolling down the plinth, row by row, wind in it | the word appears; the crowd raises its arms |
| 5 | 21.5-24.0 s | **The card** | hashtag and repo line | what to remember |

## 3b. Reveal map (text-only cue map)

| # | Scene | Anchor (s) | Visual lead | One entrance | Hold |
| --- | --- | --- | --- | --- | --- |
| 1 | place | 0.0 | on frame | monument in warm light, water moving | 4.5 s; tag `ILUSTRASI` fades in at 0.4 s and stays |
| 2 | standoff | 4.5 | camera push 0.3 s before | the crowd side, then (6.5 s) the shield line | 5.5 s |
| 3 | climb | 10.0 | 0.3 s before: camera tilts up | the two climbers appear at the plinth foot, then climb | 6.5 s |
| 4 | unroll | 16.5 | 0.3 s before: wind picks up, the crowd quietens | roll released from the top; row release 8 rows/s | 5.0 s |
| 5 | card | 21.5 | 0.2 s before: banner holds, wind drops | hashtag, then repo line | 2.5 s |

## 4. Script

Text-only cue map (no voice): the only words are `ILUSTRASI`, `#INDONESIASADAR` on the
banner and the card, and `github.com/pebaryan/raymotion`.

## 5. Sources and claims

| Claim | Source | Status |
| --- | --- | --- |
| The Selamat Datang Monument stands at the centre of the Bundaran Hotel Indonesia traffic circle, Jl. M.H. Thamrin, Central Jakarta, in a round pond with fountains | [Wikipedia: Selamat Datang Monument](https://en.wikipedia.org/wiki/Selamat_Datang_Monument) | used |
| Built for the 1962 Asian Games IV; sculptor Edhi Sunarso, sketch by Henk Ngantung (then Jakarta's vice governor); two bronze figures raising their arms, one holding a bouquet | same page; [RCTI+ "Asal-usul Patung Selamat Datang di Bundaran HI"](https://www.rctiplus.com/news/detail/okezone/5500412/asal-usul-patung-selamat-datang-di-bundaran-hi-jakarta) | used |
| Size: figures about 5 m head to foot (over 7 m to the raised hand), plinth about 10 m. Sources disagree (one Wikipedia snapshot says about 30 m in total, one outlet says 20 m); the 27 m figure belongs to the Pancoran statue, not this one | RCTI+, [GoodNews from Indonesia](https://www.goodnewsfromindonesia.id/2021/10/08/makna-dan-kisah-di-balik-5-patung-ikonik-indonesia), Wikipedia | **illustrative scale**, labelled |
| Pond diameter of "no less than 100 m" | one blog only | **not used**; pond drawn illustrative |
| Students wear almamater jackets; yellow is identified with Universitas Indonesia and several other campuses, dark blue is the most common colour | [GoodNews from Indonesia, almamater colours](https://www.goodnewsfromindonesia.id/2021/12/31/melihat-warna-warni-jas-almamater-kampus-di-indonesia), [Mojok](https://mojok.co/terminal/menebak-karakter-mahasiswa-dari-warna-jas-almamater/) | used; **no campus logo or name** on screen |
| Crowd control (Dalmas) is governed by Perkap 16/2006; the heavier tier is equipped with shields, helmets and batons | [Perkap 16/2006](https://kalsel.polri.go.id/perkap/2006/PERKAP%20NO%2016%20TH%202006%20TTG%20PENGENDALIAN%20MASSA.pdf), Polri training reports | used for the gear only; colours and barricade type are **illustrative** (the search found no source for uniform colour or barricade type) |
| Fountains were redone after the 2002 restoration, and a 2016 re-arrangement was announced | Wikipedia; [Jakarta Post 2016](https://www.thejakartapost.com/travel/2016/10/18/central-jakarta-to-have-dancing-fountain-wider-sidewalks) | context only; the film's fountains are generic |

Everything on screen that is not in this table is invented and is covered by the
`ILUSTRASI` tag.

## 6. Visual system

- **Light:** low golden-hour sun from the left, long shadows across the roundabout; sky from
  `Sky::at(~17 h)`; warm key, cool fill. Late-afternoon colour keeps the yellow and blue
  jackets readable.
- **Palette:** monument bronze and stone-grey; students in saturated **yellow** and
  **navy/mid blue** jackets (about 55/45); police in a dark neutral with clear shields;
  barricade in grey concrete and white-and-red bars; banner in off-white cloth with
  near-black text. Teal and amber overlays as in the sister films.
- **Type:** `ILUSTRASI` 22 px tracked caps, top right. The banner letters are cloth texture
  (a `DynamicTexture`), upright, heavy slab letters, read top to bottom.
- **Camera:** one orbit-and-push: high wide establishing, low push into the standoff, a tilt
  up the plinth, a hold on the banner.
- **Characters:** crowd and police are baked instanced bodies (`crowd::`), jackets set per
  instance colour; the two climbers are `rig::` characters (hero tier). Shields are
  transparent boxes on each police arm.
- **Water:** the pond uses `g3d::Water` (calm, with caustics) so the base reads as a
  fountain basin; the fountains are particle jets.

### 6b. Metaphor map

| Claim / relation | Mapping | Animated change |
| --- | --- | --- |
| "Two sides, calm but tight" | the clear strip of road between crowd and line | the strip stays open; the crowd's front row stops one step short of it |
| "The word goes up where everyone can read it" | banner row release | rows released from the roll at 8 rows/s from 16.5 s; the text becomes legible as rows fall below the ledge |
| "They hear it" | crowd heads and arms | crowd walkers switch from idle to cheer (raised arms) as the banner passes the pedestal's mid-height |

No duplicated labels: the only text in the frame is the banner, the tag and the card.

## 7. Audio plan

- Procedural score from `procgen::Score` (as in `zombie-horde`): a low drone under the
  standoff, a rising pulse on the climb, a wind bed that thickens with the unroll, a
  release on the final hold.
- Crowd murmur and a distant chant as filtered noise bursts, placed from the build; wind
  from the same gust function the cloth uses (`physics::Wind`), so sound and cloth agree.
- Loudness about -16 LUFS, true peak under -1 dBFS.

## 7b. Scenario coherence

- The crowd and the shield line stand on the roundabout road ring between the monument and
  the barricade; the two climbers start at the plinth's seaward side, away from the police,
  and the banner hangs on the side that faces the crowd.
- The sun, shadows and wind direction are constant through the film; the wind rises only at
  the unroll (0.3 s before) and drops before the card.
- Only the climbers move up; nobody crosses the strip.

## 8. Asset plan

| Need | Source | Note |
| --- | --- | --- |
| Monument | procedural: stepped plinth (`Geometry::box`), two bronze figures from `rig::` body presets or lathe shapes in bronze | stylised, not a scan |
| Pond | `g3d::Water` plus a ring wall | generic fountain jets |
| Crowd (students, police) | `crowd::addInstanced` with a baked walk/idle/cheer set | colour per instance; shields as child boxes |
| Climbers | `rig::` hero characters; the climb is a keyframed pose set (no climb clip exists yet, see 12) | |
| Rope | `physics::Rope` from the ledge to the climbers | |
| Banner | `physics::Cloth` 8 x 40, texture from a `DynamicTexture` | needs a pin release time (section 12) |
| Score | `procgen::Score` | generated into `out/bundaran-hi-banner-assets/` |

## 9. Cue map

0.0 monument, tag · 4.5 crowd then line · 10.0 climbers appear · 12.5 first climber at the
ledge · 16.5 unroll begins · 19.0 crowd cheers (mid-height) · 21.0 banner settles · 21.5 card.

## 10. Still frames to review

Chapter starts, middles and ends: 0, 60, 130 (place); 140, 200, 290 (standoff); 300, 400,
490 (climb); 500, 560, 640 (unroll); 645, 690, 719 (card). Plus the thumbnail candidate at
about frame 570, and each transition (4.5, 10.0, 16.5, 21.5 s).

### 10b. Scene review ledger

Filled in the QC doc, one scene at a time.

## 11. Acceptance checks

- [ ] Brief approved (section 13).
- [ ] The banner unrolls downward from the ledge, the text reads upright and in the right
      order at the hold, no cloth tunnelling through the plinth.
- [ ] `ILUSTRASI` visible in every frame; no logo, no named campus, no unit insignia.
- [ ] Sources in section 5 match what the film claims; scale labelled illustrative.
- [ ] A draft watched muted and with sound (owner), scene by scene.
- [ ] `ffprobe`, full decode, loudness, true peak, frames from the MP4, `list` from another
      directory.
- [ ] Authoredness >= 22 and Craft >= 13 (QC).

## 12. Risks / non-goals

- **Pin release.** `physics::Pin` holds a particle for all time. The unroll needs a release
  time per pin (a roll of rows held on a spiral at the ledge, freed one after another). Small
  engine change plus a test, done before the film.
- **Climb animation.** There is no climb clip in `rig::`. The climb is a short keyframed
  pose set on the plinth's steps and ledge; if it looks stiff, the fallback is a wider shot
  where the climbers read as silhouettes.
- **Crowd size and cost.** A few hundred baked bodies in the frame, plus the pond and the
  cloth; the cloth is cached, so the cost is paid once per frame.
- **Sensitivity.** A protest scene on a real landmark. Mitigation: staged, labelled, no real
  people or institutions, no violence.
- **Non-goals:** a documentary account of any event; a likeness of any real person or unit;
  teaching anyone to climb the monument.

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | owner | 2026-10-31 | approved (section 0 answers) |
| Script / cue map | | | |
| Stills | | | |
| Staged reveal / sync | | | |
| Draft | | | |
| Final QC | | | |
