# Reels: short vertical films with taste

Read this before making any vertical or feed cut (9:16 or 4:5, 15-90 s). It sits on top
of `authoring-standard.md`: the brief, the proof objects, the reveal map and the QC gates
all still apply. This file adds what a reel needs that a landscape explainer does not.

The defaults below are decisions, not suggestions. Use them unless the brief says why
not. When this file and your instinct disagree, follow this file and write down the
disagreement in the brief.

---

## 1. What a reel is

A reel is watched by someone scrolling, with the sound off and a thumb ready to leave.
You have about one second to earn the second second. Everything below follows from that.

- **One claim.** A reel proves one thing. If the long version has ten chapters, the reel
  keeps the three or four that carry the argument and drops the rest. "Same film,
  shorter" is wrong; "the one idea, told for a phone" is right.
- **One object.** One proof object (a table, a map, a tower, a pair of islands) that
  changes state through the whole reel. The reel is the story of that object.
- **Muted first.** The picture and the on-screen words must carry the argument without
  sound. Voice and score make it better; they may not be what makes it work.
- **Not a crop.** Never reframe a 16:9 film into 9:16. Rebuild the staging for a tall
  frame (section 4).

Cautionary tell: a numbered list of cards ("1. ... 2. ... 3. ...") with a hook card on
top and a practice card at the end. That is a slide deck in portrait. It has no object
that changes, so nothing is proved.

---

## 2. Defaults

| Decision | Default | Notes |
| --- | --- | --- |
| Frame | **1080 x 1920, 30 fps** | 1080 x 1350 (4:5) for a feed post that is not a reel |
| Length | **30-60 s** | 45 s is a good target. Under 20 s rarely proves anything; over 75 s needs a reason |
| Beats | **4-6**, 5-12 s each | one idea per beat; a beat is one state change of the object |
| Cuts | **hard cuts on the beat** (`Series`) | no fades between beats; one fade at most, into the end card |
| Camera | **one move per beat** | a crane, a push or a pan; never two moves at once, never idle drift |
| Side margin | **72-96 px** | |
| Top safe zone | **180 px** clear | platform header, progress bar, account name |
| Bottom safe zone | **300-400 px** clear | caption UI, like/share buttons, description text |
| Right edge | keep key objects **140 px** from the right | the action-button column covers it |
| Art zone | y **260-1360** (9:16) | the proof object lives here |
| Caption zone | y **1420-1700** (9:16) | above the bottom safe zone; two lines, three at most |
| Headline / hook | **96-124 px**, heavy weight | Archivo Black or Inter Bold |
| Captions | **44-56 px**, 2 lines max, ~24 characters per line | anything under **26 px is unreadable** on a phone |
| Labels on the object | **30-40 px** | fact chips smaller than this fail at feed size |
| Reading rate | **2.5 words/s**, constant | word-by-word karaoke: spoken words in ink, upcoming at ~32 % |
| Entrances | 10-16 frames, ease-out cubic | |
| Loudness | **-16 LUFS** integrated, true peak **<= -1.5 dBTP** | phones clip early; leave more headroom than landscape |
| Thumbnail / cover | a frame chosen in the brief, tested at **180 x 320** | portrait covers are judged at portrait size, not 320 x 180 |

Scale rule: type in a reel is about **1.3x** the landscape size for the same role. If you
are porting a landscape film, multiply every text size by 1.3 and then check that the
caption still fits in two lines. If it does not, cut words, not size.

---

## 3. The beat shape

Write the beat table in the brief in this shape. It is the one that worked in every
reel that passed QC in this repository.

| Beat | Time | Job | What the viewer sees |
| --- | --- | --- | --- |
| **Hook** | 0-3 s | state the tension in a few words; show the object already moving | the question and the first state of the object |
| **Setup** | 3-12 s | show what the object is and how to read it | the object labelled once, one unit at a time |
| **Turn** | 12-30 s | the object changes; this is the proof | the state change the conceit promised |
| **Consequence** | 30-45 s | what the change means; the honest limit | a second, smaller change or a caveat stamp |
| **Close** | last 3-5 s | the line people repeat; hold for the cover | the final state of the object, the title, a hold |

Rules for each beat:

- **Hook.** The first frame is **already in motion**: a tower rising, a card mid-slide, a
  plate mid-flip. A static title card on frame 0 loses the scroll. The hook line is the
  tension, not the topic: `Why ich bin — but du bist?` beats `German verb: sein`.
  `ONE WORD. 60x THE PRICE.` beats `Tokenizer bias`. Six words or fewer.
- **Setup.** Introduce the object's parts in the order the line names them, one entrance
  at a time (reveal map). Label each part once, on the object or in a key, never both.
- **Turn.** This is the reason the reel exists. Give it the most time and the clearest
  motion: a bar retracts, a model is removed, a constraint crosses out an answer.
  Animate the relation, not decoration.
- **Consequence.** Say what the turn costs or where it stops being true. A reel that
  only celebrates reads as an ad. One stamp (`PROTOTYPE`, `THIS INSTANCE ONLY`) is
  enough.
- **Close.** The final state of the object plus one line, held **at least 2 s** after the
  last word so the cover frame and the loop point are readable. If the close frame and
  the thumbnail are the same image, good.

---

## 4. Rebuild for the tall frame

A landscape explainer argues side by side: columns, skylines, left-to-right sweeps. A
tall frame argues top to bottom. Keep the **objects** and change the **axis**:

