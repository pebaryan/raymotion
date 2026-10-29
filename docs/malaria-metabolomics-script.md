# malaria-metabolomics - script and cue map

Composition: `malaria-metabolomics`
Story mode: **explainer**
Voice: **Qwen3-TTS clone of the user**, 34 sentence clips, **clap only at the beginning**
of each take (no end clap); trimmed between the clap and the last recognised word
Music: **none** (per brief)
SFX: sparse shared paper ticks, pops, stamps, whooshes; no bed
Captions: burned mono captions + `out/malaria-metabolomics.srt`
Runtime: **191.63 s** (5,749 frames)

## 1. Timing

- `LEAD = 0.37 s`; the first sentence starts at `scene start + 11 frames`.
- No end clap. Each sentence is a separate WAV; a 0.27 s gap separates sentences.
- Scene starts (frames): `0, 234, 712, 1121, 1523, 2011, 2525, 2882, 3308, 4059, 4649, 4935, 5361`;
  end `5749`.
- The SRT uses the 34 measured sentence starts and ends.

## 2. Chapter map

| Ch | Window | Length |
| --- | --- | --- |
| 1 | 0:00.00-0:07.80 | 7.80 s |
| 2 | 0:07.80-0:23.73 | 15.93 s |
| 3 | 0:23.73-0:37.37 | 13.63 s |
| 4 | 0:37.37-0:50.77 | 13.40 s |
| 5 | 0:50.77-1:07.03 | 16.27 s |
| 6 | 1:07.03-1:24.17 | 17.13 s |
| 7 | 1:24.17-1:36.07 | 11.90 s |
| 8 | 1:36.07-1:50.27 | 14.20 s |
| 9 | 1:50.27-2:15.30 | 25.03 s |
| 10 | 2:15.30-2:34.97 | 19.67 s |
| 11 | 2:34.97-2:44.50 | 9.53 s |
| 12 | 2:44.50-2:58.70 | 14.20 s |
| 13 | 2:58.70-3:11.63 | 12.93 s |

## 3. Sentence cues

| Scene | Line | Caption / VO | Voice start | Voice end |
| --- | --- | --- | --- | --- |
| 1 | 1 | Can a machine learn malaria's chemical signature, and does it still work on data it has never seen? | 0.367 | 6.300 |
| 2 | 1 | Malaria infected an estimated 282 million people in 2024 and killed 610,000. | 8.167 | 14.733 |
| 2 | 2 | Progress has stalled, parasite gene deletions can defeat rapid tests, and funding falls billions short. | 15.000 | 21.733 |
| 3 | 1 | Metabolomics measures the small molecules in blood: amino acids, vitamins, lipids. | 24.100 | 29.800 |
| 3 | 2 | Infection changes the mix, and that chemical fingerprint is a window into what the body is doing. | 30.067 | 35.367 |
| 4 | 1 | Machine learning can search that fingerprint for patterns. | 37.733 | 40.367 |
| 4 | 2 | But with hundreds of molecules and only a few hundred samples, a model can simply memorise what it has seen. | 40.633 | 46.967 |
| 4 | 3 | Memorising isn't learning. | 47.233 | 48.767 |
| 5 | 1 | We used two public serum datasets from children in Burkina Faso: 305 samples, 220 metabolites measured in both. | 51.133 | 59.233 |
| 5 | 2 | Many samples are repeat visits by the same child, so this is not 305 independent people. | 59.500 | 65.033 |
| 6 | 1 | The data is log-scaled, normalised, gap-filled and standardised. | 67.400 | 71.733 |
| 6 | 2 | Even then the two datasets sit on separate islands, a technical gap. | 72.000 | 76.100 |
| 6 | 3 | A statistical correction can merge them in a picture, but we used it only as a diagnostic. | 76.367 | 81.667 |
| 7 | 1 | Two hundred and twenty candidates is too many. | 84.533 | 86.867 |
| 7 | 2 | So two different methods, one linear, one tree-based, each vote. | 87.133 | 91.067 |
| 7 | 3 | Only metabolites both pick survive. | 91.333 | 93.567 |
| 8 | 1 | Here's the honest test. | 96.433 | 97.800 |
| 8 | 2 | Train on one whole dataset, test on the other, which the model has never seen, then swap. | 98.067 | 102.967 |
| 8 | 3 | Cleaning, selection and tuning are learned from the training data alone. | 103.233 | 106.767 |
| 8 | 4 | No peeking. | 107.033 | 107.767 |
| 9 | 1 | While tuning, models scored up to 0.99 AUC, where 0.5 is a coin flip and 1 is perfect. | 110.633 | 117.700 |
| 9 | 2 | On the unseen dataset they landed between 0.50 and 0.70. | 117.967 | 122.633 |
| 9 | 3 | Logistic regression and a heavily tuned XGBoost both averaged about 0.64. | 122.900 | 128.367 |
| 9 | 4 | More tuning didn't close the gap. | 128.633 | 130.500 |
| 9 | 5 | That gap is the finding. | 130.767 | 132.300 |
| 10 | 1 | Fit on all the data, the same selection leaves eleven metabolites. | 135.667 | 139.767 |
| 10 | 2 | The strongest three: phenylalanine, pointing up; sphingosine-1-phosphate and retinol, down. | 140.033 | 146.433 |
| 10 | 3 | Each fits published malaria literature, which makes them worth a biologist's attention, not a discovery. | 146.700 | 152.467 |
| 11 | 1 | An open-source demo takes the eleven values and explains each prediction. | 155.333 | 159.833 |
| 11 | 2 | A research illustration, not a diagnostic. | 160.100 | 162.500 |
| 12 | 1 | So this is a transparent, reusable workflow and a short list for expert review. | 164.867 | 170.167 |
| 12 | 2 | Not a validated panel, not a diagnostic, and not new knowledge about malaria in Indonesia. | 170.433 | 176.200 |
| 13 | 1 | Next: independent cohorts, targeted confirmation, and prospective local studies. | 179.067 | 183.867 |
| 13 | 2 | A modest result, and a method you can trust. | 184.133 | 186.633 |

