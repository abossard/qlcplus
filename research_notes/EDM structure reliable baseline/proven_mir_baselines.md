# Proven MIR baselines for EDM beats, downbeats, structure and drops (as of 2026-11-04)

Scope: which methods have peer-reviewed EDM numbers, and which can be run today (code + weights). Builds on `reports/EDM audio structure analysis.md` and `research_notes/EDM audio structure analysis/structure_and_drops.md`. All repo/HF/PyPI facts below were checked directly with `gh api`, the Hugging Face API and the PyPI JSON API on 2026-11-04.

## Corrections to the earlier research

### Takeaway
Four claims in the earlier notes are wrong or incomplete: EDMFormer weights ARE public, the Raveform dataset licence IS stated (CC BY 4.0), CUE-DETR IS MIT-licensed, and the Raveform table has a "Pop+EDM" row and CMLt/AMLt columns that the earlier notes omitted.

### Cited Findings
- EDMFormer: earlier notes said "neither code nor weights are announced". Wrong. The first author's account (`25ohms`, name "Sahal Sajeer Kalandan") hosts `25ohms/EDM-98` with `data/checkpoints/model.pt` as a Git LFS object (sha256 `1412e207…e165`, 417,979,984 bytes), an inference CLI/Gradio app, and a training fork `25ohms/EDMFormer` (fork of ASLP-lab/SongFormer) — [25ohms/EDM-98](https://github.com/25ohms/EDM-98); [25ohms/EDMFormer](https://github.com/25ohms/EDMFormer)
- EDMFormer venue: arXiv comment reads "Published in CUCAI 2026 conference proceedings" (not ISMIR/ICASSP/TISMIR) — [arXiv 2603.08759](https://arxiv.org/abs/2603.08759)
- Raveform dataset licence: Hugging Face dataset card metadata is `license: cc-by-4.0` (the project page itself states none) — [HF taejunkim/raveform](https://huggingface.co/datasets/taejunkim/raveform)
- CUE-DETR: GitHub repo licence MIT; HF checkpoint `disco-eth/cue-detr` licence MIT (files `50-epoch.ckpt`, `model.safetensors`, last modified 2024-08-07) — [ETH-DISCO/cue-detr](https://github.com/ETH-DISCO/cue-detr); [HF disco-eth/cue-detr](https://huggingface.co/disco-eth/cue-detr)
- Raveform Table 4 also reports a model trained on Harmonix+Raveform ("Both") and CMLt/AMLt, see next section — [Raveform, TISMIR 9(1)](https://transactions.ismir.net/articles/288)

### Inferences
- The "no EDM-label weights exist" premise of the earlier pipeline no longer holds: an EDM-taxonomy model (EDMFormer) is runnable today, but its evidence is weak (10-track test, non-ISMIR venue), and the strong peer-reviewed model (Raveform All-In-One) is still unreleased.

### Gaps
- None beyond those listed per question.

## Q1. Raveform / All-In-One v2: weights, exact EDM results, label set, split

### Takeaway
Raveform (TISMIR 2026, peer-reviewed) has the best EDM numbers anywhere (beat F1 0.991, downbeat 0.965, HR.5F 0.835, PWF 0.847), but the trained models are still not released: `mir-aidj/raveform` contains only a LICENSE file, allin1 on PyPI is still 1.1.0 (2023), and the project page still says "v2 will be released soon". Category: **peer-reviewed, no weights** (only the annotations are released).

### Cited Findings
- Citation: Kim, T., Kim, J., Kim, H., Nam, J. "Raveform: A Dataset of Metrical and Functional Structure Annotations for EDM Tracks in DJ Mixes", TISMIR 9(1), 131–143, DOI 10.5334/tismir.288, published 2026-04-20 (accepted 2026-01-09) — [TISMIR](https://transactions.ismir.net/articles/288)
- Table 4, tested on Raveform (EDM), All-In-One architecture, 8-fold CV (asterisk = computed with label mapping) — [TISMIR](https://transactions.ismir.net/articles/288):

| Training set | Beat F1 | Beat CMLt | Beat AMLt | DB F1 | DB CMLt | DB AMLt | HR.5F | PWF | Sf |
|---|---|---|---|---|---|---|---|---|---|
| Raveform (EDM) | 0.991 | 0.985 | 0.991 | 0.965 | 0.964 | 0.971 | 0.835 | 0.847 | 0.890 |
| Harmonix (Pop) | 0.930 | 0.890 | 0.918 | 0.753 | 0.746 | 0.812 | 0.635 | 0.542* | 0.643* |
| Both (Pop+EDM) | 0.990 | 0.985 | 0.989 | 0.967 | 0.965 | 0.971 | 0.835 | 0.842 | 0.890 |
| madmom (10 datasets) | 0.947 | 0.930 | 0.938 | 0.669 | 0.678 | 0.792 | – | – | – |

- Same table, tested on Harmonix (pop): Raveform-trained 0.810 beat / 0.727 downbeat / HR.5F 0.509; Both 0.953 / 0.921 / 0.659; Harmonix-trained 0.958 / 0.915 / 0.660; madmom 0.941 / 0.805 — [TISMIR](https://transactions.ismir.net/articles/288)
- No HR3F, no frame label accuracy, no per-label (drop/buildup) scores are reported — [TISMIR](https://transactions.ismir.net/articles/288)
- Label set (9): intro, buildup, breakdown, drop, cooldown, bridge, outro, ambient-intro, ambient-outro, defined by energy/tension; bridge is rare — [TISMIR](https://transactions.ismir.net/articles/288)
- Data: 1,423 annotated tracks (3 domain experts), drawn from 4,902 DJ mixes; Techno most common (~550 tracks), then Trance, Drum & Bass, Tech House; evaluation by 8-fold CV with fold indices assigned per track in the release — [TISMIR](https://transactions.ismir.net/articles/288); [Raveform project page](https://mir-aidj.github.io/raveform/)
- Paper says dataset and models "will be publicly released"; abstract says they "are publicly available" at the project page — [TISMIR](https://transactions.ismir.net/articles/288)
- Project page: "Structure Analysis Model" links to github.com/mir-aidj/all-in-one with "(v2 will be released soon)"; "Paper (TBA)"; dataset zip at HF; audio via YouTube IDs; advises against mp3 because decoder offsets vary — [Raveform project page](https://mir-aidj.github.io/raveform/)
- `mir-aidj/raveform`: MIT, single "Initial commit" 2026-03-22, tree contains only `LICENSE`, no README, no releases — [GitHub mir-aidj/raveform](https://github.com/mir-aidj/raveform) (verified via `gh api .../git/trees/main`)
- `mir-aidj/all-in-one`: last push 2024-05-09, last commit on main 2023-10-10 ("Add CHANGELOG.md"), no GitHub releases, only branch `main`, no v2 issue or PR; open PR #39 (2026-09-20) makes NATTEN optional for CPU/MPS — [GitHub mir-aidj/all-in-one](https://github.com/mir-aidj/all-in-one)
- PyPI `allin1`: newest release 1.1.0, uploaded 2023-10-10; no 2.x — [PyPI allin1](https://pypi.org/project/allin1/)
- HF `taejunkim/allinone` (MIT): 8 `harmonix-fold*` (2023-08-06), 8 undocumented `all-fold*` (2023-10-30) and one undocumented `raveform-fold3-mrkbf2f8.pth` (1,401,804 bytes, uploaded 2023-10-14). No commits since 2023-10-30 — [HF taejunkim/allinone](https://huggingface.co/taejunkim/allinone)
- allin1 1.1.0 loader only maps `harmonix-fold0..7` and ensemble `harmonix-all` (`NAME_TO_FILE`), and config only defines `HARMONIX_LABELS`; the `raveform-*` and `all-*` files are not loadable by name — [loaders.py](https://github.com/mir-aidj/all-in-one/blob/main/src/allin1/models/loaders.py); [config.py](https://github.com/mir-aidj/all-in-one/blob/main/src/allin1/config.py)
- HF dataset `taejunkim/raveform`: `raveform.zip` + README, licence cc-by-4.0, not gated, last modified 2026-03-22 — [HF taejunkim/raveform](https://huggingface.co/datasets/taejunkim/raveform)

### Inferences
- The `raveform-fold3` checkpoint (Oct 2023) predates the TISMIR submission (June 2025) by 20 months; it is a single fold of an early model with unknown label set and unknown training data. It cannot be assumed to reproduce Table 4. Using it would need code changes (register the name, read the label vocabulary from `checkpoint['config']`) and a local evaluation. Treat as an unofficial experiment, not a baseline.
- The "Both (Pop+EDM)" row matches Raveform-only on EDM and Harmonix-only on pop. When v2 ships, that variant is likely the one to use for a mixed library.
- Because annotations are CC BY 4.0 with per-track fold indices, re-training the public allin1 training code (`Add training code`, PR #5) on Raveform is the closest reproducible path to the 0.835 number, subject to YouTube audio availability.

### Gaps
- Raveform zip contents (exact fold file name, number of tracks with surviving YouTube audio) not inspected; downloading was out of scope.
- Label decoding inside `raveform-fold3-mrkbf2f8.pth` not inspected (would require downloading the checkpoint).
- No release date for allin1 v2 exists in any official source.

## Q2. EDM-CUE / CUE-DETR (ISMIR 2024, ETH)

### Takeaway
CUE-DETR is **peer-reviewed + released weights** (MIT code and checkpoint), but it predicts unlabelled DJ cue points, not sections or drops. Best F1 is 0.36–0.46 at a one-beat tolerance on a 101-track test set, so it is a weak boundary proposer, useful mainly for the 8/16-bar phrase prior.

### Cited Findings
- Venue: ISMIR 2024 poster (Argüello, Lanzendörfer, Wattenhofer, ETH Zurich) — [ISMIR 2024 program](https://ismir2024program.ismir.net/poster_19.html); repo description "Repository of the ISMIR'24 paper" — [GitHub](https://github.com/ETH-DISCO/cue-detr)
- Task: DETR object detection on spectrogram images; single category "cue"; cue points "indicate possible temporal boundaries in a transition"; "frequently align with high-level structural boundaries and tend to strongly coincide with phrase boundaries"; drops not discussed — [CUE-DETR arXiv](https://arxiv.org/html/2407.06823v1)
- EDM-CUE: 4,710 tracks, ~380 h, 21,461 cues (4.6/track), metadata only (Deezer-style IDs, rekordbox beat grid), MIT — [CUE-DETR arXiv](https://arxiv.org/html/2407.06823v1); [HF disco-eth/edm-cue](https://huggingface.co/datasets/disco-eth/edm-cue)
- Test: 101 held-out tracks, 607 annotations; tolerances T1 = one beat, T1/2 = half beat (~172 ms) — [CUE-DETR arXiv](https://arxiv.org/html/2407.06823v1)
- Results F1 (cues-only / 16-bar / 8-bar ground truth): T1 CUE-DETR r8 0.39 / 0.46 / 0.36, r16 0.36 / 0.38 / 0.26; Mixed In Key 0.22 / 0.16 / 0.12; Automix 0.13 / 0.15 / 0.11. T1/2 CUE-DETR r8 0.27 / 0.32 / 0.25 — [CUE-DETR arXiv](https://arxiv.org/html/2407.06823v1)
- Code: `model/cue_detr_{train,eval,pred}.py`, demo `cue_points.py` defaulting to the HF checkpoint (last commit 2024-08-19); no releases — [GitHub ETH-DISCO/cue-detr](https://github.com/ETH-DISCO/cue-detr)

### Inferences
- Output can serve as boundary candidates only after snapping to a downbeat grid; no label, no drop/buildup distinction. At F1 < 0.5 it is clearly weaker than any structure model's HR.5F on EDM.

### Gaps
- No evaluation of CUE-DETR against section boundaries (HR.5F) exists.

## Q3. Beat This!, madmom, BeatNet on EDM; downbeats

### Takeaway
No peer-reviewed paper reports Beat This! or BeatNet on an EDM structure set (Raveform, GiantSteps). The only EDM-ish Beat This! number is HJDB (hardcore/jungle/DnB) at 98.2 beat / 96.6 downbeat F1, but HJDB is in its training data (8-fold CV). madmom on Raveform is the only generic-tracker EDM downbeat number: 0.669. Beat This! is **peer-reviewed + released weights (MIT)**.

### Cited Findings
- Beat This! accepted at ISMIR 2024 (arXiv comment) — [arXiv 2407.21658](https://arxiv.org/abs/2407.21658)
- Training data includes HJDB ("One in the jungle: Downbeat detection in hardcore, jungle, and drum and bass"); GiantSteps not mentioned; GTZAN is the only held-out test set (beat 89.1, downbeat 78.3) — [Beat This! paper](https://arxiv.org/html/2407.21658v1)
- Table 1 (8-fold CV): HJDB 98.2 / 96.6; Groove MIDI 93.7 / 82.1 — [Beat This! paper](https://arxiv.org/html/2407.21658v1)
- Metrics: mir_eval with default parameters (F1 ±70 ms, CMLt, AMLt), default trims the first 5 s — [Beat This! paper](https://arxiv.org/html/2407.21658v1)
- Availability: `pip install beat-this`, v1.1.0 released 2026-04-14 (PyPI publishing added); commit 2026-05-28 states "The code and the published model weights are released under the MIT license"; CPU fallback via `--gpu=-1`; MPS not mentioned; `--dbn` needs madmom from git because PyPI madmom supports only Python<3.10 / numpy<1.20 — [GitHub CPJKU/beat_this](https://github.com/CPJKU/beat_this)
- madmom on Raveform: beat 0.947, downbeat 0.669 (CMLt 0.678, AMLt 0.792) — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- madmom: last PyPI-era release v0.16.1 (2018); main last commit 2024-08-25 — [GitHub CPJKU/madmom](https://github.com/CPJKU/madmom)
- BeatNet (ISMIR 2021): weights trained on GTZAN, Ballroom, Rock Corpus included in repo; CC-BY-4.0; no EDM evaluation in README — [GitHub mjhydri/BeatNet](https://github.com/mjhydri/BeatNet)

### Inferences
- Downbeat AMLt (0.792 madmom, 0.812 Harmonix-trained All-In-One) is much higher than downbeat F1 on Raveform, which suggests many generic downbeat errors are metrical-phase/level errors rather than random. That is the error a track-wide phase vote fixes, but the size of that gain is unmeasured.
- Beat This! on Raveform is the obvious missing measurement; it can be run locally once Raveform audio is obtained.

### Gaps
- No published Beat This! / BeatNet number on Raveform, GiantSteps tempo or any EDM-only held-out set.
- GiantSteps has tempo/key labels only; no beat-tracker paper found evaluating against it for beats.

## Q4. allin1 original (WASPAA 2023): Harmonix results and EDM evaluation

### Takeaway
allin1 (Harmonix) is **peer-reviewed + released weights (MIT)**, and the only published EDM evaluation of its configuration is Raveform's "Harmonix (Pop)" row: beat 0.930, downbeat 0.753, HR.5F 0.635, PWF 0.542 (with label mapping).

### Cited Findings
- Accepted at IEEE WASPAA 2023 (arXiv comment) — [arXiv 2307.16425](https://arxiv.org/abs/2307.16425)
- Harmonix 8-fold CV: beat F1 0.958 (CMLt 0.913, AMLt 0.964), downbeat 0.915 (0.873, 0.932), HR.5F 0.660, PWF 0.738, Sf 0.769 (reproduced in Raveform Table 4) — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- Harmonix-trained All-In-One on Raveform: 0.930 / 0.753 / 0.635 / 0.542* / 0.643* — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- Weights: `harmonix-fold0..7` on HF, MIT; PyPI 1.1.0 (2023-10-10) — [HF taejunkim/allinone](https://huggingface.co/taejunkim/allinone); [PyPI allin1](https://pypi.org/project/allin1/)

### Inferences
- The Raveform row is measured with the same architecture trained on Harmonix by the same group; it is a fair proxy for `allin1 --model harmonix-foldN`, but whether it used the public checkpoints or retrained folds is not stated.

### Gaps
- Whether Raveform's Harmonix row equals the released `harmonix-all` ensemble is not stated.

## Q5. SongFormer and EDMFormer

### Takeaway
SongFormer is **preprint only** (README cites it as arXiv `@misc`) but has released weights (CC-BY-4.0 code; depends on MuQ weights, CC-BY-NC-4.0). EDMFormer is published in CUCAI 2026 proceedings (an undergraduate AI conference, not an MIR venue; review rigour not verified) with released weights; treat it as **preprint-grade evidence with weights**. Its numbers come from a 10-track test set.

### Cited Findings
- SongFormer arXiv 2510.02797, v3 2026-04-08, no journal-ref/comment; README citation is `@misc{hao2026songformer…, eprint=2510.02797}` (updated 2026-05-14) — [arXiv](https://arxiv.org/abs/2510.02797); [GitHub ASLP-lab/SongFormer](https://github.com/ASLP-lab/SongFormer)
- SongFormer code licence CC-BY-4.0 (LICENSE file); HF `ASLP-lab/SongFormer` has `SongFormer.safetensors`, `model.safetensors`, `SongFormer.pt` (no licence in card metadata) — [GitHub](https://github.com/ASLP-lab/SongFormer); [HF ASLP-lab/SongFormer](https://huggingface.co/ASLP-lab/SongFormer)
- SongFormer depends on MuQ (`OpenMuQ/MuQ-large-msd-iter`, licence cc-by-nc-4.0) and MusicFM (`minzwon/MusicFM`, MIT) — [HF OpenMuQ](https://huggingface.co/OpenMuQ/MuQ-large-msd-iter); [HF MusicFM](https://huggingface.co/minzwon/MusicFM)
- SongFormer numbers (SongFormBench-Harmonix): ACC 0.795, HR.5F 0.703, HR3F 0.784; no EDM evaluation by its authors — [SongFormer paper](https://arxiv.org/html/2510.02797v2)
- EDMFormer: Sajeer, Patel, Chung, Bae (Waterloo); "Published in CUCAI 2026 conference proceedings"; EDM-98 (98 tracks, 88/10 split); results SongFormer zero-shot HR@0.5 0.569 / HR@3 0.608 / ACC 0.148 vs EDMFormer 0.616 / 0.635 / 0.883 — [arXiv 2603.08759](https://arxiv.org/abs/2603.08759); [EDMFormer HTML](https://arxiv.org/html/2603.08759v1)
- EDMFormer weights and tools: `25ohms/EDM-98` repo (LICENSE CC BY 4.0; packaged metadata MIT), PyPI `edm98` 0.1.0 (CC-BY-4.0) for labels/splits, inference via repo clone; checkpoint `data/checkpoints/model.pt` (~418 MB, LFS); labels intro, outro, silence, breakdown, buildup, drop (dataset id 9); audio not distributed, keyed by Deezer IDs — [GitHub 25ohms/EDM-98](https://github.com/25ohms/EDM-98); [PyPI edm98](https://pypi.org/project/edm98/)
- A third-party ggml/C++ port (`key2/edmformer.cpp`, Aug 2026, no licence) runs both heads on Metal/CPU and documents the checkpoint provenance — [GitHub key2/edmformer.cpp](https://github.com/key2/edmformer.cpp)

### Inferences
- EDMFormer is the only runnable model today that outputs buildup/drop/breakdown labels, but HR@0.5 0.616 on 10 tracks has wide uncertainty and is below Raveform's 0.835. MuQ's NC licence carries over to any SongFormer/EDMFormer deployment.
- EDMFormer's taxonomy lacks cooldown/bridge/ambient-intro/outro; mapping to Raveform labels for a common benchmark loses those classes.

### Gaps
- CUCAI peer-review process not verified.
- No independent evaluation of the EDMFormer checkpoint (e.g. on Raveform) exists.

## Q6. Drop / buildup detection with numbers and code

### Takeaway
The only peer-reviewed drop detectors are from the Yadati/Larson/Liem/Hanjalic line (ISMIR 2014; IEEE TMM 2018), with coarse tolerances (F1 0.71 at 15 s, >0.6 at 3 s in 2014) and no released code found. No ISMIR 2023–2025 paper on EDM drop detection was found. In practice, drop/buildup labels from Raveform (unreleased) or EDMFormer (released) section models supersede them.

### Cited Findings
- Yadati et al., ISMIR 2014: 100 SoundCloud tracks, F1 0.71 at 15 s, >0.6 at 3 s; candidates mean 2.5 s from true drop — [ISMIR 2014 PDF](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- Follow-up: "Detecting Socially Significant Music Events using Temporally Noisy Labels", IEEE Transactions on Multimedia 2018 (DOI 10.1109/TMM.2018.2801719), covering drop, build and break events with SoundCloud timed comments as noisy labels — [TU Delft record](https://repository.tudelft.nl/record/uuid:df31b873-e0d8-4a33-aac1-e098f133f04e)
- MediaEval 2014 workshop paper on multimodal drop detection (workshop, not a full peer-reviewed paper) — [CEUR-WS Vol-1263](https://ceur-ws.org/Vol-1263/mediaeval2014_submission_88.pdf)
- felixmeyer6/drop-detector (GPL-3.0) claims F1 0.95 with no dataset size or tolerance; not peer-reviewed — [GitHub](https://github.com/felixmeyer6/drop-detector)

### Inferences
- Raveform's drop/buildup labels are energy-defined, so a section model's drop-segment starts give a drop-onset detector that can be scored at beat-level tolerances, much tighter than Yadati's.

### Gaps
- TMM 2018 numbers not retrieved (TU Delft repository refused connection).
- No public code for either Yadati paper found.

## Q7. Evaluation protocol for a comparable local benchmark

### Takeaway
Use mir_eval with Raveform's metric set: beat and downbeat F1 (±70 ms), CMLt, AMLt; boundary HR.5F (and HR3F for SongFormer/EDMFormer comparability); PWF; Sf (NCE). Report frame label ACC only when comparing with EDMFormer/SongFormer. Add CUE-style beat-relative tolerances for drops.

### Cited Findings
- Raveform: beat/downbeat F1 ±70 ms, CMLt ±17.5% of beat period, AMLt (Davies et al. 2009); HR.5F, PWF, Sf (Nieto et al. 2020); mir_eval not named; Harmonix rows by 8-fold CV — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- Beat This!: mir_eval defaults, first 5 s trimmed — [Beat This! paper](https://arxiv.org/html/2407.21658v1)
- SongFormer/EDMFormer: HR.5F/HR@0.5, HR3F/HR@3, frame ACC — [SongFormer](https://arxiv.org/html/2510.02797v2); [EDMFormer](https://arxiv.org/html/2603.08759v1)
- CUE-DETR: P/R/F1 at one beat and half a beat (~172 ms) — [CUE-DETR](https://arxiv.org/html/2407.06823v1)
- Yadati 2014: drop F1 at 3–15 s windows — [ISMIR 2014 PDF](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- mir_eval functions: `beat.f_measure` (default window 0.07 s), `beat.continuity` (CMLt/AMLt), `segment.detection(window=0.5|3)`, `segment.pairwise`, `segment.nce` — [mir_eval beat](https://mir-eval.readthedocs.io/latest/api/beat.html); [mir_eval segment](https://mir-eval.readthedocs.io/latest/api/segment.html)

### Inferences
- Mapping to mir_eval: Raveform beat metrics = `mir_eval.beat.evaluate` defaults (which also trim 5 s, matching Beat This!); downbeats are scored with the same functions on downbeat times; Sf = `segment.nce` F-measure.
- Label metrics (PWF, Sf) on Raveform for non-EDM models need an explicit label mapping (Raveform marks those with *); publish the mapping with local results.

### Gaps
- Whether Raveform/allin1 used `trim=True` for HR.5F and the 5 s beat trim is not stated in the fetched text.

## Summary table: what can be run today

| Method | Venue status | Weights public | Licence | EDM evidence (test set) |
|---|---|---|---|---|
| Beat This! `final0` | ISMIR 2024 | yes (pip `beat-this` 1.1.0) | MIT code + weights | HJDB 98.2/96.6 (in training, 8-fold CV); none on Raveform |
| allin1 `harmonix-all` | WASPAA 2023 | yes (HF, pip 1.1.0) | MIT | Raveform 8-fold: 0.930 / 0.753 / HR.5F 0.635 / PWF 0.542* |
| madmom | (multiple, older) | yes | not checked (GitHub reports NOASSERTION) | Raveform: 0.947 / 0.669 |
| CUE-DETR | ISMIR 2024 | yes (HF) | MIT | EDM-CUE 101-track test: cue F1 0.36–0.46 @1 beat |
| BeatNet | ISMIR 2021 | yes (repo) | CC-BY-4.0 | none |
| Raveform All-In-One | TISMIR 2026 | **no** (v2 "soon") | n/a (annotations CC BY 4.0) | Raveform 8-fold: 0.991 / 0.965 / 0.835 / PWF 0.847 |
| raveform-fold3 (HF, 2023) | none (undocumented) | file exists, not wired | MIT (repo) | unknown |
| SongFormer | preprint | yes | CC-BY-4.0 code; MuQ CC-BY-NC-4.0 | EDM-98 zero-shot: HR@0.5 0.569, ACC 0.148 (10 tracks) |
| EDMFormer | CUCAI 2026 (undergrad conf.) | yes (LFS, 418 MB) | CC BY 4.0 repo; MuQ CC-BY-NC-4.0 | EDM-98: HR@0.5 0.616, HR@3 0.635, ACC 0.883 (10 tracks) |
| Yadati drop detector | ISMIR 2014 / TMM 2018 | no code found | n/a | 100 tracks: F1 0.71 @15 s, >0.6 @3 s |