| Landscape staging | Vertical staging |
| --- | --- |
| three towers side by side, camera pulls back | one tower at a time; the camera cranes **up** the tall one until it leaves frame |
| bar chart sweeping left to right | a stack of horizontal rows; a wipe falls **down** the frame and each row changes as it passes |
| balance scale tipping left/right | a vertical pivot; the tip reads as a tilt |
| sentence on a table, scanner sweeps across | a sentence strip slides **down** through a blade; pieces drop into a vertical tray |
| two panels side by side | two panels stacked; the comparison reads top-to-bottom, the same unit on both |

If an idea only works wide, it is not in the reel. Put it in the long version and link
to it.

---

## 5. Text with taste

- **One text layer at a time.** Either the object carries the answer or the caption
  does, never both. Showing the answer in a sentence strip *and* in the caption at the
  same moment halves the impact of both.
- **Karaoke, not subtitles.** Use one line that fills word by word at a constant rate
  (`sub::Style::upcoming` with alpha, or your own per-word ink). The eye follows the
  ink; it does not have to find where the voice is.
- **No caption boxes, no glow.** A dark outline (`outlineWidth` 3-4) is enough on a
  designed background. Boxes and glows read as auto-generated.
- **Cut words before shrinking type.** Two lines at 48 px beat three lines at 36 px.
- **Plain phrase first, term second.** Define a term once, on screen, the first time it
  is needed.
- **Numbers carry units** even in a reel: `60x the tokens per word`, not `60x`.
- **Sources go in the post description**, plus one short source line on the end card
  (`Source: Aryan et al., 2021`). No footnotes mid-reel.

---

## 6. Sound with taste

- Muted is the primary audience; sound is the second pass. Check both.
- **SFX on picture beats only.** A stamp when a stamp lands, a tick per plate flip, a
  chime per valid answer, a low hit on the turn. Never a whoosh on every cut.
- The score is quiet under speech (duck about 6 dB) and **resolves on the close**. A
  loop that ends mid-bar sounds unfinished.
- Voice at about **150-165 words per minute**. A 45 s reel holds 90-115 spoken words with
  room to breathe. A 45 s script pasted into a 38 s voice is the classic failure: measure
  the WAVs first, then set the beats.

---

## 7. The end and the loop

- The last frame is designed, not left over: final object state, the title or the line
  people repeat, a small wordmark if the mode needs one. Keep all of it inside the safe
  zones; a wordmark at 3/4 height reads, one in the bottom 300 px is covered.
- Hold the end at least 2 s after the last word.
- If the reel loops on the platform, the last frame should lead back into the first:
  the empty table returns, the tower sinks to the baseline. Not required, but it is the
  cheapest way to make a reel feel finished.

---

## 8. Failure gallery (from real reviews in this repository)

Each of these happened in a reel built here and was caught at QC. Check for them before
you ask for a review.

| Failure | Seen in | Fix |
| --- | --- | --- |
| a link line crossing a card it is not attached to | `a1-sein-six-seats` S1 | route connectors around objects; check every still for crossings |
| a label (`YOUR TURN`) placed inside the proof object | `a1-sein-six-seats` S5 | labels sit outside the object or replace a part of it |
| the answer shown twice at once (strip + caption) | `a1-sein-six-seats` S5 | one text layer at a time |
| a script written for 45 s, voice measured at 38.5 s | `a1-sein-six-seats` | measure the voice, then set beats; or add deliberate holds |
| fact chips too small to read at feed size | `asp-feed-v2` | labels on the object at 30-40 px minimum |
| unlabeled icons that read as part of the world | `here-there-feed-v2` | a small `RULE` label, or a different visual register for meta-objects |
| a removed item drifting into the gutter | `here-there-feed-v2` | removed things land in a visible tray; removal is part of the proof |
| the thumbnail drawn with two masks swapped | `here-there-feed-v2` | check the cover frame against the brief, not only the film |
| the first score peaking at -1.1 dBTP | `here-there-feed-v2` | measure true peak; target <= -1.5 dBTP |
| a generic farmer/cat card per beat, no proof | `asp-feed` v1 | replace cards with one object whose state change is the argument (v2: a map with two answer sets) |
| the end-card wordmark too small and too low | `asp-feed-v2` | 2x size, 3/4 height, clear of the bottom safe zone |

---

## 9. Reel pre-flight (add to the brief)

All must be **yes** before code:

- [ ] The reel proves **one** claim, and I can name the one object that proves it.
- [ ] The hook line is six words or fewer and states a tension, not a topic.
- [ ] Frame 0 is already in motion.
- [ ] The beat table has 4-6 beats in the hook / setup / turn / consequence / close shape.
- [ ] Staging is rebuilt for the tall frame; nothing depends on side-by-side layout.
- [ ] Every object, label and caption stays inside the safe zones (top 180, bottom
      300-400, right 140).
- [ ] Captions: 44-56 px, two lines, constant 2.5 words/s, one text layer at a time.
- [ ] Cuts between beats are hard cuts; one camera move per beat.
- [ ] Voice (if any) was measured before the beats were timed.
- [ ] The close holds 2 s after the last word; the cover frame is chosen and tested at
      180 x 320.
- [ ] Sources are in the post description and one source line is on the end card.

## 10. Reel QC additions

On top of `quality-control.md`:

- Review stills at **phone size**: scale each still to 360 px wide and read it. If a
  label is not readable there, it is not readable on a phone.
- Overlay the safe zones on at least the hook, turn and close stills.
- Watch the draft muted on a phone-sized window, from the first frame, as if scrolling:
  would you stop by second two?
- Check the cover at 180 x 320.
- Record the reel defaults you broke and why in the QC review.

See `worked-examples.md` for complete reels and films with the reasoning behind each
decision.
