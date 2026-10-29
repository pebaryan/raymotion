# malaria-metabolomics-v2 - authoring brief

> Status: **voice + 3D + geography preview built** (2026-10-27). This v2 keeps the
> supplied brief's **13-scene cadence**, adds **3D molecular specimens with NPR/Gooch
> shading**, and uses raymotion's geo layer for the **Burkina Faso / Indonesia** contrast.
> The manuscript PDF and `references/FRAME.md` are still absent; numbers come from the
> supplied project brief, and the voice is a Qwen3-TTS clone preview.

| | |
| --- | --- |
| Composition | `malaria-metabolomics-v2` |
| Story mode | **explainer / promo-grade** |
| Owner / agent | author-agent |
| Runtime | **181.8 s** (5,455 frames at 30 fps) |
| Platform / aspect | 1920x1080, 30 fps, presentation |
| Voice | **Qwen3-TTS clone**, 34 sentence clips, **leading clap only**, no end clap |
| Music | **none** |
| Audience | internal research team and life-science stakeholders |
| Source material | `project/001-malaria-metabolomics-explainer.md`; manuscript reference absent |

## 1. The idea

**One serum sample. Two exam rooms. One gap.**

The film is a paper laboratory with three 3D specimen windows. The 2D paper world carries
the evidence; the 3D windows carry the chemistry. The model’s **0.99 practice score** and
**~0.64 held-out score** are the spine.

## 2. Why v2

v1 was a faithful 13-scene lecture. v2:
- keeps the **13-scene yapper cadence** and tightens each scene;
- adds **3D ball-and-stick specimens** in Gooch/ink NPR for chemistry;
- adds **raymotion geo maps**: Burkina Faso in the data scene and Indonesia in the
  limits scene, so the two geographies are seen, not only captioned;
- keeps the **2D dot plot, PCA, LODO and is/is-not ledger** for clarity;
- uses **sentence-level voice** so each caption and cut has one spoken unit;
- gives the cold open and candidate payoff a real specimen object.

## 3. Scene table

| # | Window | Scene | On-screen proof | raymotion |
| --- | --- | --- | --- | --- |
| 1 | 0:00-0:07.5 | Cover | 3D serum specimen + "DOES IT TRAVEL?" | `g3d`, `chem::addMolecule`, Gooch |
| 2 | 0:07.5-0:23.0 | Stakes | 282M / 610K / US$5.4B + pfhrp2 note | 2D stat grid |
| 3 | 0:23.0-0:36.2 | Metabolomics | 3D Phe / S1P / retinol specimens | 3D plates + Gooch/ink |
| 4 | 0:36.2-0:49.1 | Trap | 220-column matrix + answer key | 2D matrix/stamp |
| 5 | 0:49.1-1:04.9 | Data | public cohort ledger, black table, Burkina Faso map | 2D table + `geo::` |
| 6 | 1:04.9-1:21.1 | Cleaning | log/PQN/KNN/z, PCA islands, ComBat diagnostic | 2D PCA |
| 7 | 1:21.1-1:32.0 | Narrowing | Venn 55/20/33 | 2D Venn |
| 8 | 1:32.0-1:45.2 | Honest test | four LODO lanes | 2D lanes |
| 9 | 1:45.2-1:59.3 | Result | practice vs real dot plot, 0.99 vs ~0.64 | 2D plot |
| 10 | 1:59.3-2:08.0 | Candidates | 8 tiles -> 3D Phe / S1P / retinol | 3D plates |
| 11 | 2:08.0-2:17.1 | Demo | 11 values -> model -> ranked reasons | 2D UI |
| 12 | 2:17.1-2:27.1 | Limits | is/is-not ledger + Indonesia map | 2D ledger + `geo::` |
| 13 | 2:27.1-3:01.8 | Close | three next cards, green plate, stamp | 2D cards + green ground |

## 4. Script

Full sentence cue table and source mapping: `docs/malaria-metabolomics-v2-script.md`.
34 sentence clips; no end clap; scene lengths and SRT derive from the measured WAVs.

## 5. Sources and claims

Numbers are the supplied brief’s numbers sheet; the manuscript PDF is absent. The S07
**55 LASSO / 33 RF / 20 shared** numbers-sheet values are used; the prose 35/20/13 is
superseded. The 3D **structures** are from PubChem: phenylalanine CID 6140, sphingosine
1-phosphate CID 5283560, retinol CID 445354. They are illustrative; the 3D is not evidence
of binding, diagnosis or new biology.

Guardrails: held-out is the headline; inner-CV is optimistic; “candidate”, “proof of
concept”, “modest”; never “validated biomarker”, “detects malaria” or “AI diagnoses
malaria”; ComBat is diagnostic only; the 20:1 is feature reduction; the cohorts are West
African children with repeat visits; blood sampling is not called non-invasive.

## 6. Visual system

| Element | Decision |
| --- | --- |
| 2D palette | Cream `#F2E9D6`; ink `#151515`; green training; pink held-out; yellow annotation; orange hard shadow. |
| 3D | Ball-and-stick molecules; **Gooch shading + ink lines**; no PBR/glow/neon. 3D is the only shaded surface. |
| Type | **Archivo Black** uppercase display + **Space Grotesk** body/captions + JetBrains Mono annotations. |
| Layout | 64px margins; 120-880 content; caption 900-1000; footer 1030. |
| Motion | 3D specimen orbit, 2D stamps/checks/hard cuts; no decorative particles. |
| 3D notes | Structures are normalised to a common visual radius; S1P is embedded from its 2D SDF/SMILES; model is box-centred, not centroid-centred. |
| Geography | Orthographic Africa / Burkina Faso map in scene 5; Equal Earth Indonesia silhouette in scene 12; both are illustrative location maps, not data overlays. |
| Thumbnail | 3D phenylalanine specimen + 0.99/0.64 + “THE GAP IS THE FINDING”. |

## 7. Audio

- 34 Qwen3-TTS sentence clips; each begins with a **leading clap only**.
- No end clap. Trim starts after the clap and ends at the last recognised word.
- ffmpeg normalises to 48 kHz / -16 LUFS; the comp runs the voice at 0.75 clip gain
  because mono is dual-mono in the mix.
- Sparse paper/SFX; no music.

## 8. Assets

| Asset | Source |
| --- | --- |
| Phenylalanine SDF | PubChem CID 6140, `assets/data/malaria/phenylalanine.sdf` |
| S1P | PubChem CID 5283560; 2D SDF + `chem::smiles` / `embed3d` |
| Retinol SDF | PubChem CID 445354, `assets/data/malaria/retinol.sdf` |
| Voice | `assets/vo/malaria-sentences-norm/` |
| SRT | generated as `out/malaria-metabolomics-v2.srt` |

## 9. Stills

90 (cover 3D), 450 (stakes), 800 (3D specimens), 1150 (trap), 1500-1850 (data table + Burkina Faso map),
2050 (cleaning), 2500 (Venn), 3000 (LODO), 3300 (dot plot), 3800 (3D candidates), 4500 (demo),
4900 (limits + Indonesia map), 5350 (green close).

## 10. Risks / non-goals

- **Risks:** manuscript PDF absent; 3D structures and country maps are illustrative;
  S1P 3D embedding is approximate; voice is a clone preview.
- **Non-goals:** no diagnostic claim, no biological binding claim, no new knowledge.
- **3D is not used** for the PCA, Venn, LODO or dot plot; those stay 2D for clarity.
