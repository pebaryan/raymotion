# QC: bundaran-hi-banner

Story mode: narrative (illustrated, labelled ILUSTRASI). 24 s, 1920x1080, 30 fps.

## Technical (final render `out/bundaran-hi-banner.mp4`)

- ffprobe: h264 1920x1080 30 fps + aac, 24.000 s.
- Full decode (`ffmpeg -f null`): no errors.
- Loudness -17.9 LUFS integrated, sample peak -5.6 dBFS (true-peak flag not separately logged).
- Executable (18:16:20) is newer than the last source edit (18:16:17); render rebuilt after it.
- `raymotion list` run from another directory shows the film.
- Frame extracted from the encoded MP4 at 12 s (`tmp/bh/mp4_12.png`).

## Scene review (stills, one scene at a time)

| Scene | Frames | Note |
|---|---|---|
| Place | 20, 100 | High wide view drifts to the crowd; caption and ILUSTRASI tag readable. Approved. |
| Standoff | 200, 300 | Crowd, barricade, shield line, monument all in frame. Approved. |
| Climb | 330, 380, 420, 430 | Climbers visible on the plinth with the roll; poses are simple. Approved with note. |
| Unroll | 500, 540, 620 | Banner unrolls down the shaft; text legible by 620. Approved. |
| Card | 690 | End card readable; wordmark still revealing at 690. Approved. |

## Not done / honest limits

- Draft not watched muted or with sound by a person; sync is judged from stills and the cue map only.
- Not every transition frame (130/140, 290/300, 490/500, 640/645) was inspected.
- Banner text is rotated sideways on the cloth (two lines); legible but not ideal.
- Cloth costs about 9 ms/frame (64x64), over the 5 ms target; the render itself ran in 10 s.
- Authoredness/Craft scores not yet filled from QC-REVIEW.md; this is not yet a release.

## Revision after owner review (second round)

- Statue: held waving pose (outer arm raised, inner arm bent), no animation.
- Pool widened to 20 m radius with a stone coping and island; ring road (4 lanes with lane lines) around it, pavement beyond.
- Street furniture added: lamp posts, globe lamps on the coping, traffic lights, trees. Sky has clouds and haze fog.
- Barricade and police moved onto the ring road; crowd behind them.
- Climbers now go up the east side of the shaft on two hemp ropes (physics::Rope), over the cap lip and along the cap to the front; the camera orbits to the east face with them and swings back for the unroll.
- Bug found: `camera.up` lambdas that assign a whole `Camera` wipe `up`, freezing the camera after frame 0 in renders (stills hide it). Fixed here and in `voxel_splats.cpp` (its committed render had a frozen camera; re-render it).
- Open: the carried roll reads as a flat slab from the side; passes not re-reviewed one scene at a time after this round; draft not watched or heard by a person.

## Revision: a showcase for rope and cloth (third round)

- Climbers go up opposite faces (west, east) on their own ropes; the roll is hoisted between them on two lines and is untied only when both are on top.
- Rope: the climbing ropes are hauled up after the climb (the free end follows a hand up the shaft and over the lip, so each rope folds into a loop); the hoisting lines are released at the roll end (`pin(..., until)`) and fall and swing from the hands instead of vanishing.
- Cloth: seven red-and-white flags on poles in the crowd, pinned along the pole and flown by the new aerodynamic wind (`Wind::drag`, `Wind::wavelength`); the poles lift and pump at the cheer. The banner uses the same wind, pressed to the stone with a ripple in its free end.
- Callouts name the feature where it acts (flags 5.4-8.4 s, rope 11.0-13.6 s, rope haul 14.3-16.2 s, banner 17.4-20.6 s), one at a time, on a dark backing. End card: `physics::Cloth · physics::Rope`.
- Engine: aerodynamic cloth wind added to `physics::Wind` (opt-in; old behaviour unchanged), with a test (cloth facing the wind billows, edge-on stays put, deterministic). `ctest -R "physics|scene3d|crowd"` passes.
- Skyline: ~110 generated towers with generated facades in five rings, fog density 0.0042; banner recoloured dark navy with off-white text for contrast; caption year 2026.
- Final render: 1920x1080 30 fps 24.000 s, full decode clean, -17.9 LUFS, peak -5.6 dBFS; frames checked from the MP4 at 7, 15, 20 s.
- Open: hauled rope loops look kinked at the top; flags droop during low gusts; not re-reviewed scene by scene; not watched or heard by a person.

## Revision: the junction from OpenStreetMap (fourth round)

- Background is real data: `tools/osm_bundaran_hi.py` converts an Overpass extract (650 m round the pool) into CityJSON 2.0 LoD1, `assets/data/bundaran-hi/bundaran-hi.city.json`: 1501 buildings (97 with an OSM height, the rest guessed from footprint area and marked so), 474 road centre lines as TrafficArea ribbons with class/lanes/oneway/width. The film loads it with `city::load` and draws it with `city::mesh`.
- Buildings: three tiled facade styles by height (shophouse, ribbon office, curtain wall), a tint per building; `city::Look::textureSize`/`flatUV` added for wall texture coordinates, and `DynamicTexture::repeat` so a drawn tile repeats.
- Roads: OSM ribbons (busway in concrete), dashed lane lines from each road's lane count, trees and street lamps along primary/secondary/tertiary roads, kept off buildings and other roads; merged into a few meshes.
- Roundabout at the OSM radii: pool edge 40 m, planted ring to 49.5 m, ring road 50-65.5 m with four lanes.
- Monument rebuilt at its real proportions (~30 m with figures) as a hollow frame: two side walls, a sill, a cap, open front and back. Banner lengthened to 11 x 77 (2.5 x 19 m), one line of text.
- Crowd (230) on Jl. Sudirman south of the roundabout, barricade and police across its mouth; cameras moved onto the east carriageway to keep Halte Tosari (in the OSM data) out of the low shots.
- Attribution: "Bangunan dan jalan: © kontributor OpenStreetMap" under the caption; "Map data © OpenStreetMap contributors (ODbL)" on the end card.
- Final render: 1920x1080 30 fps 24.000 s, full decode clean, -17.9 LUFS, peak -5.6 dBFS; frames checked from the MP4 at 9 and 14 s, stills at 0.7, 5, 10, 13.3, 16.7, 18.7, 20, 21.3, 23 s.
- Open: facade moiré on glass towers at grazing angles; banner text loses some contrast in the haze at 130 m; the climbers' carried roll still slab-like; not re-reviewed scene by scene; not watched or heard by a person.
- Fix (z-fighting): OSM outlines that overlap or share walls (e.g. the Grand Hyatt tower inside the Plaza Indonesia mall outline) drew coplanar walls that flickered as striped bands. The converter now drops buildings wholly inside taller ones and pulls the others in by 0.3 m per step below each taller building they overlap; a check over the output finds no coplanar walls left. Roads: camera near plane raised to 1 m (from 0.05) and the road ribbons lifted to 6-10 cm, so the roads no longer flicker against the pavement. Re-rendered and verified as above.
- Caption: a darker scrim under the type that fades out upwards and to the right, and the subtitle and OSM credit in off-white instead of grey, so the caption reads against the mid-grey plaza. Rope callout (11.0-13.6 s): anchored on the east rope 2.5 m below its climber instead of at a fixed 6 m, so it rides up with the camera instead of dropping out of frame at about 12.2 s. Re-rendered and verified (ffprobe, full decode, -17.9 LUFS, MP4 frames at 2 and 13.2 s).
