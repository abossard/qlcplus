#!/usr/bin/env python3
"""Fail-closed comparison of external reference traces with production Qt output.

`self-test` checks the comparator, not product parity.
`compare --analysis-only` does not claim effect or runtime acceptance.
`replay` runs the supplied native command twice, with diagnostics off/on.
"""

import argparse
from collections import defaultdict
import copy
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from unittest.mock import patch

import numpy as np

MANIFEST = Path(__file__).with_name("audio_reference_manifest.json")


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, allow_nan=False)+"\n")


def rows(path):
    opener = gzip.open if str(path).endswith(".gz") else open
    with opener(path, "rt") as stream:
        for line in stream:
            value = json.loads(line)
            if not isinstance(value, dict):
                raise ValueError("Every trace row must be an object")
            yield value


def trace_path(directory, fixture):
    for suffix in (".jsonl.gz", ".jsonl"):
        path = directory/(fixture+suffix)
        if path.is_file():
            return path
    raise FileNotFoundError(f"No candidate trace for {fixture} in {directory}")


def match_events(reference, candidate, window):
    """Ordered one-to-one matching; duplicates cannot inflate recall."""
    r = c = matched = 0
    while r < len(reference) and c < len(candidate):
        if abs(reference[r]-candidate[c]) <= window:
            matched += 1
            r += 1
            c += 1
        elif candidate[c] < reference[r]-window:
            c += 1
        else:
            r += 1
    return {"reference": len(reference), "candidate": len(candidate), "matched": matched,
            "precision": matched/len(candidate) if candidate else (1 if not reference else 0),
            "recall": matched/len(reference) if reference else (1 if not candidate else 0)}