## 4. Source to scene

| Scene | Claim | Source |
| --- | --- | --- |
| 2 | 282M cases, 610K deaths, US$5.4B gap, 46 countries with pfhrp2 deletions | supplied project brief numbers sheet (manuscript §1 / WHO 2024) |
| 3 | Serum metabolites; infection changes the mix | supplied brief; conceptual |
| 4 | 220 metabolites / ~300 samples; overfitting | supplied brief |
| 5 | 305 observations; 220 shared; 199/106, 263/253; Burkina Faso; repeat visits | supplied brief, Table 1, §2.2 |
| 6 | Cleaning steps; PCA 19.33 -> 0.04; ComBat diagnostic only | supplied brief, §2.3.5 |
| 7 | 55 LASSO / 33 RF / 20 shared illustrative; fold sets 20+34; final 11 | supplied brief numbers sheet; the prose's 35/20/13 is superseded |
| 8 | LODO; train/test swap; preprocessing/tuning on train only | supplied brief, §2.4 |
| 9 | Inner 0.931-0.996; held-out 0.499-0.699; LR 0.646/0.638; XGB 0.569/0.647; RF 0.499/0.633; tuned means 0.643/0.609 | supplied brief, Tables 6-7 |
| 10 | 11 candidates; Phe +1.214, S1P -1.121, retinol -0.944; top-5 SHAP differed | supplied brief, Tables 4-5 |
| 11 | Demo 11 values -> prediction + why | supplied brief, Figure 10 concept |
| 12 | Is / is not; West African children; not Indonesian | supplied brief, guardrails |
| 13 | Next steps; credits | supplied brief |

## 5. Guardrails

- Held-out is the headline; inner-CV is labelled optimistic.
- "Candidate", "proof-of-concept", "modest", "prioritised for review".
- Never "validated biomarker", "diagnostic accuracy", "detects malaria", "AI diagnoses malaria".
- ComBat is diagnostic only; it did not fix the batch effect for the predictive pipeline.
- The 20:1 is feature reduction, not performance gain.
- The final 11-metabolite model has no held-out performance figure.
- The cohorts are West African children with repeat visits; not Indonesian.
- Blood sampling is not called non-invasive.

## 6. Voice generation

- 34 sentence clips, one per sentence, generated separately with Qwen3-TTS.
- Each take begins with `Clap.`; the clap is the trim marker. There is **no end clap**.
- Trim: start after the clap word, end at the last recognised word + 0.25 s.
- ffmpeg normalises to 48 kHz / -16 LUFS; the comp plays at 0.75 clip gain to balance
  the dual-mono voice against the SFX.
- Files: raw `assets/vo/malaria-sentences/`, final `assets/vo/malaria-sentences-norm/`.

## 7. Still and final checks

- Stills: 90, 450, 900, 1250, 1700, 2250, 2750, 3200, 3800, 4600, 5000, 5250, 5680,
  5750.
- ffprobe, full decode, loudness/peak and encoded frames are in the QC review.
