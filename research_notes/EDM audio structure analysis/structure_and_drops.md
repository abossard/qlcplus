# EDM structure, drop and energy detection: state of the art (as of Nov 2026)

## 1. Which open-source models/tools give the best structure segmentation + labeling today?

### Takeaway
For pop-style benchmarks, SongFormer (Oct 2025, MuQ+MusicFM SSL features) is the strongest released model (HR.5F ~0.70 on Harmonix). For EDM specifically, the best signal comes from EDM-native work: Raveform (Kim et al., TISMIR 2026, an All-In-One-based model trained on 1,423 EDM tracks, HR.5F 0.835 on EDM) and EDMFormer (Mar 2026 preprint, SongFormer retrained on 98 EDM tracks, label ACC 0.883 vs 0.148 zero-shot). allin1 (2023) remains the only mature, pip-installable package that outputs beats, downbeats, segments and labels in one JSON, but its label set is pop (verse/chorus/break) and it degrades on EDM.

### Cited Findings
**All-In-One / allin1 (Kim & Nam, WASPAA 2023)**
- Predicts BPM, beats, downbeats, functional segment boundaries and labels; trained on Harmonix with 8-fold CV; default `harmonix-all` is an 8-model ensemble — [allin1 GitHub](https://github.com/mir-aidj/all-in-one)
- Label set: start, end, intro, outro, break, bridge, inst, solo, verse, chorus (no buildup/drop) — [allin1 GitHub](https://github.com/mir-aidj/all-in-one)
- Paper results on Harmonix (8-fold CV, 300K params): beat F1 .958, downbeat F1 .915, HR.5F .660, PWF .738, Sf .769. Baselines: SpecTNT HR.5F .558, DSF+Scluster .497, TCN .619. Uses Demucs-demixed 4-stem input + dilated neighborhood attention; demixing helps segmentation — [allin1 paper, arXiv 2307.16425](https://arxiv.org/pdf/2307.16425v1.pdf)
- Re-run by SongFormer authors on SongFormBench-Harmonix (200 songs): ACC 0.740, HR.5F 0.596, HR3F 0.730 — [SongFormer paper](https://arxiv.org/html/2510.02797v2)
- MIT license; JSON output with `bpm`, `beats`, `downbeats`, `beat_positions`, `segments`; optional activations/embeddings; warns MP3 decoders add ~20-40 ms offset, WAV recommended for timing — [allin1 GitHub](https://github.com/mir-aidj/all-in-one)
- Runtime: `harmonix-all` processed 10 songs (33 min audio) in 73 s on an RTX 4090 — [allin1 GitHub](https://github.com/mir-aidj/all-in-one)

**SongFormer (Hao, Yuan et al., arXiv 2510.02797, Oct 2025, v3 Apr 2026)**
- Fuses 30 s and 420 s window features from MuQ and MusicFM; 4-layer Transformer backend (hidden 512), features at ~8.33 Hz; songs truncated to 420 s — [SongFormer paper](https://arxiv.org/html/2510.02797v2)
- SongFormBench-Harmonix: SongFormer (HX) ACC 0.795 / HR.5F 0.703 / HR3F 0.784; with extra data ACC 0.807 / HR.5F 0.696. Comparisons: LinkSeg-7Labels 0.780/0.630/0.762; TA (Zhang et al. 2025) 0.787*/0.610*/0.801*; MusicFM 0.725*/0.640*/0.729*; MERT (5 s) 0.574*/0.626*; SpecTNT 0.701-0.723*/0.558-0.570*; Gemini 2.5 Pro 0.748/0.423/0.813 (* = copied from original papers, not re-run) — [SongFormer paper](https://arxiv.org/html/2510.02797v2)
- Training data (SongFormDB, >10k tracks) is described as multilingual/broad; EDM is not mentioned — [SongFormer paper](https://arxiv.org/html/2510.02797v2)
- Code license CC-BY-4.0; Python 3.10; tested on Ubuntu 22.04; weights on Hugging Face (ASLP-lab/SongFormer); "2-4 seconds" per song on an NVIDIA L40; CPU/MPS support not documented; output is "start_time label" text; labels intro, verse, chorus, bridge, inst, outro, silence, pre-chorus — [SongFormer GitHub](https://github.com/ASLP-lab/SongFormer)

**EDMFormer (Sajeer, Patel, Chung, Bae, Univ. of Waterloo, arXiv 2603.08759, Mar 2026, preprint)**
- Same MuQ+MusicFM (30 s / 420 s) feature fusion into the SongFormer encoder, trained on EDM-98 with EDM taxonomy: intro, build-up, drop, breakdown, outro, silence, end — [EDMFormer paper](https://arxiv.org/html/2603.08759v1)
- EDM-98: 98 tracks drawn from EDM-CUE, ±0.5 s annotations; genres house/deep house 19.4%, techno, trance/progressive, dubstep/bass, DnB, electro 14.3% each, chill 9.2%; 88/10 train/test split — [EDMFormer paper](https://arxiv.org/html/2603.08759v1)
- Results (only table): SongFormer zero-shot (pop taxonomy) HR@0.5 0.569 / HR@3 0.608 / ACC 0.148; EDMFormer 0.616 / 0.635 / 0.883 — [EDMFormer paper](https://arxiv.org/html/2603.08759v1)
- Paper text inconsistent (two annotators vs. single annotator); code/weights availability not stated; test set is only 10 tracks — [EDMFormer paper](https://arxiv.org/html/2603.08759v1)

**Raveform (Taejun Kim, Jongsoo Kim, Hyungyu Kim, Juhan Nam; TISMIR vol. 9, 2026)**
- EDM-specific, energy/tension-defined vocabulary: intro, buildup, breakdown, drop, cooldown, bridge, outro, ambient-intro, ambient-outro — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- 4,902 DJ mixes (~6,522 h), 56,873 unique tracks; 1,423 tracks manually annotated (beats, downbeats, boundaries, labels) — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- All-In-One-based model, 8-fold CV tested on Raveform: trained on Raveform beat F1 0.991, downbeat F1 0.965, HR.5F 0.835, PWF 0.847, Sf 0.890; trained on Harmonix only: 0.930 / 0.753 / 0.635 / 0.542 / 0.643; madmom beat 0.947, downbeat 0.669 — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- Release: annotations zip on Hugging Face (taejunkim/raveform), audio referenced by YouTube ID, code at github.com/mir-aidj/raveform; structure model linked to all-in-one with "v2 will be released soon"; license not stated — [Raveform project page](https://mir-aidj.github.io/raveform/)

**Foundation-model probes**
- "Do Foundational Audio Encoders Understand Music Structure?" (Sony, Dec 2025): tested 11 encoders; masked-language-model SSL on music data is "particularly effective" for MSA — [arXiv 2512.17209](https://arxiv.org/abs/2512.17209v1)
- Layer-wise MusicFM vs MuQ analysis including a Harmonix frame-level structure task (May 2025) — [arXiv 2505.16306](https://arxiv.org/abs/2505.16306)
- SongFormer notes MuQ/MusicFM were pretrained on 30 s windows, so naive longer inference hurts — [SongFormer paper](https://arxiv.org/html/2510.02797)

**MSAF (classic algorithms)**
- MIT license, 1,375 commits, 18 open issues, no GitHub releases listed; README gives no benchmarks — [MSAF GitHub](https://github.com/urinieto/msaf)
- Classic SSM approach (DSF+Scluster) scores HR.5F .497 on Harmonix vs .660 for allin1 — [allin1 paper](https://arxiv.org/pdf/2307.16425v1.pdf)

**Commercial references**
- Rekordbox uses two phrase label sets: electronic (Intro, Up, Down, Chorus, Outro) and pop (Intro, Verse, Chorus, Bridge, Outro); auto-decides genre, user-correctable; Up = build-up, Down = breakdown — [Disc DJ Store phrase guide](https://thediscdjstore.com/blog/phrase-analysis.html)
- Phrases are user-editable (move, delete, combine) — [rekordbox Phrase Analysis manual](https://manuals.plus/rekordbox/alphatheta-phrase-analysis-manual)
- No published accuracy figures; forum users call phrase analysis error-prone — [VirtualDJ forum](https://virtualdj.com/forums/257055/Wishes_and_new_features/Phrase_Analysis.html)
- Mixed In Key Energy Level is a 1-10 per-track score; algorithm unpublished (patented); vendor advises users to trust their ears — [Mixed In Key workflow](https://mixedinkey.com/workflows/use-energy-level-detection/); [US8865993](https://patents.google.com/patent/US8865993)
- 2012 press describes MIK energy detection as listening for hi-hat patterns and white noise (journalistic, not vendor doc) — [DJ TechTools](https://djtechtools.com/2012/10/31/mixed-in-key-beta-5-5-detects-energy-supports-flac)

### Inferences
- Ranking for an EDM offline tool: (1) Raveform-trained All-In-One v2 once weights land (best EDM numbers, same JSON as allin1, beats+downbeats included); (2) fine-tune SongFormer/EDMFormer-style head on Raveform's 1,423 annotations; (3) allin1 `harmonix-all` + label remapping (break->breakdown, chorus->drop, inst->drop/build by energy) as an immediately usable baseline.
- Harmonix-trained label output is nearly useless for EDM taxonomy (SongFormer zero-shot ACC 0.148 on EDM-98; Harmonix-trained PWF 0.542 on Raveform) while boundaries transfer moderately (HR.5F ~0.57-0.64).
- Raveform HR.5F 0.835 vs Harmonix-pop ~0.70 suggests EDM boundaries are easier once trained in-domain, because they sit on phrase-grid downbeats.

### Gaps
- No public allin1 v2 / Raveform weights confirmed as of the project page; check github.com/mir-aidj/all-in-one before building.
- SpecTNT has no public code/weights found; only numbers via other papers.
- No independent accuracy data for Rekordbox phrases, Serato or VirtualDJ; VirtualDJ phrase analysis appears only as a user feature wish (not confirmed either way).
- MIREX structural segmentation results not checked (likely stale, pre-2020).

## 2. EDM-specific drop and buildup detection: what features and heuristics work?

### Takeaway
Drops are best found as downbeat-aligned phrase boundaries where bass/kick energy returns after a low-energy buildup; buildups by rising brightness, rhythmic density (snare rolls) and missing low end. Classic feature+classifier systems reach F1 ~0.6-0.71 (Yadati 2014, coarse tolerance); a 2026 hobby XGBoost system claims 0.95 (unverified). Learned EDM-taxonomy models (Raveform, EDMFormer) now subsume most of this.

### Cited Findings
- Yadati, Larson, Liem, Hanjalic (ISMIR 2014, older): two-stage pipeline. Chroma-based unsupervised segmentation (12 pitch classes, 209 ms window, 139 ms hop) generates drop candidates (mean 2.5 s from true drop, <8% missed), then a classifier on spectrogram, 13 MFCCs and rhythm patterns/histograms around each boundary. Dataset: 100 SoundCloud tracks (dubstep, electro, house), 60/20/20 split; drop defined as "where the buildup ends and the bassline is re-introduced". F1 0.71 at 15 s tolerance, >0.6 at 3 s; average segment length 16.5 s — [Yadati et al. 2014 PDF](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- felixmeyer6/drop-detector (GPL-3.0, 2026): candidates via bass-boost low-shelf filter + volume-envelope rise + transient snapping; 29 context features comparing post-candidate audio vs preceding buildup (RMS, future energy, 4/8/16/32-bar grid alignment, pulse clarity, transient dominance, bass ratio, bass continuity); XGBoost + Optuna; claims F1 0.95 vs Yadati 0.71; dataset size and tolerance not stated — [drop-detector GitHub](https://github.com/felixmeyer6/drop-detector)
- EDMFormer cites switch-point studies: energy novelty, drum onset counts, spectral flux and timbral features are best cues; buildup/breakdown characterized by "spectral brightness, rhythmic density, and low-frequency energy changes" rather than chords/vocals; buildup = rising drum patterns or risers; drop = peak energy with main rhythm motif and bassline — [EDMFormer paper](https://arxiv.org/html/2603.08759v1)
- Zehren et al. (2020/2022): EDM is "highly regular both metrically and formally"; switch points always on the downbeat at the start of a period; rule set derived from DJ interviews — [Zehren et al., arXiv 2007.08411](https://arxiv.org/pdf/2007.08411)
- CUE-DETR (Argüello, Lanzendörfer, Wattenhofer, ETH, 2024): DETR object detector on Mel-spectrogram images; EDM-CUE = 4,710 tracks, 21,461 expert cue points, 95-190 BPM; 16-bar phrases most common, then 8-bar. At 1-beat tolerance, F1 0.36-0.46 (CUE-DETR) vs Mixed In Key 10 0.16-0.22 vs rule-based Automix 0.13-0.15. Code/checkpoints at github.com/ETH-DISCO/cue-detr — [CUE-DETR paper](https://arxiv.org/html/2407.06823v1)
- EDM-CUE metadata (tempo, beats, downbeats, cues) is MIT-licensed on Hugging Face; audio is not distributed — [EDM-CUE dataset card](https://huggingface.co/datasets/disco-eth/edm-cue/blob/main/README.md)
- Schwarz & Fourer's UnmixDB uses auto-generated mixes; André, Fourer, Schwarz (2024) propose multi-pass NMF for DJ-mix transcription/unmixing (mix-level, not track structure) — [Raveform, TISMIR](https://transactions.ismir.net/articles/288); [arXiv 2410.04198](https://arxiv.org/pdf/2410.04198v1)

### Inferences
- Practical heuristic stack for a rule layer on top of a model: snap candidate boundaries to downbeats, prefer 8/16/32-bar phrase positions from the first downbeat; label drop where low-band (<150 Hz) RMS and kick onset density jump after a segment with low low-band energy and rising spectral centroid/onset density; label breakdown where kick density falls for >=8 bars.
- Demucs drum/bass stems (already computed inside allin1) give cleaner kick and bass presence curves than full-mix filtering.
- Yadati-era numbers use seconds-wide tolerances and are not comparable to HR.5F.

### Gaps
- No dedicated published riser/snare-roll/filter-sweep detector with benchmark numbers found.
- drop-detector's 0.95 F1 lacks dataset size/tolerance; treat as unverified.
- Exact Schwarz/Fourer 2019 UnmixDB paper not fetched.

## 3. Beat/downbeat tracking quality on EDM and getting a bar/phrase grid

### Takeaway
Beat tracking on EDM is near-solved (F1 0.95-0.99); downbeats are the weak point for generic models (madmom 0.669, Harmonix-trained All-In-One 0.753 on Raveform EDM) and become reliable (0.965) only with EDM training data. Beat This! (ISMIR 2024) is the best maintained general tracker; allin1 gives beats/downbeats for free with segments.

### Cited Findings
- Beat This! (Foscarin, Schlüter, Widmer, ISMIR 2024): no DBN postprocessing; Harmonix beat F1 95.8 / downbeat 90.7 (vs Hung et al. 95.3 / 90.8); Ballroom 97.5 / 95.3; GTZAN 89.1 / 78.3 (with DBN 88.1 / 77.4); ~20M params (small ~2M); training data includes HJDB (hardcore/jungle/DnB) but no dataset described as electronic dance music generally — [Beat This! paper](https://arxiv.org/html/2407.21658v1)
- Beat This! package: MIT code and weights, `pip install beat-this`, PyTorch >= 2.0, CPU fallback (`--gpu=-1`), MPS not mentioned; models ~78 MB (small ~8.1 MB); optional `--dbn` needs madmom from GitHub because PyPI madmom doesn't support current Python/numpy — [beat_this GitHub](https://github.com/CPJKU/beat_this)
- allin1 (Harmonix): beat F1 .958, downbeat .915 — [allin1 paper](https://arxiv.org/pdf/2307.16425v1.pdf)
- On EDM (Raveform test): Raveform-trained model beat 0.991 / downbeat 0.965; Harmonix-trained 0.930 / 0.753; madmom 0.947 / 0.669 — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- Raveform annotators relied on Rekordbox beat grids, which became unstable with tempo changes or irregular rhythm; downbeats sometimes offset and needed manual correction — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- 16-bar phrasing dominates EDM, then 8-bar; phrase-aligned evaluation raises cue precision — [CUE-DETR paper](https://arxiv.org/html/2407.06823v1)

### Inferences
- EDM is nearly constant-tempo, so fit a single linear tempo grid (or piecewise for tempo changes) to tracker beats, then pick the downbeat phase by voting across the track; phrase grid = every 16 bars from the downbeat phase that best aligns with detected segment boundaries / drop.
- If Raveform-trained weights are not available, use Beat This! for beats+downbeats and correct downbeat phase with a kick-energy heuristic.

### Gaps
- BeatNet not evaluated in fetched sources; no EDM-specific comparison for it.
- No published head-to-head of Beat This! on an EDM-only set.

## 4. Energy, tension and atmosphere/scene-change detection

### Takeaway
There is no strong published, benchmarked "tension curve" model for EDM. Practical options are hand-built curves (RMS, low-band energy, onset density, spectral centroid/flux, novelty from SSL embeddings) and Essentia's arousal/valence regressors (CC BY-NC-SA). Commercial "energy" (Mixed In Key) is per-track, opaque and unvalidated.

### Cited Findings
- Raveform defines all section labels by energy and tension, i.e. the label sequence itself is a coarse tension curve (cooldown, ambient-intro/outro distinguish low-energy atmosphere sections) — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- Energy novelty, drum onset counts and timbral features are the strongest EDM transition cues — [EDMFormer paper](https://arxiv.org/html/2603.08759v1)
- Essentia/MTG arousal-valence models: single-hidden-layer MLPs on MusiCNN or VGGish embeddings, trained on EmoMusic, DEAM, MuSe; evaluated by pairwise ranking accuracy; CC BY-NC-SA (commercial license available) — [Replicate mtg/music-arousal-valence](https://replicate.com/mtg/music-arousal-valence/readme); [MusAV ISMIR 2022 poster](https://ismir2022program.ismir.net/static/posters/286.pdf)
- allin1 can export frame-level activations and embeddings (`--activ`, `--embed`) usable for novelty/self-similarity — [allin1 GitHub](https://github.com/mir-aidj/all-in-one)
- Mixed In Key Energy Level: 1-10 per track, not tempo-linked, method unpublished — [DJ.Studio help](https://help.dj.studio/en/articles/8119977-mixed-in-key-integration); [Mixed In Key](https://mixedinkey.com/workflows/use-energy-level-detection/)

### Inferences
- A defensible continuous curve: per-beat z-scored mix of low-band RMS, total loudness, onset/kick density, spectral centroid, smoothed over 1-2 bars; "tension" = positive slope of centroid + onset density while low-band energy is low (buildup); scene/atmosphere change = peaks of a checkerboard novelty on MuQ/MusicFM or allin1 embeddings at the 8-16 bar scale.
- Running Essentia arousal on sliding windows (e.g. 4-8 bars) is feasible but its training data is not EDM and per-window behavior is unvalidated.

### Gaps
- No benchmark found for within-track mood/scene-change detection in EDM.
- Essentia model frame/segment rate not confirmed from fetched sources.

## 5. Realistic accuracy and known failure modes on EDM

### Takeaway
On Harmonix (pop-heavy), best released systems reach HR.5F ~0.66-0.70, HR3F ~0.73-0.80, label ACC ~0.74-0.81. On EDM with an EDM taxonomy: in-domain training gives HR.5F 0.835 (Raveform) or HR@0.5 0.616 / ACC 0.883 (EDMFormer, 10-track test); pop-trained models give near-random EDM labels.

### Cited Findings
- Harmonix: allin1 HR.5F .660 (own 8-fold) / 0.596 HR.5F, 0.730 HR3F, 0.740 ACC (SongFormBench re-run); SongFormer HR.5F 0.703, HR3F 0.784, ACC 0.795-0.807; Gemini 2.5 Pro HR.5F 0.423 but HR3F 0.813 (coarse timing) — [allin1 paper](https://arxiv.org/pdf/2307.16425v1.pdf); [SongFormer paper](https://arxiv.org/html/2510.02797v2)
- EDM: SongFormer zero-shot ACC 0.148, "systematically misses" drop onsets and buildup peaks — [EDMFormer paper](https://arxiv.org/html/2603.08759v1)
- EDM: Harmonix-trained All-In-One on Raveform downbeat 0.753, HR.5F 0.635, PWF 0.542 vs Raveform-trained 0.965 / 0.835 / 0.847 — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- Failure modes: pop taxonomies and harmonic/lyric repetition cues don't fit energy-driven EDM — [EDMFormer paper](https://arxiv.org/html/2603.08759v1); gradual fade-outs make cooldown/outro and breakdown onsets ambiguous; tempo changes destabilize beat grids; labels subjective — [Raveform, TISMIR](https://transactions.ismir.net/articles/288)
- Harmonix label inaccuracies reported by users (open issue #34) — [allin1 issues](https://github.com/mir-aidj/all-in-one/issues)
- Adding noisy extra training data slightly lowers strict HR.5F (0.703 -> 0.696) — [SongFormer paper](https://arxiv.org/html/2510.02797v2)

### Inferences
- Expect allin1 out-of-the-box on EDM: boundaries within 0.5 s roughly 60% of the time, labels needing remap; snapping to a 16-bar phrase grid should recover much of the remaining boundary error since EDM boundaries are downbeat/phrase aligned.
- EDMFormer's numbers are a small-sample preprint; Raveform (peer-reviewed, 1,423 tracks, 8-fold CV) is the most reliable EDM benchmark.

### Gaps
- No HR3F or per-label (drop/buildup) numbers in Raveform; no allin1 vs SongFormer vs Raveform head-to-head on the same EDM set.

## 6. Practical: license, install, Apple Silicon, maintenance, runtime

### Takeaway
allin1 (MIT) is the most turnkey but has a fragile NATTEN dependency on macOS; Beat This! (MIT) is clean pip/PyTorch and CPU-capable; SongFormer (CC-BY-4.0) is Linux/CUDA-tested only. Essentia AV models are non-commercial. All run offline; GPU runtimes are seconds per track, CPU/MPS untested in docs.

### Cited Findings
- allin1: MIT; requires PyTorch, NATTEN ("macOS will auto-install"), madmom from GitHub, optional FFmpeg for MP3; default device cuda else cpu; Apple Silicon not mentioned — [allin1 GitHub](https://github.com/mir-aidj/all-in-one)
- allin1 open issues include "[macOS] [natten] failed to install natten" (Jan 2024), "MPS Alternatives to Natten" (Jun 2024), "[macOS] Unable to install allin1: No module named 'torch'" (Apr 2025), "Incompatibility issues with NATTEN v0.17.5" (May 2025), CPU-heavy complaint (Nov 2025), commercial-use and license questions (Oct 2026); 16 open issues — [allin1 issues](https://github.com/mir-aidj/all-in-one/issues)
- Beat This!: MIT, `pip install beat-this`, PyTorch >= 2.0, CPU fallback, MPS not mentioned, 245 commits — [beat_this GitHub](https://github.com/CPJKU/beat_this)
- madmom PyPI release does not support current Python/numpy; install from GitHub — [beat_this GitHub](https://github.com/CPJKU/beat_this)
- SongFormer: CC-BY-4.0 code, Python 3.10 conda, Ubuntu 22.04 tested, uses CUDA_VISIBLE_DEVICES, downloads MuQ weights on first run, 2-4 s/song on L40 — [SongFormer GitHub](https://github.com/ASLP-lab/SongFormer)
- CUE-DETR code/checkpoints claimed public (github.com/ETH-DISCO/cue-detr), license unstated — [CUE-DETR paper](https://arxiv.org/html/2407.06823v1)
- Essentia AV models: CC BY-NC-SA, commercial license separately — [Replicate mtg/music-arousal-valence](https://replicate.com/mtg/music-arousal-valence/readme)
- MSAF: MIT, no releases listed — [MSAF GitHub](https://github.com/urinieto/msaf)
- drop-detector: GPL-3.0, FFmpeg, XGBoost — [drop-detector GitHub](https://github.com/felixmeyer6/drop-detector)

### Inferences
- On Apple Silicon, plan to pin a NATTEN version that matches allin1's API (pre-0.17.5 per issue #30) or run allin1 in a CPU-only env; SongFormer likely runs on CPU/MPS with device edits but this is unverified.
- Decode MP3 to WAV once (ffmpeg) before all analysis to avoid the 20-40 ms decoder offset and keep beat timestamps consistent for QLC+.
- CPU runtime per track for any of these is undocumented; Demucs separation will dominate allin1's cost.

### Gaps
- No confirmed Apple Silicon/MPS benchmark for allin1, SongFormer or Beat This!.
- allin1 last commit date and SongFormer/Beat This! minimum Python versions not shown on fetched pages.
- License for Raveform dataset/model and EDMFormer code not stated.