class Comparison:
    def __init__(self, tolerances):
        self.t = tolerances
        self.failures = []
        self.stats = {}
        self.event_frames = [defaultdict(list), defaultdict(list)]
        self.previous_counters = [{}, {}]
        self.tempo = [[], []]
        self.phase = [[], []]
        self.effect_observations = [defaultdict(list), defaultdict(list)]

    def fail(self, field, reason):
        if len(self.failures) < 100:
            self.failures.append({"field": field, "reason": reason})

    def exact(self, field, reference, candidate):
        if reference != candidate:
            self.fail(field, f"exact mismatch: reference={reference!r} candidate={candidate!r}")

    def numeric(self, field, reference, candidate, family, segment):
        r = np.asarray(reference, dtype=float)
        c = np.asarray(candidate, dtype=float)
        if r.shape != c.shape or r.size == 0:
            self.fail(field, f"shape mismatch or empty: {r.shape} != {c.shape}")
            return
        if not np.isfinite(r).all() or not np.isfinite(c).all():
            self.fail(field, "nonfinite numeric value")
            return
        error = np.abs(c-r)
        key = f"{segment}/{field}"
        stat = self.stats.setdefault(key, {"family": family, "count": 0, "square_error": 0.,
                                          "square_reference": 0., "max_error": 0., "outside": 0,
                                          "pixel_errors": []})
        stat["count"] += r.size
        stat["square_error"] += float(np.sum(error**2))
        stat["square_reference"] += float(np.sum(r**2))
        stat["max_error"] = max(stat["max_error"], float(np.max(error)))
        if family == "pcm":
            limit = self.t["pcm_fft_atol"] + self.t["pcm_fft_rtol"]*np.abs(r)
        elif family == "triangle":
            limit = self.t["triangle_atol"]
        else:
            limit = self.t["banks_powers_atol"]
        if family == "pixels":
            stat["pixel_errors"].extend(error.ravel().tolist())
        else:
            stat["outside"] += int(np.sum(error > limit))

    def frame(self, reference, candidate, segment, analysis_only):
        for key in ("kind", "fixture", "frame", "sample_end", "gate"):
            self.exact(key, reference[key], candidate[key])
        self.numeric("time_s", reference["time_s"], candidate["time_s"], "pcm", segment)
        for key in ("volume_raw", "volume_filtered", "raw_rms", "fft", "pitch_midi"):
            self.numeric(key, reference[key], candidate[key], "pcm", segment)
        if reference["preemphasis"] is None:
            self.exact("preemphasis", None, candidate["preemphasis"])
        else:
            self.numeric("preemphasis", reference["preemphasis"], candidate["preemphasis"], "pcm", segment)
        self.exact("banks.count", len(reference["banks"]), len(candidate["banks"]))
        for index, (r, c) in enumerate(zip(reference["banks"], candidate["banks"])):
            self.exact(f"banks.{index}.centers_hz", r["centers_hz"], c["centers_hz"])
            for key in ("processed", "novelty"):
                self.numeric(f"banks.{index}.{key}", r[key], c[key], "banks", segment)
            self.numeric(f"banks.{index}.raw_triangles", r["raw_triangles"], c["raw_triangles"], "pcm", segment)
            if r["gain"] is None:
                self.exact(f"banks.{index}.gain", None, c["gain"])
            else:
                self.numeric(f"banks.{index}.gain", r["gain"], c["gain"], "pcm", segment)
        for key in ("powers_raw", "powers"):
            self.numeric(key, reference[key], candidate[key], "banks", segment)
        for side, record in enumerate((reference, candidate)):
            tempo = record["tempo_bpm"]
            phase = (record["beat_phase"], record["bar_phase"])
            if not np.isfinite(tempo) or not np.isfinite(phase).all():
                self.fail(f"tempo_phase.{side}", "nonfinite tempo/phase")
            if not all(0 <= value < 1 for value in phase):
                self.fail(f"phase.{side}", "beat/bar phases must be normalized cycles")
            self.tempo[side].append(tempo)
            self.phase[side].append(phase)
            for key in ("onset", "tempo", "kick", "bar_wrap"):
                counter = record["events"][key]
                previous = self.previous_counters[side].get(key, 0)
                if type(counter) is not int or not previous <= counter <= previous+1:
                    self.fail(f"events.{side}.{key}", "counter must increase by zero or one per analysis frame")
                elif counter > previous:
                    self.event_frames[side][key].append(record["frame"])
                self.previous_counters[side][key] = counter
        if not analysis_only:
            ref_effects, cand_effects = reference["effects"], candidate["effects"]
            self.exact("effects.count", len(ref_effects), len(cand_effects))
            for r, c in zip(ref_effects, cand_effects):
                for key in ("name", "width", "height"):
                    self.exact(f"effects.{key}", r[key], c[key])
                for key in ("pre", "transformed", "final"):
                    self.numeric(f"effects.{r['name']}.{r['width']}x{r['height']}.{key}",
                                 r[key], c[key], "pixels", segment)
                for side, effect in enumerate((r, c)):
                    pixels = np.asarray(effect["final"])
                    if pixels.shape != (effect["width"]*effect["height"], 3) or not np.isfinite(pixels).all():
                        self.fail(f"effects.{side}.final", "invalid final pixel shape/value")
                        continue
                    if np.any(pixels < 0) or np.any(pixels > 1):
                        self.fail(f"effects.{side}.final", "final pixels must be clipped normalized RGB")
                    key = f"{effect['name']}.{effect['width']}x{effect['height']}"
                    brightness = pixels.max(axis=1)
                    self.effect_observations[side][key].append([
                        float(brightness.max()), float(np.mean(brightness > 1/255)), int(np.argmax(brightness))])

    def finish(self, header):
        metrics = {}
        for name, stat in self.stats.items():
            count = stat["count"]
            rmse = (stat["square_error"]/count)**0.5
            norm = (stat["square_reference"]/count)**0.5
            # Zero-reference segments are an exact zero oracle, not an epsilon denominator.
            nrmse = rmse/norm if norm else (None if rmse else 0)
            metric = {"count": count, "max_error": stat["max_error"], "rmse": rmse, "nrmse": nrmse}
            if stat["family"] == "pixels":
                p99 = float(np.percentile(stat["pixel_errors"], 99))
                metric["p99"] = p99
                if rmse > self.t["linear_rgb_rmse"] or p99 > self.t["linear_rgb_p99"]:
                    self.fail(name, f"pixel rmse={rmse}, p99={p99}")
            elif stat["outside"]:
                self.fail(name, f"{stat['outside']} values exceed frozen numeric tolerance")
            if stat["family"] == "banks" and (nrmse is None or nrmse > self.t["banks_powers_normalized_rmse"]):
                self.fail(name, f"normalized RMSE={nrmse}")
            metrics[name] = metric
        event_alignment = header.get("event_alignment_frames", 0)
        phase_alignment = header.get("phase_alignment_frames", 0)
        if any(type(v) is not int for v in (event_alignment, phase_alignment)):
            raise ValueError("Alignments must be signed whole analysis frames")
        if (event_alignment or phase_alignment) and not header.get("alignment_explanation"):
            self.fail("alignment", "A nonzero native delay needs a recorded explanation")
        event_metrics = {}
        for key in ("onset", "tempo", "kick", "bar_wrap"):
            match = match_events(self.event_frames[0][key],
                                 [v+event_alignment for v in self.event_frames[1][key]],
                                 self.t["event_window_frames"])
            if match["precision"] < self.t["event_precision"] or match["recall"] < self.t["event_recall"]:
                self.fail(f"events.{key}", str(match))
            event_metrics[key] = match
        tempo_metrics = {}
        rtempo, ctempo = (np.asarray(v) for v in self.tempo)
        for side, tempo in zip(("reference", "candidate"), (rtempo, ctempo)):
            valid = np.flatnonzero(tempo > 0)
            tempo_metrics[side+"_acquisition_s"] = (int(valid[0])+1)/60 if len(valid) else None
        if not np.isfinite(rtempo).all() or not np.isfinite(ctempo).all():
            self.fail("tempo_bpm", "nonfinite tempo")
        if len(rtempo) == len(ctempo) and len(rtempo):
            valid = rtempo > 0
            self.exact("tempo_validity", valid.tolist(), (ctempo > 0).tolist())
            error = np.abs(ctempo[valid]-rtempo[valid])
            tempo_metrics["max_bpm_error"] = float(error.max()) if len(error) else 0
            tempo_metrics["extra_octave_s"] = float(np.sum(valid & (
                (np.abs(ctempo-2*rtempo) <= 2) | (np.abs(2*ctempo-rtempo) <= 2)))/60)
            if len(error) and error.max() > self.t["stable_bpm_error"]:
                self.fail("tempo_bpm", f"maximum error {error.max()} exceeds 1 BPM")
            if tempo_metrics["extra_octave_s"]:
                self.fail("tempo_bpm", "extra octave mismatch")
            rphase, cphase = (np.asarray(v) for v in self.phase)
            indices = np.arange(len(rtempo))
            cindices = indices-phase_alignment
            mask = valid & (cindices >= 0) & (cindices < len(ctempo))
            if np.any(mask):
                difference = np.abs(rphase[indices[mask]]-cphase[cindices[mask]]) % 1
                circular = np.minimum(difference, 1-difference)
                if not np.isfinite(circular).all() or circular.max() > self.t["phase_cycles"]:
                    self.fail("phase", "circular beat/bar error exceeds .03 cycles")
                tempo_metrics["max_phase_error"] = float(circular.max()) if np.isfinite(circular).all() else None
        temporal = {}
        for side, observations in zip(("reference", "candidate"), self.effect_observations):
            temporal[side] = {}
            for key, samples in observations.items():
                values = np.asarray(samples)
                amplitude, occupancy, location = values.T
                maximum = amplitude.max()
                low = np.flatnonzero(amplitude >= maximum*0.1) if maximum else []
                high = np.flatnonzero(amplitude >= maximum*0.9) if maximum else []
                tail = np.flatnonzero(amplitude[int(high[-1])+1:] <= maximum*0.1) if len(high) else []
                temporal[side][key] = {
                    "mean_nonblack_occupancy": float(occupancy.mean()),
                    "active_frames": int(np.sum(occupancy > 0)),
                    "attack_10_to_90_s": float((high[0]-low[0])/60) if len(low) and len(high) else None,
                    "release_90_to_10_s": float((tail[0]+1)/60) if len(tail) else None,
                    "peak_locations": location.astype(int).tolist(),
                    "brightness": amplitude.tolist()}
        return {"passed": not self.failures, "failures": self.failures, "metrics": metrics,
                "events": event_metrics, "tempo": tempo_metrics, "effect_temporal_metrics": temporal}


