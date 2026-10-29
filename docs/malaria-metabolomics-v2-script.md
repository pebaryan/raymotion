# malaria-metabolomics-v2 - script and cue map

Composition: `malaria-metabolomics-v2`
Story mode: **explainer / promo-grade**
Voice: **Qwen3-TTS clone**, 34 sentence clips, **leading clap only**, no end clap
Music: **none**
SFX: sparse paper ticks/pops/stamps/whooshes
Runtime: **181.83 s** (5,455 frames)

## 1. Timing

- `LEAD = 0.40 s`; each sentence is a separate WAV; 0.27 s gap.
- Scene starts (s): `0.00, 7.53, 23.00, 36.17, 49.10, 64.90, 81.07, 92.00, 105.23,
  129.30, 148.00, 157.07, 170.30`; end `181.83`.
- Scene starts (frames): `0, 226, 690, 1085, 1473, 1947, 2432, 2760, 3157, 3879, 4440,
  4712, 5109`.
- SRT: 34 cues, one per sentence; the last cue ends at 2:57.8.

## 2. Scene map

| Scene | Window | Visual proof |
| --- | --- | --- |
| 1 Cover | 0:00-0:07.5 | 3D serum specimen + title |
| 2 Stakes | 0:07.5-0:23.0 | 282M / 610K / US$5.4B |
| 3 Metabolomics | 0:23.0-0:36.2 | 3D Phe / S1P / retinol |
| 4 Trap | 0:36.2-0:49.1 | 220-column matrix |
| 5 Data | 0:49.1-1:04.9 | black cohort table + Burkina Faso map |
| 6 Cleaning | 1:04.9-1:21.1 | PCA islands + ComBat diagnostic |
| 7 Narrowing | 1:21.1-1:32.0 | Venn 55/20/33 |
| 8 Honest test | 1:32.0-1:45.2 | LODO lanes |
| 9 Result | 1:45.2-1:59.3 | practice vs real dot plot |
| 10 Candidates | 1:59.3-2:08.0 | 3D candidate plates |
| 11 Demo | 2:08.0-2:17.1 | 11 values -> model -> reasons |
| 12 Limits | 2:17.1-2:27.1 | is/is-not ledger + Indonesia map |
| 13 Close | 2:27.1-3:01.8 | next cards + green plate + stamp |

## 3. Sentence cues

