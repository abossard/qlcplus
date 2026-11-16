# Detecting vocals, instruments and mood over time in EDM tracks (late 2026)

Scope: offline MP3 analysis in a Python tool on macOS Apple Silicon. QLC+ only reads the output files. Research date 2026-11-04. Older (pre-2023) results are labelled as such.

## Source separation: which separator, its quality on EDM, Apple Silicon runtime, licenses, and whether stem RMS envelopes give a usable "what's playing now" timeline

### Takeaway
The BS-RoFormer / Mel-Band RoFormer family (2023 to 2026 community checkpoints) now leads Demucs v4 by about 3 to 4 dB SDR on vocals and 3 dB on drums and bass on MVSEP's Multisong benchmark. Demucs is archived. `audio-separator` (PyTorch MPS) or its MLX port is the practical way to run RoFormer models on Apple Silicon. Stem RMS works well for vocals, drums and bass. It is weak for the "other" stem, where synths, pads and FX all end up, and that matters a lot for EDM.

### Cited Findings
**Paper results on MUSDB18-HQ (academic, 4-stem)**
- BS-RoFormer (ByteDance, 2023): 9.80 dB average SDR on MUSDB18-HQ with no extra data (smaller model). With MUSDB18HQ plus 500 extra songs it reaches 11.99 dB average. It won SDX'23 — [Mel-Band RoFormer paper, arXiv 2310.01809](https://arxiv.org/html/2310.01809v1)
- Mel-Band RoFormer (2023), no extra data, L=6: 9.64 dB average. BS-RoFormer gets 9.92 at L=6 and 10.02 at L=9. Mel-RoFormer is better on vocals (11.21 vs 10.78 at L=6), drums and other, but much worse on bass (9.64 vs 11.43). The authors blame the mel mapping for the bass result — [arXiv 2310.01809](https://arxiv.org/html/2310.01809v1)
- SCNet (Jan 2024): 9.0 dB SDR on MUSDB18-HQ with no extra data. CPU inference takes 48% of HT Demucs time. A third-party listing gives SCNet-large 9.69 average (not checked against the paper tables) — [SCNet, arXiv 2401.13276](https://arxiv.org/abs/2401.13276)
- HT Demucs v4: 9.00 dB on the MUSDB HQ test set. The sparse-attention fine-tuned variant reaches 9.20, but its kernels were never released. The README gives no SDR for htdemucs_6s. Its guitar stem is "okay quality", and its piano stem has "a lot of bleeding and artifacts" — [facebookresearch/demucs README](https://github.com/facebookresearch/demucs)
- The Demucs repo has been archived and read-only since 1 Jan 2025 and is "not maintained anymore". The adefossez/demucs fork gets important bug fixes only. License is MIT — [facebookresearch/demucs](https://github.com/facebookresearch/demucs)

**MVSEP leaderboard (community checkpoints, Multisong dataset SDR; MVSEP's own data, so not comparable to MUSDB numbers)**
- Vocals / instrumental SDR. BS Roformer 124 bands (ver. 2026.07): 12.33 / 18.64, best single model listed. BS Roformer "fullness duality" (becruily): 12.25 / 18.47. BS PolarFormer 124 bands: 12.02 / 18.33. Ensemble 2025.06 (2x BS Roformer + MelBand ft + SCNet XL IHF): 11.93 / 18.23. BS Roformer 2025.07: 11.89 / 18.20. MelBand Roformer 2024.10: 11.28 / 17.59. SCNet XL IHF: 11.11 / 17.41. MDX23C: 10.36 / 16.66. Demucs4 Vocals 2023: 9.04 / 15.35. htdemucs_ft: 8.33 / 14.63 — [MVSEP algorithms](https://mvsep.com/en/algorithms)
- Multi-stem Multisong SDR (vocals / bass / drums / other). Ensemble 2025.06.30: 11.93 / 14.85 / 14.33 / 9.00 (average 13.67). BS Roformer SW (6 stems): 11.30 / 14.62 / 14.11 / 8.71. htdemucs_ft: 8.33 / 12.05 / 11.24 / 5.74. htdemucs: 8.18 / 11.74 / 10.90 / 5.57 — [MVSEP algorithms](https://mvsep.com/en/algorithms)
- Extra stems. BS Roformer SW: guitar 9.05, piano 7.83. For comparison, the Demucs4HT 6-stem piano scores 2.23. MVSEP lists no SDR for synth, keys, organ or rhodes stems — [MVSEP algorithms](https://mvsep.com/en/algorithms)
- DrumSep (Mel Band Roformer): kick 22.22, snare 17.09 SDR. This yields separate kick and snare activity tracks — [MVSEP algorithms](https://mvsep.com/en/algorithms)
- Karaoke (lead vs backing vocals). BS Roformer 124 bands: lead 10.77, back 7.03. Older MDX-B Karaoke: lead 7.94, back 1.88 — [MVSEP algorithms](https://mvsep.com/en/algorithms)
- A quality-checker entry dated 2026-04-15 (BS-Roformer-FNO-SC, 4-stem) has 10.59 vocals SDR, so not every 2026 entry beats the 2025 ensembles — [MVSEP quality checker](https://mvsep.com/quality_checker/entry/9933)

**Running on Apple Silicon**
- `audio-separator` (PyPI) uses PyTorch MPS for RoFormer/MDXC .ckpt models and CoreML for ONNX models. If an unsupported complex op is hit, it falls back to the CPU for that spectral step (`AUDIO_SEPARATOR_FORCE_CPU_COMPLEX=1` forces the CPU path). Native fp16 is verified only for MelBand and BS-RoFormer on MPS/CUDA. The page says Apple Silicon needs PyTorch 2.13 or newer; I could not confirm this anywhere else — [audio-separator on PyPI](https://pypi.org/project/audio-separator/)
- `mlx-audio-separator` ports the audio-separator inference paths to MLX, with no PyTorch or ONNX at inference time. On a 12-song subset on an M4 mini it measured 2.16x faster than audio-separator for a BS-RoFormer checkpoint and 2.50x for a MelBand checkpoint — [mlx-audio-separator on PyPI](https://pypi.org/project/mlx-audio-separator/0.1.16/)
- `bs-roformer-infer` has `mps` and `mlx` backends and needs an arm64 Python — [openmirlab/bs-roformer-infer](https://github.com/openmirlab/bs-roformer-infer)

**Do stem RMS envelopes give reliable activity?**
- Vocals (the best evidence, MLSP 2023). Plain RMS thresholding of Demucs-separated vocals (threshold 0.015 from grid search) gets F1 0.949 / accuracy 0.949 on the Jamendo test set. Trained LRCN and Transformer models on the separated vocals get 0.958 and 0.960 F1. RMS on the raw mixture gets only 0.679 F1. On MedleyDB the same RMS method drops to F1 0.793 (precision 0.688, recall 0.957), which means mostly false positives from bleed — [Bonzi et al., "Exploiting Music Source Separation for Singing Voice Detection", MLSP 2023](https://michelemancusi.netlify.app/publication/svd/svd.pdf)
- SDR is a flawed measure for what a separator outputs during non-vocal sections. Low-level residue there does not show up well in SDR — [Stoller et al., arXiv 1804.01650 (2018, older)](https://arxiv.org/pdf/1804.01650)

### Inferences
- For EDM, the realistic stem timeline is: vocals (RoFormer vocal model), drums (or kick/snare via DrumSep), bass, and "other". The "other" stem (Multisong SDR about 9.0 even for the best ensemble) holds leads, pads, plucks, risers and FX. Its RMS shows that "something melodic or FX is playing", not which instrument it is. Splitting "other" further needs tagging (next sections), not separation.
- Since vocals are about 3 to 4 dB cleaner with RoFormer than with htdemucs, RMS-threshold vocal detection should produce fewer bleed false positives than the Demucs numbers above. That is not measured.
- Each stem needs its own threshold, relative to that stem's own loudness distribution in the track (for example a percentile plus hysteresis). One fixed absolute threshold won't do, because EDM masters are heavily limited and the residue in a stem changes with mix density. The MedleyDB precision drop supports this.
- MVSEP numbers come from MVSEP's own datasets, and the checkpoints are community-trained. Licenses for individual checkpoints vary and should be checked one by one.

### Gaps
- I found no EDM-specific separation benchmark. MVSEP's "Synth" dataset column exists (for example BS Roformer 2025.07: vocals 14.58), but the page doesn't say what Synth contains.
- No published Apple Silicon wall-clock numbers (seconds per track on M-series) for htdemucs or RoFormer apart from the mlx-audio-separator relative speedups.
- Licenses of individual community RoFormer checkpoints (becruily, MVSEP team, viperx, ZFTurbo) were not checked. Some MVSEP top models are server-only and not downloadable.
- No separator offers dedicated synth-lead, pad or FX stems with published SDR.

## Vocal activity detection: separated-vocal energy thresholding vs dedicated singing-voice detectors, and lead vocals vs chops

### Takeaway
Separate first, then detect. RMS on a good vocal stem is within about 1 F1 point of trained detectors on Jamendo. A small classifier trained on separated vocals adds robustness. No published tool tells lead vocals from vocal chops. The closest options are Karaoke lead/back separation models plus heuristics on duration and phrasing.

### Cited Findings
- On Jamendo, separated vocals plus RMS reach F1 0.949, separated vocals plus LRCN 0.958, separated vocals plus Transformer 0.960. Earlier mixture-input detectors: Schlüter et al. accuracy 0.923, Lehner et al. F1 0.902, Leglaive et al. F1 0.910 (all pre-2023). Zhang et al. on vocals: F1 0.927 — [Bonzi et al., MLSP 2023](https://michelemancusi.netlify.app/publication/svd/svd.pdf)
- Training detectors directly on mixtures performs clearly worse: LRCN accuracy 0.868 on the mixture vs 0.960 on separated vocals — [Bonzi et al., MLSP 2023](https://michelemancusi.netlify.app/publication/svd/svd.pdf)
- Older reviews: deep SVD models reached over 90% recall and accuracy on common datasets. Whether "singing voice" includes backing vocals is ill-defined, which affects labels — [Lee, Choi, Nam, "Revisiting Singing Voice Detection" (2018, older)](https://ar5iv.labs.arxiv.org/html/1806.01180); survey of CNN, LSTM and GRU methods — [Monir et al., Entropy 2022 (older)](https://www.mdpi.com/1099-4300/24/1/114)
- A 2024 thesis also found that Demucs pre-separation improves Transformer SVD on Jamendo and DALI (a small study) — [UFPB thesis 2024](https://repositorio.ufpb.br/jspui/bitstream/123456789/34519/1/Arthur%20Ruan%20Bizerra%20Florentino_TCC.pdf)
- Practical pipeline in the literature: Demucs vocals, then the pyannote VAD gives segment timecodes — [SingFake, arXiv 2309.07525](https://arxiv.org/pdf/2309.07525)
- Lead vs backing separation is available: MVSEP Karaoke BS Roformer 124 bands scores lead 10.77 / back 7.03 SDR — [MVSEP algorithms](https://mvsep.com/en/algorithms)
- An ASR-style check for words: Music Flamingo reports lyric WER of 12.9 on Opencpop and 19.6 on MUSDB18. GPT-4o gets 53.7 / 32.7 and Qwen2.5-Omni 55.7 / 68.7 — [NVIDIA Music Flamingo page](https://research.nvidia.com/labs/adlr/MF)

### Inferences
- Lead vocal vs chop heuristic, built from the components above (untested): on the separated vocal stem, compute active-segment durations and gaps. Then run an ASR or lyric model on the segments. Sustained phrases (several seconds or more) that produce transcribable words are lead or topline. Short (under about 1 s), repetitive, rhythmically quantized bursts with no consistent words are chops or samples. The Karaoke model's lead/back split helps tell a lead from pads and harmonies.
- Heavily processed EDM vocals (vocoder, pitch-shifted chops, formant FX) may be assigned partly to "other" by separators trained mostly on pop/rock. Vocal detection recall may drop on these. No source measures this.

### Gaps
- I found no 2024 to 2026 SVD benchmark on EDM or electronic music, and no dedicated detector for vocal chops vs lead vocals.
- I found no public 2025 to 2026 SVD model that clearly beats "RoFormer vocals + threshold". The search reached older surveys and the 2023 MLSP paper only.

## Instrument / sound-type recognition over time: PANNs, BEATs, CLAP zero-shot, Essentia heads, MERT/MuQ probes. Which works per segment on EDM?

### Takeaway
No off-the-shelf model gives reliable per-frame labels for EDM-specific classes (synth lead, pad, riser, pluck). AudioSet taggers (PANNs, BEATs, EAT) give general frame-level classes (synthesizer, drum, bass, singing) with documented accuracy. Zero-shot CLAP text prompts are unreliable for instruments. The best-supported route is to run taggers on separated stems, and, for EDM-specific classes, train a small linear probe on frozen MERT, MuQ or CLAP audio embeddings using a few hundred labelled EDM clips.

### Cited Findings
- AudioSet tagging mAP. PANNs CNN14: 0.431 (2019, older) — [PANNs, arXiv 1912.10211](https://arxiv.org/pdf/1912.10211). BEATs iter3: 48.0, BEATs iter3+: 48.6, EAT: 48.6 (fine-tuned on AS-2M). On AS-20K the same three score 38.3 / 38.9 / 40.2 — [arXiv 2601.21612 (2026) comparison table](https://arxiv.org/pdf/2601.21612). A benchmark site gives BEATs iter3+ 0.498 and AST 0.485, which conflicts with that table, probably due to a different setup — [codesota audio classification](https://codesota.com/audio/classification)
- CLAP zero-shot instruments (ISMIR 2024, "I can listen but cannot read"), tested on TinySOL with 14 classes. Music CLAP and Music/Speech CLAP perform similarly. MusCALL's top-1 is worse than random for 4 of 6 prompts. Using audio-derived label embeddings instead of text prompts roughly doubles the metrics, so the text branch is the weak link. Positive and negative similarity distributions overlap heavily. Musically informed prompts are not reliably better, and removing the instrument word hurts, so the models key on the label word, not on meaning. The ontology triplet test scores only 49 to 59% — [arXiv 2407.18058](https://arxiv.org/html/2407.18058v1)
- Original CLAP (2022, older): 47% zero-shot instrument classification accuracy, 22 points above random — [CLAP, arXiv 2206.04769](https://arxiv.org/pdf/2206.04769)
- Essentia models cover MTG-Jamendo instrument, mood/theme (56 classes), genre Discogs400 (electronic subgenres such as Techno, House, Dubstep, Drum n Bass, Ambient), danceability and voice/instrumental heads. They are built on discogs-effnet, MAEST (5 to 30 s inputs), msd-musicnn, VGGish, YAMNet and OpenL3. Models are licensed CC BY-NC-SA 4.0, with proprietary licenses on request — [Essentia models](https://essentia.upf.edu/models.html)
- MERT-v2 (FullSong and 30s) model cards report frozen-encoder MARBLE probe results, including MTG-Jamendo instrument and mood/theme, with different best layers per task. The extracted table columns were ambiguous, so I'm not quoting numbers — [MERT-v2-FullSong card](https://huggingface.co/m-a-p/MERT-v2-FullSong)
- MuQ (Tencent, 2025) claims SOTA on many MIR tasks in MARBLE using Mel-RVQ targets — [tencent-ailab/MuQ](https://www.github.com/tencent-ailab/MuQ)
- A 2025 comparison of MERT, MusicFM, MAEST and others on MTG-Jamendo and MagnaTagATune found that rankings shift across datasets, tag categories and individual tags. No single embedding wins everywhere — [arXiv 2509.06936](https://arxiv.org/html/2509.06936)
- Stem retrieval: a 2024 JEPA paper addresses zero-shot retrieval of a musical stem from a mix given an instrument query. This is relevant for per-instrument presence — [arXiv 2411.19806](https://arxiv.org/pdf/2411.19806)

### Inferences
- Recommended layering for EDM, synthesized from the above:
  1. Separation gives vocals, drums, bass and other envelopes (the most reliable).
  2. DrumSep gives kick and snare/clap envelopes.
  3. A BEATs or PANNs frame tagger on the "other" stem gives AudioSet classes such as Synthesizer, Piano, Strings and Sound effect. PANNs gives per-frame framewise output through its SED variant.
  4. A linear probe on MERT, MuQ or CLAP audio embeddings over 1 to 5 s windows, trained on a small hand-labelled EDM set, covers lead, pad, pluck, riser/FX, piano and vocal chop.
  The audio-embedding prototype result above ("audio label embeddings double the metrics") supports few-shot prototypes over text prompts.
- If CLAP text prompts are used at all, use them only as relative scores across a track (z-scored per track), not as absolute presence decisions.
- The MTG-Jamendo instrument vocabulary (Essentia) is general (synthesizer, drum machine, piano, voice, and so on), not EDM sound-design roles. It is also non-commercial.

### Gaps
- No published benchmark of any tagger on EDM-specific classes (riser, pad vs lead, pluck). No frame-level EDM instrument-activity dataset was found.
- No exact MuQ vs MERT numbers on MTG-Jamendo instrument and mood were extracted.
- OpenMIC-2018 (2018, older) was not researched in this session.
- LAION-CLAP and MS-CLAP licenses and versions were not verified here.

## Mood / atmosphere over time: Essentia mood heads, DEAM/EmoMusic arousal-valence, CLAP prompts. Can they detect changes within a track?

### Takeaway
Arousal can be tracked over time with acceptable reliability. Valence is consistently harder. The only large dynamic (per-second) dataset is still DEAM (2 Hz annotations). Essentia's DEAM, emoMusic and MuSe regressors plus its binary mood heads can be run on sliding windows, but they were trained on clip-level or pop data and are non-commercial. In EDM, arousal changes within a track (breakdown vs drop) will largely track loudness, density and drums. "Atmosphere" labels such as dark, euphoric or dreamy have no validated per-segment model.

### Cited Findings
- DEAM: 1,802 songs with valence/arousal annotations at 2 Hz, Creative Commons. It was used for MediaEval "Emotion in Music" 2013 to 2015 and is the largest dynamic-annotation dataset (older data) — [DEAM, PLoS ONE 2017](https://pmc.ncbi.nlm.nih.gov/articles/PMC5345802)
- Essentia arousal/valence regressors (2 dimensions, range 1 to 9) trained on DEAM, emoMusic and MuSe, on msd-musicnn and VGGish embeddings. Binary mood heads (aggressive, happy, party, relaxed, sad) and danceability on discogs-effnet, musicnn, VGGish, YAMNet and OpenL3. MTG-Jamendo mood/theme has 56 classes on discogs-effnet. All CC BY-NC-SA 4.0 — [Essentia models](https://essentia.upf.edu/models.html)
- DSAML (Dec 2024): frozen ImageBind encoder plus an adapter, with personalized meta-learning on DEAM and PMEmo. It ranks top on arousal among compared methods. The authors confirm valence is harder to predict — [arXiv 2412.19200](https://arxiv.org/html/2412.19200v1)
- CLAP text prompts key on specific label words and ignore extra context (shown for instruments). This suggests mood adjectives behave the same way, but it was not tested for mood — [arXiv 2407.18058](https://arxiv.org/html/2407.18058v1)

### Inferences
- A workable approach: sliding 5 to 10 s windows with 1 to 2 s hop, Essentia arousal, valence and binary mood heads (non-commercial), plus per-track z-scored CLAP similarity to a small prompt set (dark, euphoric, dreamy, aggressive, melancholic). Smooth the result and segment it at structural boundaries. Treat the output as relative contrast within a track, not absolute labels.
- For lighting, arousal can be derived robustly from stem energies (drums plus bass RMS, onset density). That may be as useful as learned arousal for EDM. Not measured.
- Few-shot probes on MERT or MuQ embeddings with a handful of user-labelled "atmosphere" examples would likely beat zero-shot prompts, by analogy with the instrument finding.

### Gaps
- No 2025 to 2026 dynamic MER model with released weights and EDM evaluation was found.
- No numeric CCC or RMSE values for Essentia's DEAM regressors were extracted.
- No validation of CLAP mood prompts on EDM was found.

## Music-language models for section description: Qwen2-Audio, SALMONN, MU-LLaMA, LP-MusicCaps, newer audio LLMs. Can they reliably label sections? Cost and latency?

### Takeaway
2025 to 2026 music LLMs (NVIDIA Music Flamingo, built on Audio Flamingo 3; Qwen3-Omni) describe whole tracks much better than 2023 to 2024 models. They still have poor timestamp precision unless fine-tuned for temporal grounding. Use them per segment: cut the audio at boundaries found by signal methods, then ask for a description of each chunk. Don't ask them to produce a timeline.

### Cited Findings
- Music Flamingo (NVIDIA, Nov 2025, ICLR 2026): 7B parameters, built on Audio Flamingo 3, 24k-token context, about 15 minutes of audio. Rotary Time Embeddings give each audio token an absolute timestamp. Weights are on HF as `nvidia/music-flamingo-2601-hf` — [NVIDIA Music Flamingo](https://research.nvidia.com/labs/adlr/MF); [arXiv 2511.10289](https://arxiv.org/html/2511.10289v1)
- Music Flamingo benchmarks: MusicCaps GPT-5 score 8.8 vs 7.2 for Qwen3-Omni. MuChoMusic 74.58 vs 52.10 for Qwen3-Omni. MMAU-Pro-Music 65.60 vs 64.90 for Gemini 2.5 Flash. NSynth 75.89 / 80.76 vs 65.5 / 78.9 for AF3 — [NVIDIA Music Flamingo](https://research.nvidia.com/labs/adlr/MF). On SongCaps it scores 8.3 / 8.8 / 8.0 vs 6.5 / 6.7 / 6.2 for AF3. The paper says Qwen3-Omni gives "isolated observations" without a coherent musical narrative — [arXiv 2511.10289](https://arxiv.org/pdf/2511.10289)
- Temporal grounding: zero-shot Qwen2-Audio and Qwen2.5-Omni show low recall and low boundary F1 on precise temporal metrics. Qwen2-Audio-based models struggle with timestamps and are limited to short audio. On one grounding benchmark, fine-tuned TimeAudio gets mIoU 57.8, Qwen2-Audio 51.7 and SALMONN-7B 51.9. Fixes such as SpotSound, TimePro-RL and TAC (Timestamped Audio Captioning, 2026) add timestamp tokens or embeddings — [TAC, arXiv 2602.15766](https://arxiv.org/pdf/2602.15766); [SpotSound](https://loiesun.github.io/spotsound/); [TimeAudio README](https://huggingface.co/lysanderism/TimeAudio/blob/main/README.md); [Listening Between the Frames, arXiv 2511.11039](https://arxiv.org/html/2511.11039v2)

### Inferences
- A 7B audio LLM per 10 to 30 s chunk is feasible offline on Apple Silicon only with quantized or MLX ports, if they exist. Otherwise it needs a CUDA box or a cloud API. Rough cost: one LLM call per section (about 10 to 20 per track) instead of per frame.
- Use the LLM output as free-text section labels and atmosphere words. Use the deterministic signals (stems, taggers) for timing and for on/off presence.

### Gaps
- License of Music Flamingo weights (likely NVIDIA non-commercial, not verified), Apple Silicon support, and per-chunk latency were not found.
- No evaluation of any audio LLM on EDM section labelling (build/drop/breakdown) was found.
- Qwen3-Omni, Qwen2-Audio, SALMONN, MU-LLaMA and LP-MusicCaps licenses, sizes and latencies were not verified in this session. LP-MusicCaps and MU-LLaMA are 2023 (older) and are surpassed on MusicCaps by the models above per the Music Flamingo comparisons.

## Practical: install status, Python versions, Apple Silicon, licenses, maintenance

### Takeaway
Separation is the most mature, Mac-ready piece (audio-separator with MPS or MLX, actively maintained). Demucs is archived but still usable (MIT). Essentia's TF models are convenient but non-commercial. Taggers and embedding models run in plain PyTorch on MPS. Check each license before any commercial use.

### Cited Findings
- `audio-separator`: maintained on PyPI, with MPS support for RoFormer and CoreML for ONNX. Apple Silicon needs macOS Sonoma or later — [audio-separator on PyPI](https://pypi.org/project/audio-separator/)
- `mlx-audio-separator`: an MLX inference port of audio-separator, 2 to 2.5x faster on an M4 — [PyPI](https://pypi.org/project/mlx-audio-separator/0.1.16/)
- Demucs: archived since 2025-01-01, MIT, bug-fix-only fork at adefossez/demucs — [GitHub](https://github.com/facebookresearch/demucs)
- SCNet: official PyTorch code at starrytong/SCNet — [arXiv 2401.13276](https://arxiv.org/abs/2401.13276)
- Essentia models: CC BY-NC-SA 4.0, proprietary license on request — [Essentia models](https://essentia.upf.edu/models.html)

### Inferences
- Suggested stack, from the evidence above. audio-separator (BS/Mel RoFormer vocals + 4/6-stem + DrumSep + Karaoke) gives stem RMS timelines. A threshold or small classifier on the vocal stem gives vocal activity. Lead vs chop comes from the Karaoke split plus segment duration and ASR heuristics. BEATs or PANNs on stems gives coarse classes. A MERT, MuQ or CLAP-embedding probe gives EDM roles. Essentia arousal/valence/mood (non-commercial) plus z-scored CLAP prompts give atmosphere. An optional Music Flamingo call per section gives text descriptions. Write the output as a JSON/CSV timeline at a fixed hop (for example 0.1 to 0.5 s) for QLC+.

### Gaps
- Python version support (3.11, 3.12, 3.13) for essentia-tensorflow on macOS arm64, and for MERT and MuQ, was not verified.
- Licenses for PANNs, BEATs, LAION-CLAP, MS-CLAP, MERT, MuQ and Qwen audio models were not verified this session.