def compare_trace(reference_path, candidate_path, manifest, analysis_only, control=False):
    reference, candidate = rows(reference_path), rows(candidate_path)
    rhead, chead = next(reference), next(candidate)
    check = Comparison(manifest["tolerances"])
    for key in ("kind", "schema", "fixture", "pcm_sha256", "frame_count", "power_indices", "kick_stop_index"):
        check.exact(key, rhead[key], chead[key])
    if rhead["kind"] != "header" or rhead["schema"] != manifest["schema"]:
        raise ValueError("Unrecognized reference trace schema")
    if not control and chead["producer"] != "production-qlc-cpp":
        check.fail("producer", "Product comparison requires production-qlc-cpp, not reference/self output")
    if not analysis_only and not control:
        check.exact("effect_comparison", "faithful-reference-v1", chead.get("effect_comparison"))
        if not rhead.get("effects"):
            check.fail("effects", "No reference effects: select an effect fixture, not an empty comparison")
        check.exact("effect_parameters", rhead.get("effects"), chead.get("effects"))
        check.exact("effect_oracle", rhead.get("oracle"), chead.get("effect_oracle"))
    check.numeric("coefficients", rhead["coefficients"], chead["coefficients"], "triangle", "header")
    count = 0
    for count, r in enumerate(reference, start=1):
        c = next(candidate, None)
        if c is None:
            check.fail("frame_count", f"Candidate ended at frame {count-1}")
            break
        check.exact("reference.frame", count-1, r["frame"])
        segment = next((f"{s['start_frame']}:{s['end_frame']}" for s in rhead["segments"]
                        if s["start_frame"] <= count-1 < s["end_frame"]), None)
        if segment is None:
            raise ValueError("Reference frame outside declared comparison segments")
        check.frame(r, c, segment, analysis_only)
    if next(candidate, None) is not None:
        check.fail("frame_count", "Candidate has extra frames")
    check.exact("frame_count", rhead["frame_count"], count)
    if count == 0:
        check.fail("frame_count", "Empty fixture set is not comparison evidence")
    result = check.finish(chead)
    result.update(fixture=rhead["fixture"], compared_frames=count, comparator_control=control,
                  reference_oracle=rhead.get("oracle", {"id": "external-ledfx-original"}),
                  scope=["C0-3", "C0-5"] if analysis_only else ["C0-3", "C0-5", "C0-11"])
    return result


def compare_directories(args, manifest):
    corpus = json.loads((args.reference/"corpus.json").read_text())
    fixtures = [s["id"] for s in corpus["fixtures"] if not args.only or s["id"] in args.only]
    if not fixtures:
        raise ValueError("No matching fixtures; refusing empty comparison")
    results = []
    for fixture in fixtures:
        try:
            result = compare_trace(trace_path(args.reference, fixture),
                                   trace_path(args.candidate, fixture),
                                   manifest, args.analysis_only)
        except (KeyError, ValueError, StopIteration, FileNotFoundError) as error:
            result = {"fixture": fixture, "passed": False, "failures": [str(error)]}
        results.append(result)
        print(json.dumps({"fixture": fixture, "passed": result["passed"],
                          "failures": result["failures"][:3]}), flush=True)
    report = {"passed": all(r["passed"] for r in results), "fixtures": len(fixtures), "results": results}
    write_json(args.report, report)
    return 0 if report["passed"] else 1