| Beat | Cue | Caption / VO | Start | End |
| --- | --- | --- | --- | --- |
| 1 | 1.1 | Can a machine learn malaria's chemical signature, and does it still work on data it has never seen? | 0:00.36 | 0:06.30 |
| 2 | 2.1 | Malaria infected an estimated 282 million people in 2024 and killed 610,000. | 0:07.86 | 0:14.43 |
| 2 | 2.2 | Progress has stalled, parasite gene deletions can defeat rapid tests, and funding falls billions short. | 0:14.70 | 0:21.43 |
| 3 | 3.1 | Metabolomics measures the small molecules in blood: amino acids, vitamins, lipids. | 0:23.30 | 0:29.00 |
| 3 | 3.2 | Infection changes the mix, and that chemical fingerprint is a window into what the body is doing. | 0:29.26 | 0:34.56 |
| 4 | 4.1 | Machine learning can search that fingerprint for patterns. | 0:36.43 | 0:39.06 |
| 4 | 4.2 | But with hundreds of molecules and only a few hundred samples, a model can simply memorise what it has seen. | 0:39.33 | 0:45.66 |
| 4 | 4.3 | Memorising isn't learning. | 0:45.93 | 0:47.46 |
| 5 | 5.1 | We used two public serum datasets from children in Burkina Faso: 305 samples, 220 metabolites measured in both. | 0:49.33 | 0:57.43 |
| 5 | 5.2 | Many samples are repeat visits by the same child, so this is not 305 independent people. | 0:57.70 | 1:03.23 |
| 5 | 6.1 | The data is log-scaled, normalised, gap-filled and standardised. | 1:05.10 | 1:09.43 |
| 5 | 6.2 | Even then the two datasets sit on separate islands, a technical gap. | 1:09.70 | 1:13.80 |
| 5 | 6.3 | A statistical correction can merge them in a picture, but we used it only as a diagnostic. | 1:14.06 | 1:19.36 |
| 6 | 7.1 | Two hundred and twenty candidates is too many. | 1:21.23 | 1:23.56 |
| 6 | 7.2 | So two different methods, one linear, one tree-based, each vote. | 1:23.83 | 1:27.76 |
| 6 | 7.3 | Only metabolites both pick survive. | 1:28.03 | 1:30.26 |
| 6 | 8.1 | Here's the honest test. | 1:32.13 | 1:33.50 |
| 6 | 8.2 | Train on one whole dataset, test on the other, which the model has never seen, then swap. | 1:33.76 | 1:38.66 |
| 6 | 8.3 | Cleaning, selection and tuning are learned from the training data alone. | 1:38.93 | 1:42.46 |
| 6 | 8.4 | No peeking. | 1:42.73 | 1:43.46 |
| 7 | 9.1 | While tuning, models scored up to 0.99 AUC, where 0.5 is a coin flip and 1 is perfect. | 1:45.33 | 1:52.40 |
| 7 | 9.2 | On the unseen dataset they landed between 0.50 and 0.70. | 1:52.66 | 1:57.33 |
| 7 | 9.3 | Logistic regression and a heavily tuned XGBoost both averaged about 0.64. | 1:57.60 | 2:03.06 |
| 7 | 9.4 | More tuning didn't close the gap. | 2:03.33 | 2:05.20 |
| 7 | 9.5 | That gap is the finding. | 2:05.46 | 2:07.00 |
| 8 | 10.1 | Fit on all the data, the same selection leaves eleven metabolites. | 2:09.36 | 2:13.46 |
| 8 | 10.2 | The strongest three: phenylalanine, pointing up; sphingosine-1-phosphate and retinol, down. | 2:13.73 | 2:20.13 |
| 8 | 10.3 | Each fits published malaria literature, which makes them worth a biologist's attention, not a discovery. | 2:20.40 | 2:26.16 |
| 8 | 11.1 | An open-source demo takes the eleven values and explains each prediction. | 2:28.03 | 2:32.53 |
| 8 | 11.2 | A research illustration, not a diagnostic. | 2:32.80 | 2:35.20 |
| 12 | 12.1 | So this is a transparent, reusable workflow and a short list for expert review. | 2:37.06 | 2:42.36 |
| 12 | 12.2 | Not a validated panel, not a diagnostic, and not new knowledge about malaria in Indonesia. | 2:42.63 | 2:48.40 |
| 13 | 13.1 | Next: independent cohorts, targeted confirmation, and prospective local studies. | 2:50.26 | 2:55.06 |
| 13 | 13.2 | A modest result, and a method you can trust. | 2:55.33 | 2:57.83 |

## 4. Source mapping

Same claims and ledger as `docs/malaria-metabolomics-script.md`:

- Cases/deaths/funding/pfhrp2: supplied brief numbers sheet / WHO 2024.
- Datasets, 305 samples, 220 shared, repeat visits: Table 1, §2.2.
- Cleaning/PCA/ComBat: §2.3.5; ComBat diagnostic only.
- Venn 55/33/20: numbers sheet; 20/34 fold sets; final 11; label illustrative.
- AUC values: Tables 6-7; 2 folds; no ranking claim.
- Candidate coefficients: Table 5; top-5 SHAP differed.
- 3D structures: PubChem CIDs 6140 / 5283560 / 445354; illustrative.

## 5. 3D / NPR

- Phenylalanine and retinol: PubChem 3D SDFs, `chem::addMolecule`, Gooch + ink.
- S1P: PubChem 2D SDF/SMILES, `chem::embed3d`; approximate geometry.
- Maps: bundled Natural Earth 1:110m via `geo::countries()`; orthographic Africa/Burkina Faso in scene 5 and Equal Earth Indonesia in scene 12. Illustrative geography, not a data overlay.
- Models are box-centred and normalised to a common visual radius.
- 3D appears only on the cover, the metabolomics idea and the candidate payoff.
- No 3D PCA, 3D Venn or 3D bar chart; the data proof stays 2D.

## 6. Guardrails

- No “validated biomarker”, “diagnostic accuracy”, “detects malaria”, “AI diagnoses”.
- ComBat is diagnostic only; 20:1 is feature reduction.
- Cohorts are West African children with repeat visits; not Indonesian.
- The 11-metabolite model has no held-out performance figure.
