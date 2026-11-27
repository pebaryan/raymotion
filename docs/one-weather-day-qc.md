# QC review: One Weather Day (weather-day)

| | |
| --- | --- |
| Composition | `weather-day` (examples/showcase/weather_day.cpp), branch v0.14-weather, uncommitted edits |
| Reviewer | the building agents only (author review). **Owner review pending.** Who approved what: nothing. |
| Date | 2026-11-27 |
| Review stage | final (re-review after the owner's "no thunder / rainbow too early" notes) |
| Story mode | utility (engine demo, no voice; the labels are the script). Not released, not an owner film. |
| Artifacts | docs/one-weather-day-brief.md, out/weather-day.mp4, out/qc/weather-day-sweep/ (+ .md), out/wdqc/ scratch |

## 1. Conceit
Brief: one jetty, one lake, one weather state through a day, every engine feature shown by what it does to the same place.
Restatement after viewing: a time-lapse of one lake and jetty from dawn mist to a thunderstorm to a rainbow and a golden evening, with a readout for each effect.

## 2. Gates
| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief | pass | docs/one-weather-day-brief.md, section 14 updated |
| 1 Script / cue map | pass (text-only cue map in the brief) | |
| 2 Stills | pass | encoded stills at chapter start/middle/end and transitions, and the 54 sweep frames, all opened |
| 2b Reveal and sync | conditional | per-scene notes below; scene-only clips not cut, review was from stills and sweep frames |
| 2c Picture integrity | conditional | sweep table; every pop explained or listed open; keyed effects off vs on |
| 3 Draft muted and with sound | conditional | stills stepped muted; the audio was checked by envelope detection, not by an ear: an author should still listen |
| 4 Final technical QA | pass | ffprobe, full decode, ebur128 below |
| 5 Authored test | conditional | no mute/thumbnail/reskin test run |
| 6 Craft | conditional | see scores |
| 7 Packaging | n/a for a utility demo | no thumbnail or SRT |

## 3. Technical evidence (final MP4)
- Build chain: source edit 22:05:26, exe 22:05:30, final render 22:15:19 (569 s). The render is newer than the exe and the exe than the last source edit.
- ffprobe: h264 1920x1080, 1620 frames, 54.000 s; aac stereo 54.000 s. Full decode (`ffmpeg -f null`): no errors.
- Loudness, final MP4: I -21.9 LUFS, LRA 14.7 LU, true peak -3.5 dBTP. Before this round (first QC): I -20.4 LUFS, LRA 11.3, peak -2.1. Original build: -20.7 / 20.4 / -1.0.
- A mid-round attempt (thunder x1.9, tanh drive 2.2) measured true peak +5.8 dBTP, caused by the white-noise crack saturating the limiter into Nyquist-rate swings (sample peak -2.9 dB but intersample +2.4 dB). Fixed with a band-passed crack, a lower thunder level and a gentler limiter. The day.wav intersample check (aresample 192k astats) is the number to trust, not the sample peak.

## 4. The owner's two notes
1. Lightning and thunder.
   - Before: 3 strikes (26.9, 29.4, 30.8 s). The bright flash frames were about 20 over the whole squall (runs f809-812, f883-890, plus the strike-3 flash); the bolts were 1.6 m wide and the thunder only 10 dB over the bed, clumped at 32.35 / 32.53 / 33.65 s. The owner could not read them.
   - After: 7 strikes (24.6, 25.6, 26.9, 27.8, 29.0, 31.0, 32.4 s) at 0.5 to 3.0 km, left and right of the axis, bolts 4 m wide at intensity 1.6. Bright strike frames in f700-1010 (threshold test, luma-flash plus bolt): 38 frames in six runs (f740-742, f808-817, f836, f871-881, f932-933, f973-983); the 25.6 s far strike registers only as a thin bolt. Stills at the strike frames show the full bolt in frame (f742, f772, f811, f838, f874, f934, f976), lighting the cloud base; f811 shows the cloud and rain lit. The 31.0 s strike is 535 m away over the lake (thunder 1.5 s later).
   - Thunder arrival, logged by RMTN_WD_THUNDER (th3.txt) vs the onset found in the encoded audio (8 kHz envelope, first 10 ms window over 2x the local bed):
     | strike | expected | found |
     | --- | --- | --- |
     | 24.6 s, 1395 m | 28.67 | 28.67 |
     | 27.8 s, 724 m | 29.91 | 29.92 |
     | 29.0 s, 1054 m | 32.07 | 32.08 |
     | 31.0 s, 535 m | 32.56 | not separable, inside the 32.07 rumble (envelope peak 32.69) |
     | 26.9 s, 2316 m | 33.65 | not separable: far rumbles have a 0.14 s attack and sit on the stacked tails (5.8k vs bed 4.5k) |
     | 25.6 s, 3036 m | 34.45 | same |
     | 32.4 s, 1629 m | 37.15 | 37.18 |
     Four of seven onsets verified within 1 frame; three are masked by the neighbouring rumbles and are NOT verified. Arrivals cluster at 32.07-33.65 s by the strike table, which is why they stack.
   - Strike tags (three: 2.3 km, 1.1 km, 535 m) do not collide with each other or the cards in the sweep frames t0031-t0034.
   - Not done: the bolt duration is the engine's profile for the seed; I could not make it longer (no engine change). The bolt is visible about 0.3 to 0.5 s only by the engine's own timing, not measured per bolt.
2. Rainbow too early. The engine's Auto mode draws the bow when rain falls with the sun out. `rainbow.strength` is now a ramp 0 at 33.0 s to 1 at 34.6 s, so the bow is gated off through the squall. Checked on the final MP4: no bow at f850, f930, f990; a faint bow at f1020 (34.0 s); clear at f1060 and f1200. The "bow 41.8 deg" tag is already gated to 35.4-39.8 s. The `Auto` gate is still the engine's behaviour; the film just scales it.

## 5. Gate 2c: sweep (tools/qc_sweep.py, final MP4, 12 flags)
| Flag | Reason |
| --- | --- |
| f739-740 | strike 1 (24.6 s) leader flash. Designed |
| f809 | strike 3 (26.9 s) flash. Designed |
| f822-827 | the same strike's restrikes and the sky relit. Designed (engine strike lighting) |
| f837-842 | strike 4 (27.8 s). Designed |
| f855, f863 | engine's strike light decaying in steps of 15-30 grey levels over 3-12 frames after strike 4/5. Pure function of t (stills reproduce it), not an encode artifact |
| f871-873 | strike 5 (29.0 s). Designed |
| f891 | decay step of strike 5. As above |
| f931-939 | strike 6 (31.0 s), the close one. Designed |
| f973-974 | strike 7 (32.4 s). Designed |
| freeze f1519-1619 | the end-card hold. Designed |
The earlier f766 cloud-pattern pop (25.5 s) from round 1 no longer appears among the flags; its cause was never found, so I do not call it fixed. Flat f1447-1451 (end-card scrim fade) no longer flagged. Shake: none.

## 6. Sweep frames opened (all 54; t0001 = 0 s, one a second)
- t0001-t0006 (00:00-05:00 film, 06:00-08:00 clock): dawn mist, sun low behind the tree line with shafts, jetty and lake mirror, cumulus building; ridge tags appear at t0006. No jetty ghost.
- t0007-t0008: ridge tags 25.0 km (42 %) and 2.6 km (90 %) clear of each other; visibility and sun gauges.
- t0009-t0012: wind and wave gauges arrive, the "Cumulus building" card; ripples start; lake clean.
- t0013-t0016: wind 5.3 to 6.9 m/s, Hs 5 to 7 cm; rippled lake, no ghost or seam; green lawn.
- t0017-t0019: wind 7.4, lighter cumulus giving way to a dark deck; the "A squall line" card appears at t0019.
- t0020-t0022: dark deck over most of the sky, blue gap right; ripples; no rain yet; jetty shadow.
- t0023-t0025: first rain gauge (0 then 2 then 6 mm/h), jetty shadow raked across the lawn, rain streaks start.
- t0026-t0029: rain 10 then 14 mm/h, ground wet 26 / 44 / 59 / 70 %; very dark; the 2.3 km strike tag with a countdown.
- t0030-t0034 (14:52-15:29): bolts and flashes at strike frames, the rain veil lit; tags 2.3 km, 1.1 km and 535 m reading down to "thunder now"; lens drops. No bow.
- t0035-t0036 (15:42-15:55): "Clearing" card, thick clouds breaking, blue gaps, first faint bow, the last tag "thunder now".
- t0037-t0040: the bow brightens, the bow tag (41.8 deg), reflections return in the lake.
- t0041-t0042: purple-gold evening light, mirror lake, double bow fading.
- t0043-t0048: "Calm evening" card, wake gauge, the boat enters left, wake tag and the Kelvin wake lines follow the boat across the jetty (the bow rail passes in front of the boat at t0044).
- t0049-t0054: the end-card scrim fades in over the shot, title, then the seven API rows one at a time, then the logo and link at t0053-t0054.

## 7. Keyed effects, off vs on (stills from the engine, 480x270 mean abs diff)
| Switch | Frame | Mean diff | Pixels > 12 of 129600 |
| --- | --- | --- | --- |
| RMTN_WD_NOBOW | f1100 | 4.69 | 14174 (the bow arc) |
| RMTN_WD_NOVOL | f200 | 6.92 | 31239 (shafts and aerial haze) |
| RMTN_WD_NOCLOUD | f700 | 25.52 | 61780 (the whole deck) |
These stills were taken with the first of the three builds; they are not repeated after the strike edit, which does not touch the three effects.

## 8. Not done, or not verified
- The ridge "% of its light left" readout was not checked against pixel samples. Open.
- No listening watch: the audio was checked by envelope detection and levels only.
- No scene-only clips: scenes were reviewed from stills and sweep frames.
- Camera clearance log (RMTN_WD_CLEARANCE) not run.
- Three of seven thunder onsets are masked and unverified.
- Cause of the round-1 f766 cloud jump unknown (engine); the boat wake wedge is a shallow V at the 2.8 m camera; neither fixed.
- The owner's earlier water-reflection bug (jetty ghost, hard seam) was fixed in the engine (a1fe9b5); the lake in t0013-t0048 shows neither.

## 9. Scores (author-only review; owner review pending; NOT release-ready, no owner approval claimed)
| Axis | Score | Reason |
| --- | --- | --- |
| Authoredness | about 21 / 36 | below the 22 bar |
| Craft | about 12 / 18 | below the 13 bar |
| Verification, Spec-to-screen, Revision | capped at 2 | author-only review; the ridge check, a listen and scene clips are missing |
Critical fails: none known. Gates 0-7 are not all passed (conditional items above), so the film does not meet the release bar.