def self_test(args, manifest):
    path = trace_path(args.reference, "tone_97")
    positive = compare_trace(path, path, manifest, True, control=True)
    assert positive["passed"], positive["failures"]
    source = rows(path)
    header, first = next(source), next(source)
    mutations = [
        ("gate", lambda f: f.update(gate=not f["gate"])),
        ("frame", lambda f: f.update(frame=f["frame"]+1)),
        ("bank_value", lambda f: f["banks"][2]["processed"].__setitem__(5, 0.4)),
        ("bank_center", lambda f: f["banks"][0]["centers_hz"].__setitem__(2, 999)),
        ("counter_replay", lambda f: f["events"].update(tempo=2)),
        ("nonfinite", lambda f: f.update(volume_raw=float("nan"))),
        ("fft_shape", lambda f: f.update(fft=f["fft"][:-1])),
    ]
    evidence = []
    for name, mutate in mutations:
        candidate = copy.deepcopy(first)
        mutate(candidate)
        check = Comparison(manifest["tolerances"])
        check.frame(first, candidate, "negative-control", True)
        report = check.finish(header)
        assert not report["passed"], name
        evidence.append({"mutation": name, "rejected": True, "failures": report["failures"][:2]})
    assert match_events([10, 20, 30], [11, 19, 30], 1)["recall"] == 1
    assert match_events([10, 20, 30], [10, 10, 10], 1)["matched"] == 1
    assert match_events([], [20], 1)["precision"] == 0
    fake_candidate = compare_trace(path, path, manifest, True)
    assert not fake_candidate["passed"] and any(f["field"] == "producer" for f in fake_candidate["failures"])
    effect_source = rows(trace_path(args.reference, "sweep"))
    effect_header, effect_frame = next(effect_source), next(effect_source)
    altered = copy.deepcopy(effect_frame)
    altered["effects"][0]["pre"] = (np.asarray(altered["effects"][0]["pre"])+0.1).tolist()
    effect_check = Comparison(manifest["tolerances"])
    effect_check.frame(effect_frame, altered, "pixel-negative-control", False)
    effect_result = effect_check.finish(effect_header)
    assert any(f["field"].endswith(".pre") for f in effect_result["failures"])
    evidence.append({"mutation": "real_effect_pixels", "rejected": True,
                     "failures": effect_result["failures"][:2]})
    report = {"passed": True, "scope": "Comparator controls only. No candidate parity.",
              "reference_self_control_frames": positive["compared_frames"],
              "negative_controls": evidence, "reference_as_candidate_rejected": True}
    if args.corrected_reference:
        report["known_bank_buffer_defects"] = []
        for fixture in ("quiet_vocal", "gain_45db", "tempo_110_eighths"):
            original = trace_path(args.reference, fixture)
            corrected = trace_path(args.corrected_reference, fixture)
            header = next(rows(corrected))
            assert header["oracle"]["id"] == "ledfx-distinct-melbank-outputs-v1"
            difference = compare_trace(original, corrected, manifest, True, control=True)
            assert not difference["passed"], f"{fixture}: original alias defect disappeared"
            affected = {"powers", "powers_raw"} | {
                f"banks.{bank}.{field}" for bank in range(3) for field in ("processed", "novelty")}
            assert all(f["field"].split("/")[-1] in affected for f in difference["failures"])
            assert all(metric["max_error"] == 0 for field, metric in difference["metrics"].items()
                       if field.split("/")[-1] not in affected), "Oracle changed an upstream DSP boundary"
            report["known_bank_buffer_defects"].append({
                "fixture": fixture, "original_sha256": hashlib.sha256(original.read_bytes()).hexdigest(),
                "replacement_sha256": hashlib.sha256(corrected.read_bytes()).hexdigest(),
                "original_vs_replacement_passed": False, "failures": difference["failures"],
                "unchanged_upstream_boundaries_exact": True, "oracle": header["oracle"]})
    report["runtime_controls"] = runtime_controls(manifest)
    write_json(args.report, report)
    print(json.dumps(report))
    return 0


