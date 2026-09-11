#!/usr/bin/env python3
"""Generate lawful PCM and execute a pinned, external LedFx reference offline.

Run with the reference checkout's existing Python environment and
PYTHONDONTWRITEBYTECODE=1. No LedFx source is copied or translated here.
"""

import argparse
from contextlib import ExitStack
import copy
import gzip
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import random
import subprocess
import sys
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np

MANIFEST = Path(__file__).with_name("audio_reference_manifest.json")

BANK_BUFFER_ORACLE = {
    "id": "ledfx-distinct-melbank-outputs-v1",
    "deviation": "Only Melbank.__call__ output ownership changes: fresh energy and novelty "
                 "arrays per invocation, then copy into the original public arrays. "
                 "Deferred filter state cannot alias the next invocation's output or "
                 "the public arrays zeroed by the closed gate.",
    "unchanged": "Pinned DSP implementations, coefficients, filter parameters, native aubio, "
                 "callback order, clock, PCM, public array identity and effects. "
                 "Spectrum's width-24 public-array history alias remains.",
    "original_oracle": "external-ledfx-original",
}


def distinct_bank_outputs(original):
    def call(processor, spectrum, processed, novelty):
        energy = np.empty_like(processed)
        difference = np.empty_like(novelty)
        original(processor, spectrum, energy, difference)
        np.copyto(processed, energy)
        np.copyto(novelty, difference)
    return call


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


def specifications(manifest):
    fixtures = copy.deepcopy(manifest["fixtures"])
    fixtures += [
        {"id": f"tone_{hz}", "kind": "tone", "hz": hz,
         "seconds": manifest["tone_duration_s"],
         "rms_dbfs": manifest["tone_rms_dbfs"]}
        for hz in manifest["tone_hz"]
    ]
    fixtures += [
        {"id": f"gain_{abs(db)}db", "kind": "tone", "hz": 997,
         "seconds": 4, "rms_dbfs": db}
        for db in manifest["gain_dbfs"]
    ]
    fixtures += [
        {"id": f"tempo_{bpm}", "kind": "tempo", "bpm": bpm,
         "seconds": manifest["tempo_duration_s"]}
        for bpm in manifest["tempo_bpm"]
    ]
    return fixtures


def kick_train(seconds, rate, bpm, offbeat=0, missing=False, offset=0):
    result = np.zeros(round(seconds * rate), dtype=np.float64)
    t = np.arange(round(0.09 * rate)) / rate
    phase = np.cumsum(2 * np.pi * (150 * np.exp(-25 * t) + 45) / rate)
    kick = 0.9 * (20000 / 32768) * np.sin(phase) * np.exp(-30 * t)
    for pulse, when in enumerate(np.arange(offset, seconds, 30 / bpm)):
        if pulse % 2:
            gain = offbeat if 20 <= when < 40 else 0
        else:
            gain = 1.0
            if missing:
                gain = 0 if pulse // 2 % 11 == 7 else (1 if pulse % 8 == 0 else 0.65)
        start = int(when * rate)
        count = min(len(kick), len(result) - start)
        result[start:start + count] += gain * kick[:count]
    return result


