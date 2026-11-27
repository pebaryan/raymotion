# weather-day — authoring brief

> Status: `draft`. Written before any composition code. The film is the engine demo for
> raymotion 0.14 (the weather state, volumetric clouds, rain, wet ground, lightning, the rainbow,
> water that reflects, wakes, light in the air, camera weather). It is an engine showcase like
> `far-and-near` (0.13), not one of the owner's films, and it is not released or published.

| | |
| --- | --- |
| Composition | `weather-day` (`examples/showcase/weather_day.cpp`, `registerWeatherDay`) |
| Story mode | `utility` |
| Owner / agent | the plan (docs/weather-water-plan.md, W12) set the film; Claude (agent) wrote and built it; owner review pending |
| Status | `draft` |
| Runtime target | 54 s (1,620 frames): 48 s of weather and a 6 s end card laid over the last shot |
| Platform / aspect | 1920x1080, 30 fps, engine demo |
| Voice | none: a utility clip; the labels are the script |
| Audience | someone who makes films with raymotion and wants to know what 0.14 gives a landscape: one weather state that drives sky, clouds, wind, rain, wet ground, lightning, a rainbow, the lake and the light |
| Source material | none outside the engine. The land, the lake, the jetty and the boat are built in code. Every number on screen is the engine's own state, read at draw time. |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | `utility` |
| Goal | be legible in context: show that one `g3d::Weather` (plus the pieces it feeds) can carry a landscape through a whole day, and which call does each part |
| Proof object | per chapter, a gauge or tag read from the engine at draw time: the wind, the wave height, the visibility, the rain rate, the wetness, the strike distance and thunder delay, the bow's angle, the wake's angle |
| Required evidence | spec: no number is typed in. Wind (m/s), significant wave height Hs (cm), visibility (km), rain rate (mm/h), wetness, strike distance (m) and delay (s), sun elevation and bow angle (degrees), boat speed and Froude number are all computed from the scene's own structs at draw time |
| Text | short labels and one caption card per chapter; the API lines on the end card |
| House grammar | brand/utility, as `far-and-near` and `flora`: caption card (title, note, call), API end card, logo, repo line |
| Packaging | end card with the API lines, the raymotion logo prefab and the repo line; no SRT or thumbnail required |

## 1c. Reel / feed cut

n/a: 16:9 only.

## 1. One sentence

> This is a film about **one lake seen from one jetty through one day** that makes the viewer
> **see that raymotion 0.14's weather is a single state that moves the sky, the clouds, the
> wind, the water, the rain, the ground and the light together** by **holding the camera in
> one place while the day changes, and reading the engine's own wind, wave height, rain,
> wetness and thunder delay off the picture as it does.**

## 2. One-line argument

> The camera never moves to a new place; only the weather does, and every part of the
> picture, and every number on screen, follows the one `Weather` state.

## 3. Chapters

Film time in seconds (frames at 30 fps in brackets). It is one continuous shot of one scene:
chapters are a time-lapse of the weather inside it, not cuts. The camera is on a jetty at
(0, 2.15, 0) and turns slowly (-8 to +8 degrees about the vertical, no change of place).
The end card is laid over the last 6 s of the same shot; nothing cuts to black.