def verify_reference(args, manifest):
    """Qualify recorded observations. This does not compare a QLC candidate."""
    corpus = json.loads((args.reference/"corpus.json").read_text())
    observations = []
    for spec in corpus["fixtures"]:
        trace = rows(trace_path(args.reference, spec["id"]))
        header = next(trace)
        assert header["producer"] == "external-ledfx"
        assert header["reference_revision"] == manifest["reference_revision"]
        expected_effects = len(manifest["effects"])*len(manifest["layouts"]) if spec["id"] in manifest["effect_fixtures"] else 0
        count = silence_frames = 0
        peaks = np.zeros(4)
        frozen_gain = None
        effect_maxima = defaultdict(list)
        difference_channels = defaultdict(float)
        for count, frame in enumerate(trace, start=1):
            assert frame["frame"] == count-1 and frame["sample_end"] == count*500
            assert abs(frame["time_s"]-count/60) < 1e-12
            fft = np.asarray(frame["fft"])
            banks = frame["banks"]
            assert fft.shape == (2049,) and np.isfinite(fft).all()
            assert len(banks) == 3
            for bank in banks:
                for key in ("centers_hz", "processed", "novelty", "raw_triangles"):
                    values = np.asarray(bank[key])
                    assert values.shape == (24,) and np.isfinite(values).all()
            if frame["gate"]:
                assert np.asarray(frame["preemphasis"]).shape == (500,)
                assert np.isfinite(frame["preemphasis"]).all()
            else:
                assert frame["preemphasis"] is None and not np.any(fft)
                assert all(not np.any(b[key]) for b in banks for key in ("processed", "novelty"))
            peaks = np.maximum(peaks, frame["powers"])
            if spec["id"] == "silence_30s_resume" and 120 <= frame["frame"] < 1920:
                assert not frame["gate"]
                gain = [b["gain"] for b in banks]
                if frozen_gain is None:
                    frozen_gain = gain
                assert gain == frozen_gain
                silence_frames += 1
            assert len(frame["effects"]) == expected_effects
            for effect in frame["effects"]:
                pixels = np.asarray(effect["final"])
                assert pixels.shape == (effect["width"]*effect["height"], 3)
                assert np.isfinite(pixels).all() and np.min(pixels) >= 0 and np.max(pixels) <= 1
                effect_maxima[effect["name"]].append(float(pixels.max()))
                if effect["name"] == "spectrum":
                    key = f"{effect['width']}x{effect['height']}"
                    difference_channels[key] = max(difference_channels[key], float(np.max(np.asarray(effect["pre"])[:, 1])))
        assert count == spec["frame_count"] and count > 0
        if spec["id"] == "silence_30s_resume":
            assert silence_frames == 1800
        if spec["id"] in ("vocal_formants", "sweep"):
            assert all(max(values) > 0 and len(set(values)) > 1 for values in effect_maxima.values())
        if spec["id"] == "sweep":
            assert difference_channels["24x1"] == 0 and difference_channels["37x1"] > 0
        observations.append({"fixture": spec["id"], "frames": count, "power_maxima": peaks.tolist(),
                             "events": frame["events"], "frozen_silence_frames": silence_frames,
                             "spectrum_difference_channel_max": dict(difference_channels)})
    assert len(observations) == 36
    report = {"passed": True, "scope": "Pinned reference fixture integrity only, no QLC parity.",
              "fixtures": len(observations), "frames": sum(o["frames"] for o in observations),
              "observations": observations}
    write_json(args.report, report)
    print(json.dumps({k: v for k, v in report.items() if k != "observations"}))
    return 0


def effect_controls(args, manifest):
    path = trace_path(args.reference, "sweep")
    reference = rows(path)
    header = next(reference)
    sample = next(frame for frame in reference if frame["frame"] == 30)
    positive = Comparison(manifest["tolerances"])
    positive.previous_counters = [sample["events"].copy(), sample["events"].copy()]
    positive.frame(sample, sample, "effect-control", False)
    assert positive.finish(header)["passed"]
    mutations = {
        "missing-effect": lambda r: r["effects"].pop(),
        "wrong-layout": lambda r: r["effects"][0].update(width=23),
        "pre-gain": lambda r: r["effects"][0].update(
            pre=(np.asarray(r["effects"][0]["pre"]) * .5).tolist()),
        "transformed-mirror": lambda r: r["effects"][0].update(
            transformed=np.roll(r["effects"][0]["transformed"], 5, axis=0).tolist()),
        "final-black": lambda r: r["effects"][0].update(
            final=np.zeros_like(r["effects"][0]["final"]).tolist()),
    }
    controls = []
    for name, mutate in mutations.items():
        candidate = copy.deepcopy(sample)
        mutate(candidate)
        check = Comparison(manifest["tolerances"])
        check.previous_counters = [sample["events"].copy(), sample["events"].copy()]
        check.frame(sample, candidate, "effect-control", False)
        result = check.finish(header)
        assert not result["passed"], name
        assert all("effects" in failure["field"] for failure in result["failures"]), result["failures"]
        controls.append({"mutation": name, "rejected": True, "failures": result["failures"][:2]})
    defects = []
    for directory, label, effects in (
        (args.original_reference, "Spectrum shared previous-bank history", {"spectrum"}),
        (args.filter_alias_reference, "Spectrum/Energy first-input filter alias", {"spectrum", "energy"}),
    ):
        original = rows(trace_path(directory, "sweep"))
        corrected = rows(path)
        next(original), next(corrected)
        differences = defaultdict(float)
        for before, after in zip(original, corrected):
            for b, a in zip(before["effects"], after["effects"]):
                if b["name"] in effects:
                    key = f"{b['name']}/{b['width']}x{b['height']}"
                    differences[key] = max(differences[key],
                        float(np.max(np.abs(np.asarray(b["pre"]) - a["pre"]))))
        assert any(error > manifest["tolerances"]["linear_rgb_p99"] for error in differences.values())
        defects.append({"boundary": label, "original_trace": str(trace_path(directory, "sweep")),
                        "observed_max_errors": dict(differences), "preserved_failure": True})
    report = {"passed": True, "scope": "Real effect comparator and preserved reference defect controls, not Qt parity",
              "negative_controls": controls, "known_effect_alias_defects": defects}
    write_json(args.report, report)
    print(json.dumps(report))
    return 0