def synthesize(spec, manifest, rate=30000):
    """Original deterministic signals, not a replacement audio analyzer."""
    n = round(spec["seconds"] * rate)
    t = np.arange(n) / rate
    kind = spec["kind"]
    rng = np.random.default_rng(manifest["seed"])
    signal = np.zeros(n)
    if kind == "tone":
        signal = np.sqrt(2) * 10 ** (spec["rms_dbfs"] / 20) * np.sin(2*np.pi*spec["hz"]*t)
    elif kind in ("tempo", "eighths", "missing_accents"):
        signal = kick_train(spec["seconds"], rate, spec["bpm"],
                            spec.get("offbeat_gain", 0), kind == "missing_accents")
    elif kind == "tempo_change":
        half = n // 2
        signal[:half] = kick_train(24, rate, 90)
        signal[half:] = kick_train(24, rate, 128)
    elif kind in ("vocal", "vocal_kicks"):
        fundamental = 137 + 21 * np.sin(2*np.pi*0.31*t) + 8 * t / spec["seconds"]
        phase = np.cumsum(2*np.pi*fundamental/rate)
        formant = 900 + 550 * np.sin(2*np.pi*0.17*t)
        for harmonic in range(1, 35):
            frequency = harmonic * fundamental
            weight = (np.exp(-((frequency-formant)/280)**2)
                      + 0.7*np.exp(-((frequency-2300)/420)**2)) / np.sqrt(harmonic)
            signal += weight * np.sin(harmonic*phase)
        signal *= 0.35 + 0.65*np.sin(np.pi*t/1.7)**2
        signal *= 10**(spec.get("rms_dbfs", -24)/20) / np.sqrt(np.mean(signal**2))
        if kind == "vocal_kicks":
            signal += kick_train(spec["seconds"], rate, 90)
    elif kind in ("noise", "hihat"):
        noise = rng.standard_normal(n)
        noise[1:] -= 0.95 * noise[:-1]
        if kind == "hihat":
            envelope = np.exp(-70*np.mod(t, 0.25))
        else:
            envelope = np.where(np.mod(t, 1.37) < 0.4, np.sin(np.pi*np.mod(t, 1.37)/0.4)**2, 0)
        signal = 0.12 * noise * envelope
    elif kind in ("impulses", "attacks"):
        if kind == "attacks":
            signal = 0.15 * np.sin(2*np.pi*337*t)
        for i, offset in enumerate((0, 1, 127, 249, 499)):
            signal[int((i + 0.5)*rate) + offset] += 0.15 if kind == "attacks" else 0.8
    elif kind == "silence_resume":
        signal[:2*rate] = 0.2*np.sin(2*np.pi*179*t[:2*rate])
        signal[32*rate:] = 0.2*np.sin(2*np.pi*997*t[32*rate:])
    elif kind in ("gate", "gain_steps", "gain_ramp"):
        levels = {"gate": [-90, -81, -79, -65, -81, -79, -45, -90],
                  "gain_steps": [-45, -45, -33, -33, -45, -45, -57, -57]}
        db = (-75 + 55*t/spec["seconds"] if kind == "gain_ramp"
              else np.array(levels[kind])[np.minimum(t.astype(int), 7)])
        signal = np.sqrt(2)*10**(db/20)*np.sin(2*np.pi*997*t)
    elif kind == "clipped":
        signal = np.clip(3*np.sin(2*np.pi*997*t), -1, 1)
    elif kind == "sweep":
        frequencies = 40 * (14000/40)**(t/spec["seconds"])
        signal = 0.2*np.sin(np.cumsum(2*np.pi*frequencies/rate))
    elif kind != "silence":
        raise ValueError(kind)
    # Stable fixtures include a genuine release; timed regression fixtures retain their full duration.
    if kind in ("tone", "vocal", "noise", "hihat", "sweep", "clipped"):
        signal[-rate//2:] = 0
    return np.asarray(signal, dtype="<f4")


def segments(spec):
    end = round(spec["seconds"]*60)
    points = {0, min(60, end), max(60, end-30), end}
    if spec["kind"] == "eighths":
        points.update((8*60, 20*60, 40*60))
    elif spec["kind"] == "silence_resume":
        points.update((120, 1920))
    elif spec["kind"] == "tempo_change":
        points.update((24*60, 32*60))
    elif spec["kind"] in ("gate", "gain_steps"):
        points.update(range(0, end, 60))
    points = sorted(p for p in points if 0 <= p <= end)
    return [{"start_frame": a, "end_frame": b} for a, b in zip(points, points[1:]) if b > a]


def generate(manifest, out):
    out.mkdir(parents=True, exist_ok=True)
    inventory = []
    for spec in specifications(manifest):
        pcm = synthesize(spec, manifest)
        path = out / (spec["id"] + ".f32")
        pcm.tofile(path)
        inventory.append({**spec, "pcm": path.name, "pcm_sha256": digest(path),
                          "sample_count": len(pcm), "frame_count": len(pcm)//500,
                          "segments": segments(spec)})
    write_json(out/"corpus.json", {"schema": manifest["schema"],
                                   "manifest_sha256": digest(MANIFEST),
                                   "canonical": manifest["canonical"],
                                   "fixtures": inventory})
    return inventory


def generate_adaptation(manifest, out):
    inventory = []
    spec = {"kind": "vocal", "seconds": 4, "rms_dbfs": -24}
    for rate in (44100, 48000):
        left = synthesize(spec, manifest, rate)
        t = np.arange(len(left))/rate
        right = 0.61*left + 0.03*np.sin(2*np.pi*3071*t)
        for channels in (1, 2):
            samples = left[:, None] if channels == 1 else np.stack((left, right), axis=1)
            for dtype in ("<i2", "<f4"):
                tag = f"adapt-{rate}-{channels}-{'s16' if dtype == '<i2' else 'f32'}"
                encoded = (np.clip(samples, -1, 1)*32767).astype(dtype) if dtype == "<i2" else samples.astype(dtype)
                pcm = out/(tag+".pcm")
                encoded.tofile(pcm)
                decoded = encoded.astype(np.float32)
                if dtype == "<i2":
                    decoded /= 32768
                mono = out/(tag+"-mono.f32")
                decoded.mean(axis=1).astype("<f4").tofile(mono)
                size = pcm.stat().st_size
                pattern = [1, 7, 997, 31, 8193, 3, 4096, 211]
                chunks, consumed = [], 0
                while consumed < size:
                    count = min(pattern[len(chunks) % len(pattern)], size-consumed)
                    chunks.append(count)
                    consumed += count
                inventory.append({"id": tag, "sample_rate": rate, "channels": channels,
                                  "dtype": dtype, "pcm": pcm.name, "pcm_sha256": digest(pcm),
                                  "mono_before_resample": mono.name, "mono_sha256": digest(mono),
                                  "irregular_packet_bytes": chunks,
                                  "regular_packet_bytes": round(rate/60)*channels*np.dtype(dtype).itemsize,
                                  "source_samples_per_channel": len(left),
                                  "poison_probe": ({"insert_before_packet": 5,
                                                    "complete_source_frames": round(rate/60),
                                                    "encoding": "Float32 NaN followed by finite samples"}
                                                   if dtype == "<f4" else None)})
    write_json(out/"adaptation.json", {
        "fixtures": inventory, "expected": "Packetization does not change candidate output bytes or sample times.",
        "resampler_alignment": "Determine real startup latency from native consumed/generated counts, never discard unmatched input.",
        "reference_divergence": "LedFx callback resamples each packet by packet-length ratio and drops shortened buffers; this is not the adaptation oracle.",
        "poison_probe": "Inject one whole nonfinite Float32 block, consume once, compare subsequent valid output with the same run minus that block."})


class OfflineStream:
    """The capture boundary only. Analysis, callbacks and effect code are real."""
    def __init__(self, **kwargs):
        self.samplerate = kwargs["samplerate"]

    def start(self):
        pass

    def stop(self):
        pass

    def close(self):
        pass


def provenance(reference, manifest):
    result = {"revision": subprocess.check_output(["git", "-C", str(reference), "rev-parse", "HEAD"], text=True).strip()}
    if result["revision"] != manifest["reference_revision"]:
        raise RuntimeError("Reference revision differs from frozen pin")
    result["git_status"] = subprocess.check_output(["git", "-C", str(reference), "status", "--porcelain=v1", "--untracked-files=all"], text=True)
    if result["git_status"]:
        raise RuntimeError("Reference checkout must be clean")
    result["source_hashes"] = {str(p): digest(reference/p) for p in (
        "ledfx/effects/audio.py", "ledfx/effects/melbank.py", "ledfx/effects/math.py",
        "ledfx/effects/__init__.py", "ledfx/effects/gradient.py",
        "ledfx/effects/spectrum.py", "ledfx/effects/energy.py",
        "ledfx/effects/scroll.py", "ledfx/effects/wavelength.py",
        "ledfx/effects/strobe.py", "uv.lock", "LICENSE.txt")}
    result["python"] = sys.version
    result["executable"] = sys.executable
    result["machine"] = platform.uname()._asdict()
    result["harness_hashes"] = {str(path): digest(path) for path in (
        Path(__file__), MANIFEST, Path(__file__).with_name("audio_reference_compare.py"))}
    result["dependencies"] = sorted(
        [{"name": d.metadata["Name"], "version": d.version} for d in importlib.metadata.distributions()],
        key=lambda d: d["name"].lower())
    result["native"] = []
    for module_name in ("aubio", "samplerate", "numpy", "_sounddevice_data"):
        module = __import__(module_name)
        root = Path(module.__file__).parent
        paths = ({Path(module.__file__)} if str(module.__file__).endswith(".so")
                 else set(root.rglob("*.so")) | set(root.rglob("*.dylib")))
        for path in sorted(paths):
            libraries = subprocess.run(["otool", "-L", str(path)], capture_output=True, text=True)
            result["native"].append({"path": str(path), "sha256": digest(path),
                                     "linked_libraries": libraries.stdout})
    return result


def run_reference(reference, manifest, out, only, correct_bank_buffer_alias=False,
                  effect_palette=None, correct_spectrum_history=False, corpus=None,
                  correct_effect_filter_state=False):
    sys.path.insert(0, str(reference.resolve()))
    import aubio
    import ledfx.effects.audio as audio_module
    from ledfx.effects import Effect
    from ledfx.effects.energy import EnergyAudioEffect
    from ledfx.effects.math import ExpFilter
    from ledfx.effects.melbank import Melbank
    from ledfx.effects.scroll import ScrollAudioEffect
    from ledfx.effects.spectrum import SpectrumAudioEffect
    from ledfx.effects.strobe import Strobe
    from ledfx.effects.wavelength import WavelengthAudioEffect

    metadata = provenance(reference, manifest)
    oracle = copy.deepcopy(BANK_BUFFER_ORACLE) if correct_bank_buffer_alias else {"id": "external-ledfx-original"}
    if correct_spectrum_history:
        oracle["effect_history"] = {
            "id": "spectrum-owned-history-v1",
            "boundary": "Copy Spectrum._prev_y after the actual callback/render. "
                        "The next producer callback cannot overwrite its prior-frame input.",
            "original_preserved": "reference-distinct-v1 contains the width-24 alias failure.",
        }
    if correct_effect_filter_state:
        oracle["effect_filter_state"] = {
            "id": "effect-owned-filter-state-v1",
            "boundary": "Copy Spectrum._b_filter.value and Energy._p_filter.value after rendering. "
                        "Their first input aliases mutable public bank/pixel buffers otherwise.",
            "unchanged": "Actual reference effect/filter functions, parameters and callback order.",
        }
    metadata["oracle"] = oracle
    if corpus:
        out.mkdir(parents=True, exist_ok=True)
        inventory = [spec for spec in json.loads(corpus.read_text())["fixtures"]
                     if not only or spec["id"] in only]
        if not inventory:
            raise ValueError("No matching fixtures; refusing empty reference execution")
        pcm_directory = corpus.parent
        write_json(out/"corpus.json", {"fixtures": inventory, "canonical": manifest["canonical"]})
    else:
        inventory = generate(manifest, out)
        generate_adaptation(manifest, out)
        pcm_directory = out
    write_json(out/"provenance.json", metadata)
    write_json(out/"frozen-manifest.json", manifest)
    classes = dict(zip(manifest["effects"], (SpectrumAudioEffect, EnergyAudioEffect,
                                             ScrollAudioEffect, WavelengthAudioEffect, Strobe)))
    clock = [manifest["canonical"]["clock_origin_s"]]
    device = {"name": "synthetic", "hostapi": 0, "default_samplerate": 30000,
              "max_input_channels": 1, "max_output_channels": 0}
    source_class = audio_module.AudioInputSource
    events = SimpleNamespace(add_listener=lambda *a, **k: None, fire_event=lambda *a, **k: None)
    summaries = []
    with ExitStack() as patches:
        if correct_bank_buffer_alias:
            patches.enter_context(patch.object(Melbank, "__call__", distinct_bank_outputs(Melbank.__call__)))
        for name, value in {
            "query_devices": lambda: [device],
            "query_hostapis": lambda: [{"name": "OFFLINE"}],
            "valid_device_indexes": lambda: [0],
            "input_devices": lambda: {0: "OFFLINE: synthetic"},
            "default_device_index": lambda: 0,
        }.items():
            patches.enter_context(patch.object(source_class, name, staticmethod(value)))
        patches.enter_context(patch.object(source_class, "_persist_config", lambda self: False))
        patches.enter_context(patch.object(audio_module.sd, "InputStream", OfflineStream))
        patches.enter_context(patch.object(audio_module, "save_config", side_effect=RuntimeError("Offline config write forbidden")))
        patches.enter_context(patch("time.time", lambda: clock[0]))
        patches.enter_context(patch("timeit.default_timer", lambda: clock[0]))
        for spec in inventory:
            if only and spec["id"] not in only:
                continue
            random.seed(manifest["seed"])
            np.random.seed(manifest["seed"])
            clock[0] = manifest["canonical"]["clock_origin_s"]
            source_class._audio_stream_active = False
            source_class._last_active = None
            source_class._last_device_name = None
            source_class._volume_filter = ExpFilter(-90, alpha_decay=0.99, alpha_rise=0.99)
            host = SimpleNamespace(config={"audio": copy.deepcopy(manifest["reference_audio_config"]),
                                           "melbanks": copy.deepcopy(manifest["reference_bank_config"]),
                                           "sendspin_always_on": False},
                                   events=events, dev_enabled=lambda: False, audio=None)
            audio = audio_module.AudioAnalysisSource(host, host.config["audio"])
            host.audio = audio
            pcm = np.fromfile(pcm_directory/spec["pcm"], dtype="<f4")
            effects = []
            if spec["id"] in manifest["effect_fixtures"]:
                for name, cls in classes.items():
                    for width, height in manifest["layouts"]:
                        virtual = SimpleNamespace(effective_pixel_count=width*height,
                                                  frequency_range=SimpleNamespace(min=20, max=15000), id="offline")
                        config = copy.deepcopy(manifest["effect_transforms"])
                        if effect_palette and name in ("energy", "scroll"):
                            config.update(zip(("color_lows", "color_mids", "color_high"), effect_palette))
                        if effect_palette and name in ("wavelength", "strobe"):
                            colors = [tuple(bytes.fromhex(color.lstrip("#"))) for color in effect_palette]
                            config["gradient"] = "linear-gradient(90deg, " + ", ".join(
                                f"rgb{color} {index*50}%" for index, color in enumerate(colors)) + ")"
                        effect = cls(host, config)
                        effect.activate(virtual)
                        effects.append((name, width, height, effect))
            processors = audio.melbanks.melbank_processors
            header = {"kind": "header", "schema": manifest["schema"], "producer": "external-ledfx",
                      "oracle": oracle,
                      "fixture": spec["id"], "pcm_sha256": spec["pcm_sha256"],
                      "frame_count": spec["frame_count"], "segments": spec["segments"],
                      "audio_config": audio._config, "bank_config": audio.melbanks.melbanks_config,
                      "reference_revision": metadata["revision"],
                      "tempo_delay_samples": audio._tempo.get_delay(),
                      "onset_delay_samples": audio._onset.get_delay(),
                      "event_alignment_frames": 0, "phase_alignment_frames": 0,
                      "power_indices": audio.freq_mel_indexes, "kick_stop_index": audio.beat_max_mel_index,
                      "coefficients": [p.filterbank.get_coeffs().copy().tolist() for p in processors],
                      "effects": [{"name": name, "width": w, "height": h, "config": e.config,
                                   "palette": effect_palette,
                                   "selected_bank": e._selected_melbank,
                                   "selected_min": e._melbank_min_idx, "selected_max": e._melbank_max_idx}
                                  for name, w, h, e in effects]}
            if effects:
                write_json(out/(spec["id"]+".header.json"), header)
            counters = dict(onset=0, tempo=0, kick=0, bar_wrap=0)
            previous_bar = 0
            tempos = []
            event_frames = {key: [] for key in counters}
            gate_count = 0
            alias_count = 0
            trace = out/(spec["id"]+".jsonl.gz")
            with gzip.open(trace, "wt", compresslevel=3) as output:
                output.write(json.dumps(header, allow_nan=False, sort_keys=True)+"\n")
                for frame, block in enumerate(pcm.reshape(-1, 500)):
                    clock[0] = manifest["canonical"]["clock_origin_s"]+(frame+1)/60
                    # The real callback modifies its input buffer when removing NaNs.
                    audio._audio_sample_callback(block.copy(), 500, None, None)
                    gate = audio.volume() > audio._config["min_volume"]
                    gate_count += gate
                    current_bar = audio.beat_counter
                    flags = {"onset": audio.onset(), "tempo": audio.bpm_beat_now(),
                             "kick": audio.volume_beat_now(),
                             "bar_wrap": current_bar == 0 and previous_bar == 3 and audio.bpm_beat_now()}
                    previous_bar = current_bar
                    for key, fired in flags.items():
                        counters[key] += int(fired)
                        if fired:
                            event_frames[key].append(frame)
                    record = {"kind": "frame", "fixture": spec["id"], "frame": frame,
                              "sample_end": (frame+1)*500, "time_s": (frame+1)/60,
                              "gate": bool(gate), "volume_raw": float(audio.volume(False)),
                              "volume_filtered": float(audio.volume()),
                              "raw_rms": float(np.sqrt(np.mean(block.astype(float)**2))),
                              "preemphasis": audio.audio_sample().copy().tolist() if gate else None,
                              "fft": audio.frequency_domain().norm.copy().tolist(),
                              "banks": [{"centers_hz": p.melbank_frequencies.tolist(),
                                         "processed": audio.melbanks.melbanks[i].copy().tolist(),
                                         "novelty": audio.melbanks.melbanks_filtered[i].copy().tolist(),
                                         "raw_triangles": p.filterbank(audio.frequency_domain()).copy().tolist(),
                                         "gain": None if p.mel_gain.value is None else float(p.mel_gain.value)}
                                        for i, p in enumerate(processors)],
                              "powers_raw": audio.freq_power_raw.copy().tolist(),
                              "powers": audio.freq_power_filter.value.copy().tolist(),
                              "pitch_midi": float(audio.pitch()),
                              "tempo_bpm": float(audio._tempo.get_bpm()),
                              "tempo_confidence": float(audio._tempo.get_confidence()),
                              "beat_phase": float(audio.beat_oscillator()),
                              "bar_phase": float(audio.bar_oscillator()/4),
                              "events": counters.copy(), "effects": []}
                    tempos.append(record["tempo_bpm"])
                    for name, width, height, effect in effects:
                        effect._render()
                        pre = effect.pixels.copy()/255
                        transformed = effect.get_pixels().copy()/255
                        record["effects"].append({"name": name, "width": width, "height": height,
                                                  "pre": pre.tolist(), "transformed": transformed.tolist(),
                                                  "final": np.clip(transformed, 0, 1).tolist()})
                        if name == "spectrum" and width == 24 and frame > 0 and gate:
                            alias_count += int(np.shares_memory(effect._prev_y, audio.melbanks.melbanks[2]))
                        if name == "spectrum" and correct_spectrum_history:
                            effect._prev_y = effect._prev_y.copy()
                        if correct_effect_filter_state and name in ("spectrum", "energy"):
                            state = effect._b_filter if name == "spectrum" else effect._p_filter
                            if state.value is not None:
                                state.value = state.value.copy()
                    output.write(json.dumps(record, allow_nan=False, sort_keys=True, separators=(",", ":"))+"\n")
            tempos = np.asarray(tempos)
            summary = {"fixture": spec["id"], "frames": len(tempos), "gate_open_frames": int(gate_count),
                       "event_frames": event_frames, "trace_sha256": digest(trace),
                       "tempo_final": float(tempos[-1]), "tempo_median_after_8s":
                       float(np.median(tempos[480:])) if len(tempos) > 480 else None,
                       "spectrum_alias_frames_width24": alias_count}
            summary["spectrum_alias_observation"] = "After render, before optional caller-history copy"
            if spec["kind"] == "eighths":
                summary["tempo_segments"] = []
                for start, end in ((8,20), (20,40), (40,60)):
                    values = tempos[start*60:end*60]
                    summary["tempo_segments"].append({"start_s": start, "end_s": end,
                        "min": float(values.min()), "max": float(values.max()), "median": float(np.median(values)),
                        "within_1bpm_s": float(np.sum(np.abs(values-spec["bpm"]) <= 1)/60),
                        "double_octave_s": float(np.sum(np.abs(values-2*spec["bpm"]) <= 2)/60)})
                write_json(out/"tempo-110-eighths-summary.json", summary)
            if spec["id"] == "silence_30s_resume":
                # No callback means no reference update. This intentionally records
                # the upstream stale-source defect, not a QLC acceptance target.
                before = {"volume": audio.volume(), "events": counters.copy(),
                          "powers": audio.freq_power_filter.value.copy().tolist()}
                clock[0] += 0.3
                after = {"volume": audio.volume(), "events": counters.copy(),
                         "powers": audio.freq_power_filter.value.copy().tolist()}
                write_json(out/"no-callback-divergence.json", {"elapsed_without_callbacks_s": 0.3,
                    "before": before, "after": after, "reference_stale": before == after,
                    "candidate_requirement": "Publish unavailable/reset within 250ms; do not reproduce reference freeze."})
            summaries.append(summary)
            write_json(out/"summary.json", summaries)
            print(json.dumps({k: v for k, v in summary.items() if k != "event_frames"}), flush=True)
            for _, _, _, effect in effects:
                effect._active = False
                Effect.deactivate(effect)
            audio._callbacks.clear()
            audio._invalidate_caches()
            # Bypass unsubscribe shutdown timers, not DSP. The stream is our inert stub.
            source_class._audio_stream_active = False
            source_class._stream = None
    final_status = subprocess.check_output(["git", "-C", str(reference), "status", "--porcelain=v1", "--untracked-files=all"], text=True)
    if final_status:
        raise RuntimeError("Reference checkout changed during offline execution")
    write_json(out/"reference-exit.json", {"git_status": final_status, "fixtures": len(summaries),
                                          "frames": sum(s["frames"] for s in summaries)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["generate", "reference"])
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--reference", default=Path("../ledfx"), type=Path)
    parser.add_argument("--only", nargs="+")
    parser.add_argument("--corpus", type=Path, help="Read immutable existing PCM instead of regenerating the corpus")
    parser.add_argument("--effect-palette", nargs=3, help="Three matched #rrggbb stops for effect comparison")
    parser.add_argument("--effect-blur", type=float, help="Explicit matched effect Gaussian sigma")
    parser.add_argument("--effect-brightness", type=float, help="Explicit matched effect brightness")
    parser.add_argument("--effect-mirror", choices=("on", "off"), help="Explicit matched strip mirror")
    parser.add_argument("--correct-spectrum-history", action="store_true",
                        help="Named caller-history boundary oracle; preserves actual Spectrum code")
    parser.add_argument("--correct-effect-filter-state", action="store_true",
                        help="Isolate first-input filter aliases in Spectrum/Energy at the caller boundary")
    parser.add_argument("--correct-bank-buffer-alias", action="store_true",
                        help="Named alternate oracle; only isolate Melbank output buffers. "
                             "Write to a separate directory, never overwrite original traces.")
    args = parser.parse_args()
    if args.out.is_absolute():
        parser.error("--out must be relative to the working directory")
    manifest = json.loads(MANIFEST.read_text())
    for key in ("blur", "brightness", "mirror"):
        value = getattr(args, "effect_" + key)
        if value is not None:
            manifest["effect_transforms"][key] = value == "on" if key == "mirror" else value
    if args.correct_bank_buffer_alias or args.correct_spectrum_history or args.correct_effect_filter_state or args.effect_palette:
        if args.mode != "reference":
            parser.error("--correct-bank-buffer-alias requires reference mode")
        if any(args.out.glob("*.jsonl*")):
            parser.error("Alternate oracle requires a new output directory")
    if args.mode == "generate":
        fixtures = generate(manifest, args.out)
        generate_adaptation(manifest, args.out)
        print(json.dumps({"fixtures": len(fixtures), "adaptation": 8}))
    else:
        if os.environ.get("PYTHONDONTWRITEBYTECODE") != "1":
            parser.error("Set PYTHONDONTWRITEBYTECODE=1 to leave the external checkout untouched")
        run_reference(args.reference, manifest, args.out, args.only, args.correct_bank_buffer_alias,
                      args.effect_palette, args.correct_spectrum_history, args.corpus,
                      args.correct_effect_filter_state)


if __name__ == "__main__":
    main()