| # | Time (s) | Chapter | On-screen text | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0.0-8.5 (0-255) | Dawn mist | `Dawn mist` card; `visibility N km`, `sun N deg` | the low sun ahead through the trees over a lake under mist: sun shafts in the haze; the far ridges pale and blue by distance | **ridge tags**: the first and the farthest ridge, each with its distance (from the scene's table) and how much of its own colour the haze has left (aerial perspective, read at draw time); the **visibility gauge** from `Weather::fogDensity()` | a still room tone; wind bed at the dawn breath |
| 2 | 8.5-17.5 (255-525) | Cumulus building | `Cumulus building` card; `wind N m/s`, `waves N cm` | the clouds grow from nothing to a field of cumulus; the wind picks up; the lake goes from a mirror to rippled and the reflection of the far shore and clouds breaks up | the **wind and Hs gauges** (`Weather::wind.speed`, `Weather::waveHeight(fetch)`) side by side, with the mirror-to-breezy state shown by the lake itself | the wind bed swells with the wind speed |
| 3 | 17.5-33.0 (525-990) | A squall crosses the lake | `A squall line` card; `rain N mm/h`, `wet N %`, `strike N m`, `thunder in N s` | a storm cell with an anvil comes in over the far shore; the sky darkens; rain moves in over the lake as a curtain, then falls on the jetty, which darkens wet, with rings in the puddles; lightning strikes at three distances; the lens (a weather-camera housing) beads with drops and a wiper clears them once | the **rain-rate** (from the engine's own drops), **wetness** (`Weather::ground`), and the **strike tag**: the distance from the camera to the bolt's foot and the delay `distance / 343` (`bolt::thunderDelay`), counting down to the arrival | rain bed; three thunder rumbles placed at `arrives` |
| 4 | 33.0-40.5 (990-1215) | Clearing, a rainbow | `Clearing` card; `bow N deg` | the rain moves off and stops; the cloud breaks; the sun, behind the camera, lights the last of the rain over the far shore: a bow, and a faint second | the **bow tag**: the primary at `rainbowAngle(550 nm, 1)` from the anti-solar point, with the sun's elevation (`rainbowOf`), drawn as a ring marker over the engine's own arc | rain thins out; birds none |
| 5 | 40.5-48.0 (1215-1440) | Calm evening | `Calm evening` card; `Hs N cm`, `wake N deg`, `Fr N` | golden light; the lake a mirror again; a boat crosses and its Kelvin wake spreads; the reflections ripple as the V passes | the **wake tag**: the arm's half-angle from the wake's own geometry (`asin(1/3)`), the boat's speed and Froude number (`speed / sqrt(g length)`); the **Hs** gauge back at the dawn value | the wind bed falls; a low pad |
| 6 | 48.0-54.0 (1440-1620) | End card | `ONE PLACE. ONE WEATHER.` and the API lines | the shot dims under a scrim; the lines come in one at a time; logo and repo line | none (packaging) | the pad resolves |

One idea per chapter: 1 the air has a distance, 2 one wind raises the water, 3 the storm
reaches the ground, 4 the sun behind the camera finds the rain, 5 the same lake calm again.
The proof column is never "caption".

### 3b. Reveal map (text-only cue map)

There is no voice. An anchor is an engine event (the rain starting, a strike, the boat
entering). A label that explains a state comes 0.20-0.40 s before the state has fully
arrived (the card leads the change it names); a read-out of a flash or a strike follows it
by 0.2-0.4 s, because a flash cannot be led. Times are film seconds. Every unit holds at least
0.8 s before the next unrelated entrance; one entrance at a time.

| Scene | Unit | Anchor (beat) | Visual start | Lands | Hold |
| --- | --- | --- | --- | --- | --- |
| 1 | the shot itself, no text | film start: the sun in the haze | 0.0 | 0.0 | 1.2 |
| 1 | card `Dawn mist` | the shafts have been seen | 1.2 | 1.6 | to 7.6 |
| 1 | visibility gauge | the card has held | 2.8 | 3.2 | to 7.6 |
| 1 | ridge tags (near ridge, then far ridge) | the gauge has held | 4.4 / 5.6 | 4.8 / 6.0 | to 7.6 |
| 2 | card `Cumulus building` | the first cumulus is up | 8.9 | 9.3 | to 16.7 |
| 2 | wind gauge | the card has held | 10.6 | 11.0 | to 16.7 |
| 2 | wave-height gauge | the wind gauge has held | 12.2 | 12.6 | to 16.7 |
| 3 | card `A squall line` | the anvil clears the far shore | 17.8 | 18.2 | to 32.4 |
| 3 | rain-rate gauge | the rain curtain arrives over the lake (rain 0.1) | 22.4 | 22.8 | to 32.4 |
| 3 | wetness gauge | the jetty has begun to darken | 25.0 | 25.4 | to 32.4 |
| 3 | strike tag 1, with its countdown | strike 1 at 26.9 s, +0.3 s | 27.2 | 27.5 | until its thunder arrives |
| 3 | lens drops and the wiper | the rain is at full | 28.0 | 29.0 | wiper at 31.0 |
| 3 | strike tags 2 and 3 | strikes at 29.4 s and 30.8 s, +0.3 s | 29.7 / 31.1 | 30.0 / 31.4 | their own arrival |
| 4 | card `Clearing` | the rain has gone off the near water | 33.2 | 33.6 | to 39.8 |
| 4 | bow tag | the bow has lit | 35.4 | 35.8 | to 39.8 |
| 5 | card `Calm evening` | the lake is a mirror | 40.7 | 41.1 | to 47.4 |
| 5 | Hs gauge | the card has held | 42.4 | 42.8 | to 47.4 |
| 5 | wake tag | the boat is 8 s into its run; the V is wide | 44.2 | 44.6 | to 47.4 |
| 6 | scrim, title | 48.0 | 47.7 | 48.3 | to the end |
| 6 | API lines, one at a time | after the title | 49.2 | 49.5 + 0.2 each | to the end |
| 6 | logo, repo line | after the last line | 52.0 | 52.4 | to the end |

No scene is at full density at its first frame: scene 1 is a bare picture for 1.2 s.

## 4. Script

Voice: none. A utility clip whose labels are its script.

| # | Spoken text | Caption text | Voice seconds | Notes |
| --- | --- | --- | --- | --- |
| 1 | none | `Dawn mist` / `the sun low behind the trees, light in the haze, and the ridges paling with distance` / `Scene::volumetric, Scene::aerial`; `visibility N km`, `sun N deg`; ridge tags `ridge  2.5 km  N% of its colour` | n/a | numbers read from the scene |
| 2 | none | `Cumulus building` / `one wind raises the lake, and the mirror breaks up` / `Weather::wind, Water::reflections`; `wind N m/s`, `waves N cm` | n/a | Hs from `Weather::waveHeight` |
| 3 | none | `A squall line` / `rain, wet ground, a strike and its thunder delay` / `Scene::precipitation, Scene::ground, Scene::lightning`; `rain N mm/h`, `wet N %`, `strike N m`, `thunder in N s` | n/a | delay = distance / 343 |
| 4 | none | `Clearing` / `the sun behind you and rain in front of you` / `Weather::rainbow`; `bow N deg` | n/a | |
| 5 | none | `Calm evening` / `the same lake, a mirror again, and a boat's wake` / `Scene::wakes`; `wake N deg`, `Fr N` | n/a | |
| 6 | none | `ONE PLACE. ONE WEATHER.` + seven API lines | n/a | |

> `Cue map` (seconds from the film start; frame = 30 x seconds): shot f0; card 1 f48; visibility
> f96; ridge tags f144 and f180; card 2 f267; wind gauge f318; Hs gauge f366; card 3 f534; rain
> gauge f672; wetness gauge f750; strike 1 f807 (tag f825), strike 2 f882 (tag f900), strike 3
> f924 (tag f942); lens drops f840, wiper f930; card 4 f996; bow tag f1074; card 5 f1233; Hs f1284;
> wake tag f1338; scrim f1431; API lines f1476 on; logo f1560. The scene's own table in code
> (`KEY`) holds each of these once; the picture, the lightning schedule and the sound read it.

## 5. Sources and claims

| Claim / number | Value used | Source | On-screen? | Exact or illustrative? |
| --- | --- | --- | --- | --- |
| wind speed | `Weather::wind.speed` at the frame | the engine's weather state, set by the film's keys | gauge | exact (the film's setting; the landscape reads the same number) |
| significant wave height | `Weather::waveHeight(fetch)`: min(0.0016 U sqrt(F/g), 0.21 U^2/g) | docs/weather.md (W1, JONSWAP, fetch-limited) | gauge | exact for the engine's wave model; lake size (fetch) is the film's |
| visibility | 3.912 / `Weather::fogDensity()` | exponential fog: density 0.065 haze^2 per metre (docs/weather.md); 3.912 is -ln(0.02), the 2% contrast distance | gauge | exact for this model; the haze value is art direction |
| ridge distance and "colour left" | the ridge table in code; the share is `exp(-strength d / 30 km ...)` (the engine's aerial formula, read from `AerialPerspective`) | docs/weather.md ("Aerial perspective") | tags | exact for the model; the ridges are **illustrative** (made up) |
| rain rate | summed from `Downfall::drop` over one tile: volume x fall speed per drop / tile volume | the engine's own drop sizes and speeds (docs/weather.md, W3) | gauge | exact for the engine's drops; the drop spectrum is the engine's, not a measured one |
| wetness | `Weather::ground(seconds).wetness` | docs/weather.md (W4) | gauge | exact; the soak and dry times are the engine's, run 60x faster (`accumulationScale`) |
| strike distance, thunder delay | the camera to the nearest of the bolt's ends and middle; delay = distance / 343 | `g3d::thunderCues`, `bolt::thunderDelay` (W5) | tag | exact; the strikes are the film's (explicit list, seeded) |
| bow angle | `rainbowAngle(550, 1)` = Descartes' primary for water | docs/weather.md (W5) | tag | exact for the model |
| sun elevation | from `Sky::sunDirection` of the sky the scene is drawn with | the engine's `Sky::at(hour)` | tag | exact; the hour is the film's |
| wake half-angle, Froude number | asin(1/3) = 19.47 deg; `speed / sqrt(g length)` | Kelvin (1887), docs/weather.md (W8) | tag | exact for deep water |
| the day (time-lapse) | hours 6.2 to 18.2 in 48 s (about 900x); the clouds' run is accelerated as well | the film | clock | **illustrative**: a time-lapse; grass and trees sway in real time |
| land, lake, ridges, boat | built in code | none | - | **illustrative**: a made-up place |

No historical or statistical claim. The weather's keys (when the squall comes) are art direction.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | cool dark UI ink (`#f2f4ff`, `#b4bfcc`) on a dark scrim, as `far-and-near`; teal (`#3ad6c5`) = a number read from the engine; amber (`#ffd27a`) = the call (API text) and the tag lines; scene colours are the weather's own |
| Type | `fonts::ui`: caption title 60, note 28, call 26 (as `flora`, `far-and-near`); gauge number 52, unit 26; tag number 38; end card 96 |
| CJK | n/a |
| Margins / safe area | 64 px; caption card at the lower left, gauges top right, clock top left |
| Caption box | the `flora` card: three lines over a soft scrim, 186 px high |
| HUD / persistent frame | a small clock at the top left (`06:12`, from the hour that sets the sky) with `time-lapse`; the gauges at the top right; nothing else |
| Motion grammar | 0.4-0.6 s ease-out entrances; the camera turns slowly all film; no idle motion on text |
| Motif | the one fixed view (the jetty into the lake) and the gauge column |
| Charts / maps | none; gauges are a number, a unit and a short bar that fills in the engine's range |
| Text performance | short labels; one entrance at a time |
| Brand kit | raymotion logo prefab and repo line on the end card |

**Density rule:** one idea per screen: at most the card, the clock and two gauges, plus a tag
that is pinned to a thing in the picture.

### 6b. Metaphor map

| Claim | Visual metaphor | Mapping | Animated change |
| --- | --- | --- | --- |
| the air has a distance | pale ridges | each ridge has a tag with its distance; the tag's bar is the share of its own colour left, so the far ridge's is short | the bars fill as the tags enter, one ridge at a time; the haze thins through the chapter and the numbers move |
| one wind raises the water | the lake itself, and a gauge | wind m/s drives the bar on the left, Hs cm the bar on the right (both fill against their own range); the reflection is the third read | both bars rise together as the wind picks up, the reflection breaks |
| the rain reaches the ground | the jetty deck | wetness is the share of the deck that is dark; rain rate is the bar; the rings in the puddles are the rate made visible | the deck darkens through the squall; the bar rises, then falls |
| thunder arrives later than the flash | a tag pinned to the strike's foot with a countdown | distance m, delay s; the countdown runs from `delay` to 0 at `arrives` | the tag enters after the flash, counts down, ends with `now` and goes |
| the bow is where the sun and the rain make it | a ring marker | the marker's radius is the bow's angular radius, drawn from the same anti-solar point the engine uses | the marker draws in after the bow has lit and fades; it never hides the bow |
| a boat's wake has one angle | a wedge marker | the two lines at +-19.5 deg behind the boat | drawn after the V has spread, then fades |

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | none |
| Score | none as music. A generated soundscape (`procgen::Score` into `out/weather-day-assets/day.wav`): a wind bed whose level and brightness follow the same wind-speed function as the picture; a rain bed that follows the rain intensity; a quiet pad on the evening; and the thunder |
| Thunder | three synthetic rumbles (low-passed noise with a crack and a tail), placed at each strike's `arrives` (strike time + distance / 343) from the same strike table as the lightning: no recorded clip is used. The volume falls with distance |
| Ducking | none: no voice |
| SFX | none beyond the above |
| Silence | the first second is room tone only (the wind bed at its dawn level) |
| Loudness target | about -18 LUFS integrated (a quiet soundscape), true peak under -1 dBTP |

The sound reads the same key table and the same wind and rain functions as the picture, so
retiming the squall moves both.

## 7b. Scenario coherence

| Quantity | Value | Fits / depends on | Source or assumption |
| --- | --- | --- | --- |
| Place | a lake about 370 m x 250 m (ellipse axes 185 x 125 m), a jetty 24 m long, a 7.3 m cabin cruiser (4.6 m hull scaled 1.6), ridges to 25 km | fetch and Hs; the sun's azimuth; aerial perspective | illustrative |
| Camera | fixed at (6, 2.82, 0) on the jetty, looking east, turning -8 to +8 degrees | the dawn sun is ahead, the rainbow's centre is ahead, the sun behind the camera in the evening | `Sky::at` puts the sun at azimuth about -26 degrees at 6:00 and about 150 degrees at 17:00 |
| Wind | 0.6 m/s at dawn to 9 m/s in the cumulus, 14 m/s in the squall, 2 m/s in the evening, blowing towards +z | Hs, the lake's break-up, the cloud run | the film's keys |
| Day | 6.0 h to 18.3 h in 54 s | the sky colours, the sun's height, the rainbow only when the sun is low | illustrative time-lapse |
| Rain | 23-35 s, intensity up to 0.9 | `accumulationScale` 60: wetness reaches 0.9 and dries to about 0.55 by the evening | the engine's soak and dry times |
| Strikes | 2.3 km, 1.05 km, 0.54 km from the camera (engine thunderCues: 2316, 1072, 533 m) | delay 6.7, 3.1, 1.6 s | the film's list |
| Boat | 5 m/s, 7.3 m, Froude 0.59 (sqrt(g L) with L 7.3 m) | the V half-angle 19.5 deg, the wake's strength | the film's boat |

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Soundscape | `c.assets`, from the code | `out/weather-day-assets/day.wav` | regenerated on demand |
| Land, lake, jetty, boat, trees, grass | built in code, lazily, at the first draw | n/a | n/a |
| Noise atlases of the clouds | baked lazily by the engine at the first cloud frame | n/a | n/a |
| Logo | prefab `raymotion-logo` | engine | n/a |

No textures, no models, no recorded sounds.

**Clean-machine test:** with `out/` absent the film regenerates its soundscape; `raymotion list`
works from any directory and does not build the scene (the time of `list` is recorded in the QC).

## 9. Cue map

| Film seconds | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| 0.0 | the sun in the haze | wind bed at its dawn level | the key table |
| 26.9, 29.4, 30.8 | strike 1, 2, 3 | thunder at strike + distance / 343 | the strike table |
| 23.0-35.0 | the rain | the rain bed | the rain history |
| 48.0 | end card | pad resolves | the key table |

## 10. Still frames to review

| Frame | Chapter | What must be true |
| --- | --- | --- |
| f0, f128, f250 | 1 | the sun low and ahead, shafts visible in the haze among the trees, the lake a mirror, the far ridges paler by distance; no label at f0; tags inside the safe area and not over the card |
| f260, f390, f520 | 2 | cumulus appear and grow; the reflection of the far shore breaks as the wind rises; the gauges' numbers rise with it |
| f530, f720, f800 | 3 start, middle, strike | the anvil on the far shore; a rain curtain over the lake; the dark base; the jetty wet with rings |
| f830, f900, f980 | 3 lightning, lens, end | a bolt with branches and the flash on the lake; drops on the lens and the wiper; the squall on its way out |
| f1000, f1100, f1210 | 4 | rain moving off over the far shore, a bow against it with the sun behind, the second bow faint; the marker never over the bow's colours |
| f1220, f1330, f1435 | 5 | golden light, the boat and its V, the reflections rippling, the lake a mirror away from the V |
| f1440, f1530, f1619 | 6 | scrim, title, API lines, logo and repo line inside the safe area |
| each chapter boundary (f255, f525, f990, f1215) | all | one continuous shot, not a cut |

Thumbnail candidate: the chapter-4 bow over the lake (f1100), or the strike over the lake (f830);
chosen after the stills.

### 10b. Scene review ledger

| Scene | Review asset | Reveal / sync notes | Approved? | Re-render |
| --- | --- | --- | --- | --- |
| 1 | stills | | pending | |
| 2 | stills | | pending | |
| 3 | stills | | pending | |
| 4 | stills | | pending | |
| 5 | stills | | pending | |
| 6 | stills | | pending | |

## 11. Acceptance checks

- [ ] Muted, the argument is legible: one place, five weathers, the numbers moving.
- [ ] Each chapter's proof object is visible at its stated frame.
- [ ] Every scene starts sparse and reveals one unit at a time with a lead and a hold.
- [ ] Every scene approved individually (by the author; the owner has not seen it).
- [ ] The executable is newer than the last source edit; the final render was rebuilt after the last approved change.
- [ ] Every number on screen is read from the engine at draw time.
- [ ] Labels inside the safe area; the gauges never over the caption card.
- [ ] Sound and picture share the key table and the strike table.
- [ ] `list` works from another directory, with `out/` absent, and its time is unchanged.
- [ ] The camera's clearance over the ground and the jetty is logged for every frame.
- [ ] Duration, frame count, loudness and peak recorded.
- [ ] Packaging placed (end card, logo, repo line).
- [ ] Authoredness >= 22 and Craft >= 13 (author-only review: Verification, Spec-to-screen and Revision at most 2).

## 12. Risks / non-goals

- **Risks:** the cost of volumetric clouds, reflections, shafts and rain together at 1080p;
  the Cb cell's look from a ground camera (open in W2); the rain is the same all round the
  camera (it cannot be a curtain with a place), so the curtain over the lake is the far layer
  seen through a growing near radius; a squall line that races across the sky in a time-lapse;
  the first frame of the clouds bakes noise; the rainbow only shows with the sun behind the
  camera and low.
- **Non-goals:** snow; a sea (this is a lake); speaking characters; recorded thunder; a human
  voice; release.
- **Out of scope:** the weather's tree flutter amplitude and gust phase under a changing
  wind (documented limits in docs/weather.md).

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | owner | | pending |
| Script / cue map | | | |
| Stills | | | |
| Staged reveal / sync | | | |
| Draft | | | |
| Final QC | | | |

## 14. Changes made during the build

Recorded after the build; each one is also in the QC.

As-built changes from the brief (all also in docs/one-weather-day-qc.md):

- Camera moved to (6, 2.82, 0) on the jetty; the deck is at y 1.12; lawn grass (`flora::grass("lawn")`) so the jetty is visible.
- The day runs 6.0 h to 18.3 h in 54 s (the brief said 6.2 to 18.2 in 48 s). The start hour is 6.0 so the sun sits just behind the tree line and the shafts show.
- The cloud layer is Cumulonimbus from the first frame; its base lowers 1300 to 700 m and its top climbs 3000 to 11000 m over 16-26 s. A Cumulus-to-Cumulonimbus switch at 26 s (round 1) gave a visible pop at f806 and was removed: one type for the whole film, so there is no switch to hide.
- The two storm cells are authored in the pattern frame and come in from 15 km beyond the far ridge (x + 15000 m, eased to 0 over 12-28 s). At the start they are 35 percent size and grow to full by 26 s. After 33 s they move off to the far side (a move, not a shrink; shrinking left narrow columns).
- Cloud speed is a time-lapse curve (60 to 900 to 100 m/s), integrated numerically as windRun.
- Rainbow: the distantRain keys end at 40.5 s so the bow fades before chapter 5 (round 1 left it up).
- Lens: the rain-on-lens filter runs only in 27.8-33.8 s; blur-behind and the wiper were dropped (the wiper blade read as a heavy black diagonal).
- Boat: a 7.3 m cabin cruiser (hull, cabin, windows, stripes) at 5 m/s on a path at x = 34 m from z = -40 to 60, starting at 37 s, so it crosses the middle of the frame at about 45 s. The brief had 3.5 m/s, 4.6 m and Froude 0.52; as built Froude is 0.59.
- The far strike's tag is lifted 120 px to clear the later tags. The bow-ring marker was removed (it drew a wrong dashed line); the bow tag stays.
- The end card is an overlay scrim over the same shot, not a separate scene.
- RMTN_WD_THUNDER is implemented: it writes the engine's thunderCues for every strike (2316 / 1072 / 533 m against the readout table's 2314 / 1054 / 535 m). RMTN_WD_NOBOW, RMTN_WD_NOVOL and RMTN_WD_NOCLOUD are debug switches used for the off-versus-on checks.

Round 2 as-built changes (QC, 2026-11-27):

- The storm cells come in from 60 km beyond the far ridges for the dawn (an extra x offset on `come`), so no cell block shows at f0.
- Overlay tags are clamped to 64..(1856 - width) px; the wake tag is lifted 150 px; the far ridge's tag sits left and 70 px up so the two ridge tags no longer overlap.
- Seven strikes (24.6, 25.6, 26.9, 27.8, 29.0, 31.0, 32.4 s) instead of three; three of them carry the distance/delay tags (2.3 km, 1.1 km, 535 m). Lightning width 4 m, intensity 1.6.
- The rainbow is gated by `rainbow.strength = sm(s, 33.0, 34.6)`: the engine's Auto mode would draw it during the squall; now only chapter 4 shows it.
- Sound: wind and rain beds rebalanced; thunder is a band-passed crack (near) plus a rumble that lengthens with distance, placed at strike + distance / 343; a soft limiter `0.7 * tanh(1.3 v / 0.7)` replaces the fixed 0.88 gain. Final MP4: -21.9 LUFS, LRA 14.7, true peak -3.5 dBTP.
- Sweep and results: docs/one-weather-day-qc.md. Owner review pending; not release-ready.
