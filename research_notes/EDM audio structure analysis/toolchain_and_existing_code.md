# Offline EDM audio analysis: macOS arm64 toolchain and existing QLC+ code

Scope note: "verified" below means verified against live PyPI JSON metadata (wheel filenames, `requires_python`, `requires_dist`), GitHub API data and model-host file sizes, queried 2026-11-04. Nothing was installed or executed. Runtime figures come from published benchmarks on other Apple chips. Figures for this machine are labelled as inference.

Local machine (read-only checks): Apple M1 Max, 64 GB RAM. Homebrew has `python@3.11` to `python@3.15` (default `python3` = 3.15.2). `uv 0.13.7` (Homebrew, 2026-11-03), `pyenv` and `conda` are present, as is `ffmpeg`. `audiowaveform` is not installed (Homebrew has a bottle for 1.11.1).

## 1. Install status and Python version requirements on macOS arm64

### Takeaway
Python 3.15 is too new for this stack. torch, numpy, scipy, numba and mlx ship cp315 arm64 wheels, but essentia, tensorflow, onnxruntime, sphn (a demucs dependency), samplerate and h5py do not. **Python 3.13** is the newest version where essentia, essentia-tensorflow, TensorFlow, onnxruntime, torch and demucs all have arm64 wheels. Use one uv-managed 3.13 project for the main pipeline. Run the numpy<2 holdouts (madmom/allin1, the laion-clap and msclap packages) in a separate env, or avoid them.

