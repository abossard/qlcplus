import copy
import os
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

import generate as gallery


class GalleryValidationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = Path(os.environ.get(
            "HUE_GALLERY_TEST_DIR", "build/hue-gallery-test-artifacts")).resolve()
        cls.work.mkdir(parents=True, exist_ok=True)

    def test_palette_acceptance(self):
        root = self.work / "palette"
        scripts = root / "resources/huescripts"
        scripts.mkdir(parents=True, exist_ok=True)
        inventory = [
            {"file": f"effect{n}.js", "name": f"Effect {n}", "acceptColors": n,
             "audio": False, "properties": []}
            for n in range(6)
        ]
        (scripts / "CMakeLists.txt").write_text(
            "set(SCRIPT_FILES\n" + "".join(f"    effect{n}.js\n" for n in range(6)) + ")\n")
        with patch.object(gallery, "ROOT", root):
            entries = gallery.plan_entries(inventory)
        self.assertEqual([e["acceptedColorCount"] for e in entries], list(range(6)))
        self.assertTrue(all(len(e["palette"]) == 5 for e in entries))
        self.assertIn("no palette", entries[0]["description"])
        for count in range(1, 5):
            with self.subTest(count=count):
                self.assertIn(f"first {count} of 5 assigned colors", entries[count]["description"])
        self.assertNotIn("first 5 of", entries[5]["description"])

    @staticmethod
    def report():
        return {
            "audio": False, "warmupMs": 4000, "wallMs": 7980,
            "renderIntervalMs": 20, "frames": 40,
            "frameWallTimesMs": [4000 + i * 100 for i in range(40)],
        }

    def test_measured_realtime_pacing(self):
        entry = {"id": "timing", "durationMs": 4000, "fps": 10, "playbackScale": 1}
        result = gallery.validate_pacing(entry, self.report())
        self.assertEqual(result["clock"], "wall")
        self.assertEqual(result["expectedWallMs"], 7980)
        self.assertEqual(result["measuredWarmupMs"], 4000)
        self.assertEqual(result["measuredCaptureSpanMs"], 3900)
        self.assertEqual(result["maximumFrameTargetErrorMs"], 0)
        self.assertEqual(result["frameTargetToleranceMs"], 50)
        jitter = self.report()
        jitter["wallMs"] += 8
        jitter["frameWallTimesMs"] = [t + 4 + (i % 3) for i, t in enumerate(jitter["frameWallTimesMs"])]
        gallery.validate_pacing(entry, jitter)

    def test_known_palette_mode_usage(self):
        root = self.work / "palette-modes"
        scripts = root / "resources/huescripts"
        scripts.mkdir(parents=True, exist_ok=True)
        modes = {
            "audiomelt": "LedFx Melt", "audiomeltsparkle": "LedFx Melt and Sparkle",
            "audioblocks": "LedFx Block Reflections", "audiocrawler": "LedFx Crawler",
            "audiolava": "LedFx Lava Lamp", "audiowater": "LedFx Water", "audiofire": "LedFx Fire",
        }
        inventory = [
            {"file": stem + ".js", "name": stem, "acceptColors": 3, "audio": True,
             "properties": [{"name": "mode", "choices": ["Artistic", mode]}]}
            for stem, mode in modes.items()
        ]
        (scripts / "CMakeLists.txt").write_text(
            "set(SCRIPT_FILES\n" + "".join(f"    {stem}.js\n" for stem in modes) + ")\n")
        with patch.object(gallery, "ROOT", root):
            entries = gallery.plan_entries(inventory)
        for entry in entries:
            with self.subTest(effect=entry["effectName"], mode=entry["modeLabel"]):
                self.assertEqual(entry["acceptedColorCount"], 3)
                self.assertEqual(entry["palette"], gallery.PALETTE)
                self.assertIn("ignores the injected palette" if entry["modeLabel"] == "Artistic"
                              else "uses the first 3 assigned colors", entry["description"])

    def test_rain_pulse_description_and_flash_warning(self):
        root = self.work / "rain-pulse"
        scripts = root / "resources/huescripts"
        scripts.mkdir(parents=True, exist_ok=True)
        (scripts / "CMakeLists.txt").write_text("set(SCRIPT_FILES\n    audiopuddles.js\n)\n")
        inventory = [{
            "file": "audiopuddles.js", "name": "Audio Puddles", "acceptColors": 5,
            "audio": True, "properties": [{"name": "mode", "choices": ["Artistic", "Rain Pulse"]}],
        }]
        with patch.object(gallery, "ROOT", root):
            entries = gallery.plan_entries(inventory)
        for entry in entries:
            rain = entry["modeLabel"] == "Rain Pulse"
            with self.subTest(mode=entry["modeLabel"]):
                self.assertEqual(entry["flashing"], rain)
                self.assertEqual(entry["settings"], {"mode": entry["modeLabel"]})
                if rain:
                    self.assertIn("full-field pulse", entry["description"])
                    self.assertIn("white", entry["description"])
                    self.assertIn("band-color controls", entry["description"])
                    self.assertIn("droplet animation is not included", entry["description"])
                else:
                    self.assertNotIn("full-field pulse", entry["description"])

    def test_rejects_invalid_realtime_pacing(self):
        entry = {"id": "timing", "durationMs": 4000, "fps": 10, "playbackScale": 1}
        cases = [
            ("too-slow", {"wallMs": 7980 * 1.06,
                          "frameWallTimesMs": [t * 1.06 for t in self.report()["frameWallTimesMs"]]}),
            ("too-fast", {"wallMs": 7980 * 0.94,
                          "frameWallTimesMs": [t * 0.94 for t in self.report()["frameWallTimesMs"]]}),
            ("warmup-slow", {"wallMs": 8221, "frameWallTimesMs": [4241 + i * 100 for i in range(40)]}),
            ("warmup-fast", {"wallMs": 7739, "frameWallTimesMs": [3759 + i * 100 for i in range(40)]}),
            ("capture-slow", {"wallMs": 8214, "frameWallTimesMs": [4000 + i * 106 for i in range(40)]}),
            ("capture-fast", {"wallMs": 7746, "frameWallTimesMs": [4000 + i * 94 for i in range(40)]}),
            ("missing-timestamps", {"frameWallTimesMs": []}),
            ("nonmonotonic", {"frameWallTimesMs": [4000] * 40}),
        ]
        for stall in (300, 900, 2000):
            timestamps = []
            for i in range(40):
                target = 4000 + i * 100 + (stall if i == 20 else 0)
                timestamps.append(max(target, timestamps[-1] + 0.001) if timestamps else target)
            cases.append((f"recovered-stall-{stall}ms", {
                "wallMs": max(7980, timestamps[-1] + 20),
                "frameWallTimesMs": timestamps,
            }))
        for label, overrides in cases:
            with self.subTest(case=label):
                report = {**self.report(), **overrides}
                with self.assertRaises(RuntimeError):
                    gallery.validate_pacing(entry, report)

    def test_synthetic_audio_is_not_wall_paced(self):
        report = {**self.report(), "audio": True, "wallMs": 20}
        result = gallery.validate_pacing(
            {"id": "synthetic", "durationMs": 4000, "fps": 10, "playbackScale": 1}, report)
        self.assertEqual(result["clock"], "synthetic")

    @unittest.skipUnless(os.environ.get("HUE_GALLERY_CAPTURE_DIR"),
                         "Set HUE_GALLERY_CAPTURE_DIR to the saved full native capture")
    def test_saved_sparse_clips(self):
        capture = Path(os.environ["HUE_GALLERY_CAPTURE_DIR"])
        entries = {e["id"]: e for e in gallery.read_json(capture / "capture-plan.json")}
        reports = {r["id"]: r for r in gallery.read_json(capture / "native-reports.json")}
        cases = [
            ("audioflame--default", "native"),
            ("audioshot--default", "native"),
            ("audioflame--default", "all-zero"),
            ("audioflame--default", "empty"),
        ]
        for ident, variant in cases:
            with self.subTest(effect=ident, variant=variant):
                entry, report = copy.deepcopy(entries[ident]), copy.deepcopy(reports[ident])
                work = self.work / "saved-sparse" / f"{ident}-{variant}"
                (work / ident).mkdir(parents=True, exist_ok=True)
                stage = work / "publish"
                (stage / "media").mkdir(parents=True, exist_ok=True)
                raw = (capture / ident / "frames.rgb").read_bytes()
                if variant == "empty":
                    raw = b""
                elif variant == "all-zero":
                    raw = bytes(len(raw))
                    for frame in report["nativeMapPixelProof"]:
                        for pixel in frame["pixels"]:
                            pixel["rgb"] = 0
                (work / ident / "frames.rgb").write_bytes(raw)
                if variant != "native":
                    with self.assertRaisesRegex(RuntimeError, "[Bb]lank|Invalid RGB stream"):
                        gallery.encode(entry, report, work, stage)
                    self.assertEqual(list((stage / "media").iterdir()), [])
                    continue
                validation, _ = gallery.encode(entry, report, work, stage)
                metrics = validation["appearance"]
                self.assertLessEqual(metrics["maximumMeanRGB"], 3)
                self.assertGreater(metrics["peakChannel"], 0)
                self.assertIn("mean RGB", entry["description"])
                self.assertIn(f"peak {metrics['peakChannel']}/255", entry["description"])
                self.assertTrue(validation["posterExactNativeRGB"])
                self.assertEqual(validation["nativePixelComparisons"], 120)
                self.assertEqual(validation["gifSampleComparisons"], 40)
                self.assertEqual(validation["decodedDurationMs"], 4000)
                gallery.write_json(work / "encoding-verification.json", {
                    "entry": entry, "validation": validation,
                    "savedRawSha256": gallery.sha256(capture / ident / "frames.rgb"),
                })

    @unittest.skipUnless(os.environ.get("HUE_GALLERY_CAPTURE_DIR"),
                         "Set HUE_GALLERY_CAPTURE_DIR to the saved full native capture")
    def test_saved_magnitude_fidelity(self):
        from PIL import Image, ImageChops, ImageStat

        capture = Path(os.environ["HUE_GALLERY_CAPTURE_DIR"])
        ident = "audiomagnitude--default"
        entry = next(e for e in gallery.read_json(capture / "capture-plan.json") if e["id"] == ident)
        report = gallery.read_json(capture / ident / "native-report.json")
        work = self.work / "magnitude-fidelity"
        (work / ident).mkdir(parents=True, exist_ok=True)
        stage = work / "publish"
        (stage / "media").mkdir(parents=True, exist_ok=True)
        raw = (capture / ident / "frames.rgb").read_bytes()
        (work / ident / "frames.rgb").write_bytes(raw)
        validation, _ = gallery.encode(entry, report, work, stage)
        self.assertLess(validation["maximumGifMeanAbsoluteRGBError"], 10)
        self.assertEqual(validation["gifSampleComparisons"], 40)
        self.assertEqual(validation["decodedDurationMs"], 4000)
        size = (report["width"], report["height"])
        stride = size[0] * size[1] * 3
        expected = Image.frombytes("RGB", size, raw[35 * stride:36 * stride])
        frame35_error = None
        with Image.open(stage / entry["animation"]) as gif:
            elapsed = 0
            for i in range(gif.n_frames):
                gif.seek(i)
                duration = gif.info["duration"]
                if elapsed <= 3500 < elapsed + duration:
                    actual = gif.convert("RGB").resize(size, Image.Resampling.NEAREST)
                    frame35_error = sum(ImageStat.Stat(ImageChops.difference(actual, expected)).mean) / 3
                    break
                elapsed += duration
        self.assertIsNotNone(frame35_error)
        self.assertLess(frame35_error, 10)
        gallery.write_json(work / "encoding-verification.json", {
            "entry": entry, "validation": validation, "frame35MeanAbsoluteRGBError": frame35_error,
        })

    def test_sample_refuses_complete_without_mutation(self):
        for status in ("complete", "sample"):
            with self.subTest(status=status):
                output = self.work / status
                (output / "media").mkdir(parents=True, exist_ok=True)
                gallery.write_json(output / "catalog.json", {"generation": {"status": status}})
                gallery.write_json(output / gallery.MARKER, {
                    "generator": gallery.GENERATOR,
                    "files": {"catalog.json": gallery.sha256(output / "catalog.json")},
                })
                before = {p.relative_to(output): p.read_bytes()
                          for p in output.rglob("*") if p.is_file()}
                argv = ["generate.py", "--sample", "--output", str(output),
                        "--work-dir", str(self.work / "build-work")]
                with patch.object(sys, "argv", argv), \
                     patch.object(gallery, "cache_values", return_value={"CMAKE_BUILD_TYPE": "Release"}), \
                     patch.object(gallery, "run", side_effect=RuntimeError("native build reached")) as run:
                    message = "complete.*different.*output" if status == "complete" else "native build reached"
                    with self.assertRaisesRegex(RuntimeError, message):
                        gallery.main()
                    self.assertEqual(run.call_count, 0 if status == "complete" else 1)
                after = {p.relative_to(output): p.read_bytes()
                         for p in output.rglob("*") if p.is_file()}
                self.assertEqual(before, after)


if __name__ == "__main__":
    unittest.main()