def evaluate_runtime(path, diagnostics, manifest):
    records = rows(path)
    header = next(records)
    required = {"kind": "runtime-header", "producer": "production-qlc-cpp",
                "diagnostics": diagnostics, "profiles": [7, 29],
                "effects": manifest["replay"]["effects"], "duration_s": 600,
                "analysis_hz": 60, "render_hz": 50,
                "default_channel_retained": True, "producer_channels": 3}
    failures = []
    for key, expected in required.items():
        if header.get(key) != expected:
            failures.append(f"runtime header {key}: expected {expected!r}")
    records = list(records)
    data = [row for row in records if row.get("kind") == "render"]
    analyses = [row for row in records if row.get("kind") == "analysis"]
    if len(analyses) != 36000:
        failures.append(f"Expected 36000 production two-profile analyses; got {len(analyses)}")
    if len(data) != 30000:
        failures.append(f"Expected 30000 committed 50Hz render samples over 600s; got {len(data)}")
    if len(data)+len(analyses) != len(records):
        failures.append("Unknown runtime record kinds")
    keys = ("time_s", "render_ms", "blocking_ms", "pending_work",
            "rss_bytes", "allocations", "stale_age_ms", "audio_frame")
    values = {}
    for key in keys:
        values[key] = np.asarray([row[key] for row in data], dtype=float)
        if not len(values[key]) or not np.isfinite(values[key]).all():
            failures.append(f"Missing/nonfinite runtime values: {key}")
    analysis_ms = np.asarray([row["analysis_ms"] for row in analyses], dtype=float)
    analysis_clock = np.asarray([row["time_s"] for row in analyses], dtype=float)
    if not np.isfinite(analysis_ms).all() or np.any(analysis_ms < 0):
        failures.append("Invalid analysis durations")
    if not np.array_equal([row["frame"] for row in analyses], np.arange(36000)):
        failures.append("Missing or out-of-order canonical analysis frame")
    if len(analyses) == 36000 and not np.allclose(analysis_clock, (np.arange(36000)+1)/60, rtol=0, atol=1e-9):
        failures.append("Analysis source-time schedule differs from the exact ten-minute request")
    if not len(data) or not len(analyses) or not np.isfinite(analysis_ms).all() or any(
            not np.isfinite(value).all() for value in values.values()):
        return {"passed": False, "failures": failures}
    clock = values["time_s"]
    if len(data) == 30000 and not np.allclose(clock, (np.arange(30000)+1)/50, rtol=0, atol=1e-9):
        failures.append("Replay did not cover exactly ten minutes in ordered source time")
    steady = clock >= manifest["replay"]["warmup_s"]
    if not np.any(steady) or not np.any(analysis_clock >= manifest["replay"]["warmup_s"]):
        return {"passed": False, "failures": failures + ["Replay contains no post-warmup measurements"]}
    metrics = {}
    for key in keys[1:]:
        metrics[key] = {f"p{p}": float(np.percentile(values[key][steady], p)) for p in (50,95,99)}
    metrics["analysis_ms"] = {
        f"p{p}": float(np.percentile(analysis_ms[analysis_clock >= manifest["replay"]["warmup_s"]], p))
        for p in (50,95,99)}
    for key, limit in (("analysis_ms", manifest["tolerances"]["analysis_p99_ms"]),
                       ("blocking_ms", manifest["tolerances"]["blocking_p99_ms"])):
        if metrics[key]["p99"] >= limit:
            failures.append(f"{key} p99 does not satisfy <{limit}")
    if np.max(values["render_ms"][steady]) >= manifest["tolerances"]["four_effect_timer_ms"]:
        failures.append("Four-effect committed render missed the 20ms timer budget")
    if np.any(np.diff(values["audio_frame"]) <= 0):
        failures.append("Repeated/backward available audio frame during 50Hz rendering of 60Hz input")
    if not all(row["available"] for row in data):
        failures.append("Unavailable audio during canonical available-input replay")
    if np.max(values["pending_work"]) > header.get("pending_capacity", 0):
        failures.append("Pending work exceeded declared bounded handoff capacity")
    if not any(row.get("pending_observations", 0) > 0 for row in data):
        failures.append("No pending work observed during actual rendering")
    second_frames = np.asarray([row.get("second_audio_frame", 0) for row in data])
    if np.any(np.diff(second_frames) <= 0):
        failures.append("Repeated/backward second-profile frame")
    actual_starts = np.asarray([row.get("actual_start_s", 0) for row in data])
    producer_starts = np.asarray([row.get("actual_start_s", 0) for row in analyses])
    work_starts = np.asarray([row.get("work_start_s", -1) for row in data], dtype=float)
    work_ends = np.asarray([row.get("work_end_s", -1) for row in data], dtype=float)
    producer_ends = np.asarray([row.get("work_end_s", -1) for row in analyses], dtype=float)
    observed_work_end = None
    if (not all(np.isfinite(v).all() for v in (work_starts, work_ends, producer_ends))
            or np.any(work_starts < actual_starts) or np.any(work_ends < work_starts)
            or np.any(producer_ends < producer_starts)):
        failures.append("Missing or invalid measured workload completion timestamps")
    else:
        observed_work_end = float(max(work_ends[-1], producer_ends[-1]))
        if observed_work_end < 600:
            failures.append("Observed workload did not span the complete 600 seconds")
    render_end_age = np.asarray([row.get("stale_age_at_render_end_ms", -1) for row in data], dtype=float)
    if not np.isfinite(render_end_age).all() or np.any(render_end_age < 0):
        failures.append("Missing or invalid render-completion publication age")
    elif np.any(render_end_age > 250) or not all(row.get("fresh_at_render_end") is True for row in data):
        failures.append("Stale publication at render completion")
    for key in ("js_cpu_ms", "snapshot_ms", "deadline_lateness_ms", "analysis_backlog"):
        measured = np.asarray([row.get(key, -1) for row in data])
        if np.any(measured < 0) or not np.isfinite(measured).all():
            failures.append(f"Missing/nonfinite runtime attribution: {key}")
        metrics[key] = {f"p{p}": float(np.percentile(measured[steady], p)) for p in (50, 95, 99)}
        metrics[key]["max"] = float(np.max(measured))
    worst = data[int(np.argmax(values["render_ms"]))]
    metrics["render_ms"]["max"] = float(np.max(values["render_ms"]))
    # Separate minute windows show growth rather than hiding it behind one end-to-end average.
    minutes = [values["rss_bytes"][(clock >= start) & (clock < start+60)]
               for start in range(60, 600, 60)]
    memory_floors = [float(np.min(v)) for v in minutes if len(v)]
    queue_floors = [float(np.min(values["pending_work"][(clock >= start) & (clock < start+60)]))
                    for start in range(60, 600, 60)
                    if np.any((clock >= start) & (clock < start+60))]
    if len(memory_floors) > 2 and np.all(np.diff(memory_floors) >= 0) and memory_floors[-1] > memory_floors[0]:
        failures.append("Resident memory floor grows in every post-warmup minute")
    if len(queue_floors) > 2 and np.all(np.diff(queue_floors) >= 0) and queue_floors[-1] > queue_floors[0]:
        failures.append("Pending work floor grows in every post-warmup minute")
    return {"passed": not failures, "failures": failures, "metrics": metrics,
            "memory_minute_floors": memory_floors, "pending_minute_floors": queue_floors,
            "worst_render": worst, "actual_last_render_start_s": float(actual_starts[-1]),
            "actual_last_analysis_start_s": float(producer_starts[-1]),
            "analysis_count": len(analyses), "render_count": len(data),
            "missed_render_rounds": max(0, 30000 - len(data)),
            "observed_work_end_s": observed_work_end,
            "max_render_completion_age_ms": float(np.max(render_end_age)) if np.isfinite(render_end_age).all() else None}