### Cited Findings
**Core / DSP**
- librosa 1.0.0 (2026-08-11): pure-Python wheel, `requires_python >=3.12`. Depends on numba>=0.61, numpy>=2.1, scipy>=1.15, soxr>=1.0, soundfile. — [PyPI librosa](https://pypi.org/pypi/librosa/json)
- numba 0.68.1 and llvmlite 0.50.0 ship arm64 wheels for cp310 to cp315. numpy 2.5.4 and scipy 1.18.1 ship cp312 to cp315. — [PyPI numba](https://pypi.org/pypi/numba/json), [PyPI numpy](https://pypi.org/pypi/numpy/json), [PyPI scipy](https://pypi.org/pypi/scipy/json)
- soundfile 0.14.0 ships a `py2.py3-none-macosx_11_0_arm64` wheel (bundles libsndfile). — [PyPI soundfile](https://pypi.org/pypi/soundfile/json)
- soxr 1.1.0 arm64 wheels: cp39 to cp311, cp312 abi3 (covers 3.12+), cp314t. — [PyPI soxr](https://pypi.org/pypi/soxr/json)

**Essentia**
- essentia / essentia-tensorflow latest is 2.1b6.dev1438 (2026-05-19), with macOS **cp314 only** (`macosx_15_0_arm64`). The previous release, 2.1b6.dev1389 (2025-07-24), has macOS arm64 wheels for cp39 to cp313. No cp315 wheel exists, and no sdist is published. — [PyPI essentia](https://pypi.org/pypi/essentia/json), [PyPI essentia-tensorflow](https://pypi.org/pypi/essentia-tensorflow/json)
- Essentia is AGPL-3.0 (MTG/essentia, last push 2026-10-22). — [GitHub MTG/essentia](https://github.com/MTG/essentia)
- TensorFlow 2.21.0 macOS arm64 wheels: cp310 to cp313 only. — [PyPI tensorflow](https://pypi.org/pypi/tensorflow/json)
- Essentia TF model sizes (HTTP Content-Length): discogs-effnet-bs64-1.pb is 17.5 MB, genre_discogs400 head 2.0 MB, mtg_jamendo_moodtheme head 2.6 MB. — [essentia.upf.edu/models](https://essentia.upf.edu/models/feature-extractors/discogs-effnet/discogs-effnet-bs64-1.pb)
- MAEST (discogs-maest-30s-pw-129e) is 331 MB as safetensors on HF, licensed CC-BY-NC-SA-4.0. — [HF mtg-upf/discogs-maest-30s-pw-129e](https://huggingface.co/mtg-upf/discogs-maest-30s-pw-129e)

**madmom**
- PyPI madmom 0.16.1 is from 2018-11-14 and ships an sdist only. — [PyPI madmom](https://pypi.org/pypi/madmom/json)
- GitHub main's last code commit is 2024-08-25, "CI and NumPy compatibility updates (#540)". There is no tag after v0.16.1. — [GitHub CPJKU/madmom](https://github.com/CPJKU/madmom)
- Open PR #559 (2026-05-09) is needed for **Python 3.14 / NumPy 2.4+**. It fixes setup.py's use of `distutils` (removed in 3.12) and legacy pickled model dtypes that warn on NumPy 2.4 and "will hard-fail" later. Other open numpy-compat issues: #557, #548, #542, and #463 (Cython not installed before setup.py). — [madmom #559](https://github.com/CPJKU/madmom/pull/559), [madmom issues](https://github.com/CPJKU/madmom/issues/463)
- The Beat This README says the PyPI madmom "only supports Python<3.10 and numpy<1.20" and recommends `pip install git+https://github.com/CPJKU/madmom.git`. — [Beat This README](https://github.com/CPJKU/beat_this)

**allin1 (All-In-One music structure analyzer)**
- PyPI allin1 1.1.0 dates from 2023-10-10. The repo was last pushed 2024-05-09 (MIT). `requires_dist` includes demucs, librosa, hydra-core and `natten; platform_system == 'Darwin'`. — [PyPI allin1](https://pypi.org/pypi/allin1/json), [GitHub mir-aidj/all-in-one](https://github.com/mir-aidj/all-in-one)
- NATTEN on PyPI is sdist-only (0.21.7, also 0.17.5). Newer NATTEN releases removed the functions allin1 calls. Open/closed issues: #23 "natten1dqkrpb() takes 4 positional arguments but 5 were given", #30 (NATTEN 0.17.5 incompatibility), #28 "[macOS] Unable to install allin1". — [PyPI natten](https://pypi.org/pypi/natten/json), [allin1 issues](https://github.com/mir-aidj/all-in-one/issues)
- Open PR #39 (2026-09-20, unmerged) reimplements the four NATTEN ops in plain PyTorch and makes NATTEN optional on CPU/MPS. Its measurements on an Apple M5 (harmonix-all, 2:24 song, structure model only, stems cached): NATTEN naive CPU 148 s, `natten_torch` CPU 11 s, `natten_torch` MPS 26 s. Results were identical, and segment boundaries stayed within 0.52 s on a 5:31 song. — [allin1 PR #39](https://github.com/mir-aidj/all-in-one/pull/39)
- A community fork (not-matt/all-in-one, PR #37) documents a working install with torch 2.7 + CUDA 12.8 + natten 0.21 + madmom from git. That path is CUDA-only. — [allin1 PR #37](https://github.com/mir-aidj/all-in-one/pull/37)
- allin1 output: `bpm`, `beats`, `downbeats`, `beat_positions`, and `segments` with `start`/`end`/`label` (labels such as start, intro, chorus, verse, bridge, outro). Default model `harmonix-all`. — [allin1 README](https://github.com/mir-aidj/all-in-one)

**Beat This!**
- beat-this 1.1.0 (2026-04-14) is pure Python. It depends on torch>=2, torchaudio, einops, rotary-embedding-torch and soxr, and needs madmom only for optional `--dbn`. The repo is MIT, last pushed 2026-05-28. — [PyPI beat-this](https://pypi.org/pypi/beat-this/json), [GitHub CPJKU/beat_this](https://github.com/CPJKU/beat_this)
- Models: `final0..2` are about 78 MB each, `small0..2` about 8.1 MB. They download automatically. Batch CLI flags: `--touch-first --skip-existing`. Device falls back to CPU when CUDA is unavailable (the README does not mention MPS). — [Beat This README](https://github.com/CPJKU/beat_this)

**Demucs**
- facebookresearch/demucs is archived. **adefossez/demucs** is "the officially maintained Demucs" (last push 2026-10-26). PyPI demucs 4.1.0 (2026-07-11) requires Python >=3.10. On arm64 it requires torch>=2.1 with no numpy<2 pin (the pin applies to Intel Macs only). It depends on `sphn`. — [GitHub adefossez/demucs](https://github.com/adefossez/demucs), [PyPI demucs](https://pypi.org/pypi/demucs/json)
- sphn 0.2.1 arm64 wheels: cp310 to cp314 only, no cp315. — [PyPI sphn](https://pypi.org/pypi/sphn/json)
- demucs-mlx 1.6.1 (2026-10-05, Python >=3.10, needs MLX >=0.32.3, no torch needed for inference). The default htdemucs weights are about 160 MB. — [PyPI demucs-mlx](https://pypi.org/project/demucs-mlx/)

**audio-separator / RoFormer**
- audio-separator 0.48.0 (2026-10-29): `requires_python !=3.14.1,>=3.10`. On Apple Silicon it **requires torch>=2.13,<3**. Other dependencies include onnxruntime (extra `cpu`), diffq (non-Windows), samplerate==0.1.0 and numpy>=2. — [PyPI audio-separator](https://pypi.org/pypi/audio-separator/json)
- README for Apple Silicon: PyTorch models use MPS and ONNX models use CoreML. MDXC/RoFormer `.ckpt` models run on MPS with spectral ops kept on-device. Native fp16 is verified for MelBand/BS-RoFormer on MPS. `--use_autocast --use_torch_compile` speeds up warm repeated runs but has a cold-start cost. — [audio-separator README](https://github.com/nomadkaraoke/python-audio-separator)
- Open issue #314: on MPS, RoPE and RMSNorm in fp32 take 30 to 46% of Mel-RoFormer attention time. — [audio-separator #314](https://github.com/nomadkaraoke/python-audio-separator/issues/314)
- Model list includes htdemucs/htdemucs_ft/htdemucs_6s (4-stem SDRs: vocals 10.8, drums 10.1, bass 11.9 for _ft) and `model_bs_roformer_ep_317_sdr_12.9755.ckpt` (vocals 12.9, instrumental 17.0; 2 stems only). — [audio-separator README](https://github.com/nomadkaraoke/python-audio-separator)
- Wheel gaps for transitive dependencies: onnxruntime 1.31.1 covers cp311 to cp314 (no cp315). samplerate 0.2.4 covers cp39 to cp314. diffq 0.2.4 arm64 wheels exist only for cp38 to cp310, so newer Pythons build from sdist (needs a C compiler and Cython). — [PyPI onnxruntime](https://pypi.org/pypi/onnxruntime/json), [PyPI samplerate](https://pypi.org/pypi/samplerate/json), [PyPI diffq](https://pypi.org/pypi/diffq/json)
- bs-roformer (lucidrains) 1.4.1 is pure Python and is a training/architecture library (no bundled separation CLI). — [PyPI bs-roformer](https://pypi.org/pypi/bs-roformer/json)
- ZFTurbo/Music-Source-Separation-Training is the active hub for RoFormer checkpoints and training (last push 2026-11-03). — [GitHub ZFTurbo/MSST](https://github.com/ZFTurbo/Music-Source-Separation-Training)

**Embeddings / tagging**
- laion-clap 1.1.7 pins `numpy<2.0.0` and pulls in wandb, webdataset and h5py. msclap 1.3.4 pins `numpy<2`, `librosa<0.11` and `transformers<5`. microsoft/CLAP is archived. — [PyPI laion-clap](https://pypi.org/pypi/laion-clap/json), [PyPI msclap](https://pypi.org/pypi/msclap/json), [GitHub microsoft/CLAP](https://github.com/microsoft/CLAP)
- numpy 1.26.4 (last 1.x) has macOS arm64 wheels only for cp39 to cp312. — [PyPI numpy 1.26.4](https://pypi.org/pypi/numpy/1.26.4/json)
- CLAP checkpoint sizes: LAION `630k-audioset-best.pt` 1777 MB, `music_audioset_epoch_15_esc_90.14.pt` 2243 MB (CC0). HF-transformers versions: `laion/clap-htsat-unfused` 586 MB, `laion/larger_clap_music` 740 MB (Apache-2.0). MS CLAP 2023 weights 657 MB (MS-PL). — [HF lukewys/laion_clap](https://huggingface.co/lukewys/laion_clap), [HF laion/larger_clap_music](https://huggingface.co/laion/larger_clap_music), [HF microsoft/msclap](https://huggingface.co/microsoft/msclap)
- panns-inference 0.1.1 (2023, pure Python, depends on librosa and torchlibrosa). The Cnn14_mAP=0.431.pth checkpoint is 312 MB and the 16 kHz variant 342 MB (Zenodo). — [PyPI panns-inference](https://pypi.org/pypi/panns-inference/json), [Zenodo 3987831](https://zenodo.org/records/3987831)
- MERT: `MERT-v1-95M` pytorch_model.bin 360 MB, `MERT-v1-330M` 1203 MB. Both are **CC-BY-NC-4.0**. — [HF m-a-p/MERT-v1-95M](https://huggingface.co/m-a-p/MERT-v1-95M), [HF m-a-p/MERT-v1-330M](https://huggingface.co/m-a-p/MERT-v1-330M)
- transformers 5.22.1 is pure Python, `requires_python >=3.11`. — [PyPI transformers](https://pypi.org/pypi/transformers/json)

**torch / MPS**
- torch 2.15.0 (2026-10-28) ships `macosx_14_0_arm64` wheels for cp311 to cp315 (plus free-threaded 3.14t/3.15t). It requires Python >=3.11. torchaudio 2.11.1 ships a `py3-none-macosx_14_0_arm64` wheel. — [PyPI torch](https://pypi.org/pypi/torch/json), [PyPI torchaudio](https://pypi.org/pypi/torchaudio/json)
- mlx 0.32.4 arm64 wheels: cp310 to cp315. — [PyPI mlx](https://pypi.org/pypi/mlx/json)

### Inferences
- **Python version: 3.13.** It is the newest version where every heavy dependency has an arm64 wheel: essentia(-tensorflow) dev1389, TensorFlow 2.21, onnxruntime, sphn, samplerate, h5py, torch 2.15 and librosa 1.0 (needs >=3.12). Python 3.14 only works with essentia dev1438 and loses TensorFlow (essentia-tensorflow bundles its own TF, so that may not matter). 3.15 blocks demucs (sphn), audio-separator (onnxruntime, samplerate) and essentia.
- **Environment manager: uv**, with a project-local `.python-version` of 3.13 and a `uv.lock`. uv downloads its own CPython, so nothing system-wide changes. The machine already has uv-managed cpython 3.13.13. Suggested layout:
  - env A `analysis` (3.13): torch, torchaudio, beat-this, demucs (or demucs-mlx), audio-separator[cpu], librosa, essentia-tensorflow==2.1b6.dev1389, transformers (for MERT and HF `ClapModel`), soundfile.
  - env B `allin1` (3.12 or 3.13, optional): allin1 from the PR #39 branch (no NATTEN) plus madmom from git main or PR #559. Keep it isolated because madmom needs a Cython build and is fragile.
  - Avoid `laion-clap` and `msclap` (numpy<2, which has no cp313 wheels). Load CLAP through `transformers` (`laion/larger_clap_music`) instead.
- madmom on 3.13 from git main should build (setuptools still provides the distutils shim below v81). This is untested here. Pin setuptools<81 or apply PR #559.
- allin1 on macOS works only with patches. Upstream is unmaintained (last push 2024-05), NATTEN has no Mac wheels and breaks its API, and the madmom dependency comes from git. A Beat This + custom segmentation stack carries less risk. Treat allin1 as an optional section-label oracle.
- Licenses matter if outputs or models ship. MERT and MAEST are non-commercial. Essentia is AGPL, but calling it from a separate Python process keeps it out of the QLC+ binary (this is my reading of the license, not legal advice).

### Gaps
- No import or run test was done. Whether madmom git main compiles against numpy 2.5 on 3.13 is unverified.
- Whether essentia dev1438's missing cp313 wheel is intentional (a build-matrix change) or a partial upload is unknown.
- Whether PyTorch MPS works end to end for Beat This (rotary embeddings, STFT) is not documented. The README mentions only CUDA and CPU.

## 2. Throughput per 6-minute track on Apple Silicon (planning ~800 tracks)

### Takeaway
Published Apple numbers exist only for Demucs (M4 Max, M1 Pro MLX), Beat This (M4 CPU) and allin1's structure model (M5). The rest are gaps. For an M1 Max, about **1 to 2 minutes per track** for decode, beats, 4-stem separation, structure and embeddings is plausible. That is about 15 to 30 hours for 800 tracks (80 hours of audio). Content-hash caching makes it a one-time cost.

### Cited Findings
- htdemucs on an **M4 Max**, 120 s stereo input, median of 4 warm runs: PyTorch CPU 19.4 s (6x realtime), PyTorch MPS 2.82 s (42x), MPS with STFT kept on-device 2.39 s (50x), demucs-mlx 1.05 s (114x). — [PyPI demucs-mlx](https://pypi.org/project/demucs-mlx/)
- htdemucs via an MLX port on an **M1 Pro**: 14.8 s for a 3:19 track (batch 8). An fp16 variant reports 7.1 s on M1 Pro. — [WebSearch summary of demucs-mlx / HF schism-audio pages](https://huggingface.co/schism-audio/htdemucs-mlx) (secondary; not re-fetched)
- A Japanese setup guide lists about 120 s for htdemucs and 240 s for htdemucs_ft on an M1 Pro, without stating track length or device. — [stemsplit.io guide](https://stemsplit.io/ja/blog/demucs-local-setup-guide) (low confidence, vendor blog)
- Demucs GPU memory: about 7 GB with default args, 3 GB with `--segment 8`. — [adefossez/demucs README](https://github.com/adefossez/demucs)
- Beat This (Python, PyTorch **CPU**, no DBN) on an **M4 MacBook Pro**: 9 s file in 1.8 s, 4:32 in 5.1 s, 13:48 in 11.9 s. ONNX ports run about the same. — [beat-this Rust crate README](https://docs.rs/crate/beat-this/0.3.0/source/README.md)
- MLX port of Beat This final0: about 22 ms per 30 s chunk on an M5 Max. — [HF schism-audio/beat-this-mlx](https://huggingface.co/schism-audio/beat-this-mlx) (via search snippet)
- allin1 structure model only (stems cached), **M5**, 2:24 song: 11 s CPU or 26 s MPS with the PR #39 pure-torch attention, 148 s with NATTEN's naive CPU kernel. — [allin1 PR #39](https://github.com/mir-aidj/all-in-one/pull/39)
- allin1 harmonix-all on an RTX 4090: 10 songs (33 min) in 73 s. — [allin1 README](https://github.com/mir-aidj/all-in-one)
- BS-RoFormer family on an RTX 5090: about 15.5 to 33 s per hour of audio without TTA. TTA roughly triples the time. No Apple numbers found. — [PyPI pymss](https://pypi.org/project/pymss/2.0.0/)

### Inferences
Estimates for a 6-minute track on an M1 Max. Scaled from the figures above, assuming the M1 Max GPU is about 2 to 3x slower than an M4 Max and its CPU about 1.5x slower than an M4. Unmeasured.

| Step | Estimate (6 min track) | Basis |
| --- | --- | --- |
| ffmpeg decode to 44.1k WAV + 22.05k mono | ~1 to 2 s | typical ffmpeg speed (unmeasured) |
| librosa features (RMS, onset env, chroma, mel) | ~3 to 10 s | unmeasured; numba JIT warm-up on the first track |
| Beat This final0 CPU | ~8 to 12 s | 4:32 = 5.1 s on M4 |
| htdemucs, PyTorch MPS | ~20 to 30 s | 120 s = 2.82 s on M4 Max |
| htdemucs, demucs-mlx | ~8 to 15 s | M1 Pro 3:19 = 14.8 s; M1 Max has 2x the GPU cores |
| htdemucs, CPU | ~1.5 to 2 min | 120 s = 19.4 s on M4 Max |
| allin1 structure (stems cached, PR #39, CPU) | ~30 to 45 s | 2:24 = 11 s on M5 |
| BS-RoFormer vocals (MPS) | unknown, probably 30 s to 2 min | no Apple data |
| Essentia effnet embeddings + heads | ~2 to 5 s | small 17.5 MB model, unmeasured |
| MERT-95M / CLAP on 10 s windows | ~10 to 30 s | unmeasured |

- Budget: about 60 to 120 s per track for 800 tracks is 13 to 27 hours. Run it overnight as a resumable batch, with GPU-bound (Demucs, RoFormer) and CPU-bound (Beat This, librosa, Essentia) stages in parallel processes.
- Storage: a 6-minute 44.1 kHz stereo 16-bit track is 63.5 MB as PCM. Four stems are about 254 MB raw, or roughly 120 to 150 MB as FLAC. For 800 tracks that is about 100 GB as FLAC. Either keep stems only as 22.05 kHz mono FLAC or Opus, or keep only per-stem activity curves and delete the stems.
- Caching: key everything on the sha256 of the file bytes (the research branch plan uses the same key), plus stage name, tool version and model id. Write one JSON per stage (`<sha>/beats.beat_this-final0.json`, `<sha>/stems.htdemucs/…`). The merged per-track file is then a cheap derived artifact that can be rebuilt whenever schema or heuristics change, without re-running models. Use `beat_this --skip-existing` style idempotence everywhere.

### Gaps
- No M1 Max measurement for any tool. Run one 6-minute reference track per stage before committing to the 800-track plan.
- No Apple timings for BS-RoFormer / Mel-RoFormer, MERT, CLAP, PANNs or Essentia RhythmExtractor2013.

## 3. Output format for a C++ consumer (per-track JSON) and precedents

### Takeaway
Use a per-track JSON with times in seconds as floats, plus an explicit beat grid with bar positions, in the spirit of JAMS (time, duration, value, confidence per observation, with a namespace per annotation type). Add DJ-style fields from Rekordbox PSSI (phrase kinds, mood) and VirtualDJ POIs. QLC+ already parses JSON with nlohmann::json in `mcp/`, and Qt's QJsonDocument works elsewhere.

### Cited Findings
- JAMS observation = `time` (s), `duration` (s), `value`, `confidence`. Namespaces: `beat` (value = metrical position or null), `beat_position` (value = {position, measure, num_beats, beat_units}), `tempo` (BPM, confidence 0..1), `onset`, `segment_open` / `segment_salami_*` / `multi_segment` (label + level), and `tag_open` plus vocabulary-constrained tag namespaces. There is no separate downbeat namespace (position 1 = downbeat). — [JAMS namespaces](https://jams.readthedocs.io/en/stable/namespace.html)
- Rekordbox ANLZ: PQTZ beat grid entries hold beat-in-bar (1..4), tempo (BPM×100) and time in ms. PSSI "song structure" (phrase analysis used for lighting) has a mood (1 high, 2 mid, 3 low). High-mood phrase kinds are Intro, Up, Down, Chorus and Outro, with numbered variants. Phrase starts are in **beat numbers**, and there are fill flags and a lighting "bank". The body is XOR-masked in rekordbox 6. PCOB/PCO2 hold cues and loops. — [Deep Symmetry ANLZ docs](https://djl-analysis.deepsymmetry.org/rekordbox-export-analysis/anlz.html)
- Serato Markers2: base64 binary with typed entries CUE (index, position in ms, RGB colour, name), LOOP, COLOR, BPMLOCK and FLIP. — [Holzhaus serato-tags](https://github.com/Holzhaus/serato-tags/blob/main/docs/serato_markers2.md)
- VirtualDJ database.xml, as already parsed by QLC+: `<Scan Bpm>` is the beat period in seconds, `Phase` is the grid anchor, and `<Poi>` entries carry cue/loop/automix/beatgrid with Pos in seconds, Num, Color and Name. — local `qmlui/vdjdatabasereader.h`
- allin1 JSON: `bpm`, `beats`, `downbeats`, `beat_positions`, `segments[{start,end,label}]`. — [allin1 README](https://github.com/mir-aidj/all-in-one)
- QLC+ beat encoding: Function speeds in beat mode are `uint` with 1000 = 1 beat, quantized to 1/16. — `docs/beat-timing-analysis.md` on `origin/claude/research-audio-analysis-N5WGL`
- The research-branch plan stores `beats(time_ms, beat_index, bar_index, confidence)`, `onsets(time_ms, band, strength)`, `structural_events(time_ms, kind build|drop|break|outro, confidence)` and a chroma blob in SQLite. — `docs/audio-dsp-upgrade-plan.md` on that branch

### Inferences
Suggested schema (`<sha256>.analysis.json`, versioned):
- `schema_version`; `track {path, sha256, duration_s, sample_rate, title, artist}`; `analyzers[] {stage, tool, version, model, params_hash, created}`.
- `tempo {bpm, confidence, stable: bool}`; `beats [{t, bar, beat_in_bar, confidence}]` (downbeats = beat_in_bar 1, so no separate list needed). Optionally `grid {anchor_s, period_s}` when the tempo is constant, which matches VDJ Scan/Phase and lets C++ compute Function beat values.
- `sections [{start_s, end_s, start_bar, bars, label, energy, confidence, source}]` with an EDM label vocabulary (intro, build, drop, breakdown, outro, plus PSSI-style up/down/chorus mapping).
- `events [{t, kind: drop|build_start|riser|impact|silence|filter_sweep, bar, confidence, source}]`.
- `stems {hop_s, names:[drums,bass,vocals,other], activity:{name:[float 0..1]}}`. A ~10 to 20 Hz curve is enough for lighting. Store larger curves as base64 float32 or in a sidecar `.npy`/`.f32` to keep the JSON small.
- `curves {hop_s, rms_db, low, mid, high, onset_strength}`; `tags [{label, score, source}]`; `embeddings_ref` (path, not inline).
- Keep human review ratings in a separate file keyed by sha and stage, so re-running analysis never overwrites ratings.
- C++ side: parse with nlohmann::json, which is already vendored for `mcp/`. Map `beats` to a Show's beat grid, the same way `VdjDatabaseReader::SongGrid` (beatPeriodMs, anchorMs, pois) feeds the Show editor's VDJBeat mode today.

### Gaps
- No official Rekordbox/Serato/VDJ structure-export standard. Of the three, only PSSI is a phrase/structure precedent.
- JAMS has no EDM vocabulary for drop/build. A custom label set is needed.

## 4. Web review UI building blocks

### Takeaway
Use wavesurfer.js v8 (BSD-3, active, Regions + Spectrogram plugins, accepts precomputed `peaks` + `duration`) on a static page. Serve short MP3 excerpts cut with ffmpeg around each predicted event and ask yes/no/offset questions per excerpt. peaks.js + audiowaveform is the BBC alternative but is slower-moving.

### Cited Findings
- wavesurfer.js 8.0.5, released 2026-11-02, BSD-3-Clause, ~10.4k stars. Official plugins in `src/plugins`: envelope, hover, minimap, record, regions, spectrogram, spectrogram-windowed, spectrogram-worker, timeline, zoom. Regions = "visual overlays and markers for regions of audio". — [GitHub katspaugh/wavesurfer.js](https://github.com/katspaugh/wavesurfer.js)
- WaveSurferOptions include `peaks` ("Pre-computed audio data, arrays of floats for each channel"), `duration` ("Pre-computed audio duration in seconds"), `media` (an existing media element) and `backend` (default MediaElement). — [wavesurfer.js docs](https://wavesurfer.xyz/docs/types/wavesurfer.WaveSurferOptions)
- peaks.js (BBC, LGPL-3.0): zoomable/scrollable waveform with segments and points, using pre-computed waveform data (from audiowaveform) or Web Audio. Latest release v3.4.2 (2024-08-15), repo pushed 2026-10-21. — [GitHub bbc/peaks.js](https://github.com/bbc/peaks.js)
- audiowaveform (GPL-3.0) generates waveform data/images. Latest GitHub release 1.10.3 (2025-08-20). Homebrew bottle 1.11.1, homepage moved to codeberg.org/chrisn/audiowaveform. — [GitHub bbc/audiowaveform](https://github.com/bbc/audiowaveform), `brew info audiowaveform` (local)

### Inferences
- Excerpts: `ffmpeg -ss <t-8> -t 16 -i in.flac -ac 2 -b:a 128k -af afade=t=in:d=0.3,afade=t=out:st=15.7:d=0.3 out.mp3` per candidate drop or section boundary. That is about 250 KB each, so a few thousand excerpts total a few hundred MB. Put `-ss` before `-i` for fast seeking. Overlay a click track on beats with an `amix` of a generated click to judge beat/downbeat accuracy by ear.
- Generate peaks in Python (min/max per ~256 samples) into JSON next to each excerpt, so the browser never decodes full tracks. audiowaveform is optional.
- Store ratings in a small local JSON/SQLite through a minimal Python HTTP server, keyed by `(sha, stage, event_id, analyzer_version)`.

### Gaps
- No evaluation of wavesurfer's spectrogram plugin performance on long files. The windowed/worker variants exist, which suggests large files are a known concern.

## 5. Existing QLC+ code: what exists, what is live-only, what can feed an offline pipeline, and what the research branch proposes

### Takeaway
Everything in `engine/audio/src` is a **live, streaming** analyzer: aubio features, a LedFx-style mel post-processor and a standalone beat tracker. None of it produces downbeats, sections or drops, and nothing in `qmlui/` stores song structure. The only per-song musical metadata QLC+ uses today is external: VirtualDJ's beat grid and cue POIs, and the ID3 TBPM tag. The research branch is mostly a plan, aimed at C++-embedded Essentia plus SQLite. Its code is a small live scaffold that predates the aubio work now on `mcp-server`.

### Cited Findings (all local, read-only)
**engine/audio/src on mcp-server (live path)**
- `AubioProcessor` (aubioprocessor.h/.cpp, 997 lines) wraps aubio with hop 500 and window 4096 (`hopSize()`, `windowSize()`). It computes 9 onset methods (energy, hfc, complex, phase, wphase, specdiff, kl, mkl, specflux), tempo/beat/tatum, `beatPhase`/`barPhase`/`beatInBar` (counted with a fixed `m_beatsPerBar = 4`, not detected downbeats), pitch, notes, MFCC(13), 40-band mel plus low/mid/high mel banks, a power mel, TSS transient/steady split, and spectral centroid/spread/rolloff/flux/hfc. Results go into the `AubioResults` struct (aubioresults.h).
- `AudioAnalyzer` (audioanalyzer.h/.cpp) fans `AudioFrame`s out to per-profile `AudioChannel`s. Each owns an `AubioProcessor` (audiochannel.cpp:139) and several `MelPostProcessor`s, and publishes an immutable `AudioSnapshot`: triggers with `firedThisFrame`/`heldMs`/cooldown, mel raw/processed/novelty, beatPower/bassPower/lows/mids/highs, music {beat, bpm, beatPhase, barPhase, beatInBar, confidence}, and spectral {rmsDb, peakDb, crest, centroid, rolloff, flux, flatness}. `AudioCapture::processData` calls `m_analyzer->processFrame(frame)` and emits `beatDetected(bpm)` from aubio (audiocapture.cpp:395-405).
- `MelPostProcessor` is a port of LedFx melbank.py smoothing/AGC/novelty filters (constants cite melbank.py:375-378).
- `BeatTracker` (beattracker.h/.cpp, plus beattracker.md) is by varghele. It is pure C++ with an internal FFT and no Qt/fftw: a 3-band saturating onset front end, then an autocorrelation + harmonic comb + belief filter + octave walk. Its doc reports 64/64 on a synthetic suite (in-repo bench reproduces 63/64), 65% of post-warm-up time within 4% on a 9-song real album, ~1.3% CPU, first estimate after ~4 s, and re-lock ~12-16 s. It is **compiled into the library (CMakeLists.txt:31) but only referenced by `engine/test/beattracker` and `engine/test/beattrackerbench`**, not by production code.
- `AudioDecoder` plugins (`engine/audio/plugins/mad`, `sndfile`) decode files to PCM for the Audio function.

**qmlui (song-level data)**
- `VdjDatabaseReader` (qmlui/vdjdatabasereader.h) reads VirtualDJ `database.xml` read-only: `Scan Bpm` (beat period in seconds), `Phase` (grid anchor), and POIs (cue/loop/remix/automix, Num, Color). The Show editor's VDJBeat mode reads the grid and POIs from it at display time. Nothing is copied into QLC+ structures.
- `ShowFactory` builds one Show per song (Audio + Show + Track) in the `Songs` folder from `DjFsm::DeckSong` and saves song-to-Show mappings in the workspace. `DjManager`, `DjFsm` and `DjSetRecorder` drive the DJ/VDJ integration.
- `AudioBpmTag` reads ID3v2 TBPM/TBP and sets a Show to BPM_4_4 at that tempo.
- No section/drop/phrase data: grep hits for "section" in `pageplanner.*` refer to VC fixture-control page sections, not song sections. `.session-notes/` has no prior offline audio analysis plan. The only "drop" logic is the live JS heuristic in `resources/rgbscripts` (e.g. buildup.js DROP/COOLDOWN states, per `.session-notes/hue-audio-scripts-plan.md`).
- `mcp/tools` has no audio tools (only `show_tools.cpp` is show-related).

**Research branch `origin/claude/research-audio-analysis-N5WGL`**
- Merge base with mcp-server: 2ed7322ec (2026-05-05). The branch has **no** `aubioprocessor.*`. Its 5 commits add `audiofeatures.h`, `liveaudioanalyzer.{h,cpp}` (166 lines), AudioCapture glue, and `engine/test/liveaudioanalyzer` with JSON vectors.
- `LiveAudioAnalyzer::analyze()` maps the 32 log bands to 5 perceptual bands (sub<80, bass<250, lowMid<500, mid<2000, high), computes centroid/rolloff(85%)/flatness/positive flux, and fires a naive onset (flux > max(0.35, 1.8×avg) and rmsDb > −54). `AudioFeatures` includes `source Live|Cached`, `trackId`, `positionMs`, `matchLocked`, `matchConfidence`.
- `docs/audio-dsp-upgrade-plan.md` (697 lines) proposes:
  - Offline **Essentia linked in C++** (AGPL accepted) for BPM (RhythmExtractor2013), beat/bar grid, key (EDMA), 10 Hz chroma, danceability, MFCC and drop candidates from novelty plus a low-band energy ramp heuristic ("rising flux + rising rms over 4–16 bars, then sudden full-band energy after a partial silence").
  - A single **SQLite** `~/.local/share/qlcplus/audio.db` (tracks keyed by sha256, feature_frames ~23 ms, beats with bar_index, onsets per band, structural_events build/drop/break/outro, Olaf fingerprints, chroma blob, audio_profiles).
  - **Olaf** one-shot identification; a tiered **PositionSource** (OS2L / Pro DJ Link / StagelinQ, then chroma cross-correlation with ±8% speed search, then aubio clock); a `CachedAudioAnalyzer` that replays the same `AudioFeatures` at the locked position; MCP tools (`analyze_audio_file`, `get_audio_features`, …).
  - Accuracy targets: BPM ±0.5 on 90%, key Camelot-neighbour on 80%, drops ±1 s on 5 hand-annotated tracks.
  - Its own M0-M5 snapshot (2026-05-05) marks M0 missing, M1 partial, M2-M5 missing.

### Inferences
- **Live-only, keep as-is:** AubioProcessor, AudioChannel/AudioSnapshot, MelPostProcessor. They are stream-based (`process(samples, n)`) and deterministic in audio time, so they *could* run over decoded file PCM. Their value offline is narrow: computing exactly the features the live engine sees, so thresholds tuned offline transfer to live.
- **Should be replaced for offline:** tempo/beat (aubio tempo, BeatTracker) by Beat This (beats + downbeats). Heuristic drops by a section/event model (allin1 labels and/or a custom EDM build/drop detector on stems + energy). BeatTracker's synthetic bench (`engine/test/beattrackerbench`) and the doc's evaluation method (time within 4% tolerance) are reusable as an evaluation harness.
- **Feeds an offline pipeline directly:** the VDJ database (existing user-curated beat grids and cue POIs) is free ground truth or a prior for beat grid and hot cues, and a fallback when analysis is low-confidence. ID3 TBPM is a cross-check. `ShowFactory` song Shows are the natural place in C++ to attach analysis results (sections become Show tracks/items).
- **Reuse from the research branch:** the data model (sha256 track key, beats with bar index, structural_events kinds, per-band onsets, chroma for future position tracking) and the `AudioFeatures.source = Cached` idea. **Replace:** C++-linked Essentia, which conflicts with the user decision that analysis runs in Python, and the SQLite-first storage. Per-track JSON from Python is simpler for review and diffing. SQLite can remain a later C++-side index. Olaf/PositionSource are out of scope (live).
- The branch's code is stale relative to mcp-server. mcp-server already has richer live analysis (aubio, channels, snapshots) than the branch's LiveAudioAnalyzer scaffold, so nothing in that scaffold needs porting.

### Gaps
- I did not inspect the Show editor's VDJBeat rendering code or rgbscriptv4's audio object wiring in detail. How analysis results would surface in Show/VC is not mapped here.
- The other research-branch docs (audio-effects-plan.md, audio-monitor-panel.md, vc-analysis.md) were not read beyond the file list.