def runtime_controls(manifest):
    """Synthetic validator checks, never candidate timing evidence."""
    header = {"kind": "runtime-header", "producer": "production-qlc-cpp",
              "diagnostics": False, "profiles": [7, 29], "effects": manifest["replay"]["effects"],
              "duration_s": 600, "analysis_hz": 60, "render_hz": 50,
              "default_channel_retained": True, "producer_channels": 3, "pending_capacity": 1}
    analyses = [{"kind": "analysis", "frame": i, "time_s": (i + 1) / 60,
                 "actual_start_s": (i + 1) / 60 + .000001, "analysis_ms": .2 + .1 * (i % 5)}
                for i in range(36000)]
    renders = [{"kind": "render", "time_s": (i + 1) / 50, "actual_start_s": (i + 1) / 50 + .003,
                "render_ms": .7 + .1 * (i % 7), "blocking_ms": .7 + .1 * (i % 7),
                "pending_work": i % 2, "pending_observations": i + 1,
                "rss_bytes": 1000000 + 1000 * ((i // 3000) % 2), "allocations": 1000 + i % 11,
                "stale_age_ms": 3 + i % 5, "audio_frame": (i + 1) * 6 // 5,
                "second_audio_frame": (i + 1) * 6 // 5, "available": True, "js_cpu_ms": .5,
                "snapshot_ms": .01, "deadline_lateness_ms": 0, "analysis_backlog": 0}
               for i in range(30000)]
    for row in analyses:
        row["work_end_s"] = row["actual_start_s"] + row["analysis_ms"] / 1000
    for row in renders:
        row["work_start_s"] = row["actual_start_s"]
        row["work_end_s"] = row["work_start_s"] + row["render_ms"] / 1000
        row["stale_age_at_render_end_ms"] = row["stale_age_ms"] + row["render_ms"]
        row["fresh_at_render_end"] = True
    cases = [
        ("healthy", None, None, None, None),
        ("missed_tick", "render", None, None, "Expected 30000"),
        ("short_replay", "short", None, None, "no post-warmup"),
        ("short_wall_duration", "fast", None, None, "Observed workload did not span"),
        ("invalid_completion_order", "last", "work_end_s", 0, "invalid measured workload"),
        ("stale_render_completion", "last", "stale_age_at_render_end_ms", 251, "Stale publication at render completion"),
        ("nonfinite_completion_age", "last", "stale_age_at_render_end_ms", float("nan"), "invalid render-completion publication age"),
        ("blocking_limit", "render", "blocking_ms", 5, "blocking_ms p99"),
        ("analysis_limit", "analysis", "analysis_ms", 8, "analysis_ms p99"),
        ("timer_limit", "last", "render_ms", 20, "20ms timer budget"),
        ("unobserved_pending", "render", "pending_observations", 0, "No pending work observed"),
        ("missing_default", "header", "default_channel_retained", False, "default_channel_retained"),
        ("repeated_frame", "last", "audio_frame", 1, "Repeated/backward available"),
        ("repeated_second_frame", "last", "second_audio_frame", 1, "Repeated/backward second"),
        ("unavailable", "last", "available", False, "Unavailable audio"),
        ("nonfinite", "last", "render_ms", float("nan"), "Missing/nonfinite runtime"),
    ]
    evidence = []
    for name, kind, field, value, reason in cases:
        h, a, r = dict(header), [dict(row) for row in analyses], [dict(row) for row in renders]
        if kind == "fast":
            for row in a:
                row["actual_start_s"] *= .9
                row["work_end_s"] = row["actual_start_s"] + row["analysis_ms"] / 1000
            for row in r:
                row["actual_start_s"] *= .9
                row["work_start_s"] = row["actual_start_s"]
                row["work_end_s"] = row["work_start_s"] + row["render_ms"] / 1000
        elif kind == "short":
            a, r = a[:60], r[:50]
        elif kind == "render" and field is None:
            r.pop()
        elif kind:
            selected = a if kind == "analysis" else r if kind == "render" else [h] if kind == "header" else [r[-1]]
            for row in selected:
                row[field] = value
        with patch.dict(globals(), {"rows": lambda _: iter([h, *a, *r])}):
            result = evaluate_runtime("synthetic-validator-control", False, manifest)
        assert result["passed"] == (name == "healthy"), name
        if reason:
            assert any(reason in failure for failure in result["failures"]), (name, result)
        if name == "missed_tick":
            assert result["render_count"] == 29999 and "metrics" in result
            assert result["observed_work_end_s"] >= 600
            assert not any("600 seconds" in failure for failure in result["failures"])
        evidence.append({"case": name, "expected_outcome": True, "failures": result["failures"]})
    return evidence


def replay(args, manifest):
    if not args.command:
        raise ValueError("Supply a production Qt runner command after --command")
    args.out.mkdir(parents=True, exist_ok=True)
    reports = []
    for diagnostics in (False, True):
        label = "diagnostics-on" if diagnostics else "diagnostics-off"
        request = args.out/(label+"-request.json")
        trace = args.out/(label+".jsonl")
        corpus = json.loads((args.reference/"corpus.json").read_text())
        fixture = next(s for s in corpus["fixtures"] if s["id"] == manifest["replay"]["fixture"])
        write_json(request, {"kind": "runtime-request", **manifest["replay"], "diagnostics": diagnostics,
                             "pcm": str((args.reference/fixture["pcm"]).resolve()),
                             "pcm_sha256": fixture["pcm_sha256"], "canonical": manifest["canonical"]})
        command = [str(part).replace("{request}", str(request)).replace("{trace}", str(trace))
                   for part in args.command]
        if not any("{request}" in part for part in args.command) or not any("{trace}" in part for part in args.command):
            raise ValueError("Command must contain {request} and {trace} placeholders")
        start = time.monotonic()
        run = subprocess.run(command, capture_output=True, text=True, timeout=900,
                             env={**os.environ, "QTEST_FUNCTION_TIMEOUT": "900000"})
        (args.out/(label+".log")).write_text(run.stdout+"\n"+run.stderr)
        result = {"diagnostics": diagnostics, "command": command, "exit_code": run.returncode,
                  "wall_seconds": time.monotonic()-start}
        if run.returncode == 0 and trace.exists():
            result.update(evaluate_runtime(trace, diagnostics, manifest))
        else:
            result.update(passed=False, failures=["Native production replay failed or emitted no metrics"])
        reports.append(result)
    report = {"passed": all(r["passed"] for r in reports), "runs": reports,
              "scope": "C0-12 candidate only. Requires separately recorded baseline metrics."}
    write_json(args.out/"runtime-report.json", report)
    print(json.dumps(report))
    return 0 if report["passed"] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    compare = sub.add_parser("compare")
    compare.add_argument("--reference", required=True, type=Path)
    compare.add_argument("--candidate", required=True, type=Path)
    compare.add_argument("--report", required=True, type=Path)
    compare.add_argument("--only", nargs="+")
    compare.add_argument("--analysis-only", action="store_true")
    controls = sub.add_parser("self-test")
    controls.add_argument("--reference", required=True, type=Path)
    controls.add_argument("--report", required=True, type=Path)
    controls.add_argument("--corrected-reference", type=Path,
                          help="Also retain original-vs-corrected bank-buffer defects as negative controls")
    integrity = sub.add_parser("verify-reference")
    integrity.add_argument("--reference", required=True, type=Path)
    integrity.add_argument("--report", required=True, type=Path)
    effects = sub.add_parser("effect-controls")
    effects.add_argument("--reference", required=True, type=Path)
    effects.add_argument("--original-reference", required=True, type=Path)
    effects.add_argument("--filter-alias-reference", required=True, type=Path)
    effects.add_argument("--report", required=True, type=Path)
    runtime = sub.add_parser("replay")
    runtime.add_argument("--reference", required=True, type=Path)
    runtime.add_argument("--out", required=True, type=Path)
    runtime.add_argument("--command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    for name in ("report", "out"):
        if getattr(args, name, None) is not None and getattr(args, name).is_absolute():
            parser.error(f"--{name} must be relative to cwd")
    manifest = json.loads(MANIFEST.read_text())
    try:
        if args.mode == "compare":
            return compare_directories(args, manifest)
        if args.mode == "self-test":
            return self_test(args, manifest)
        if args.mode == "verify-reference":
            return verify_reference(args, manifest)
        if args.mode == "effect-controls":
            return effect_controls(args, manifest)
        return replay(args, manifest)
    except (ValueError, KeyError, StopIteration, FileNotFoundError) as error:
        print(json.dumps({"passed": False, "error": str(error)}), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
