#!/usr/bin/env python3
"""Generate native HSV previews without opening or controlling the live app."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[2]
TOOL = Path(__file__).resolve().parent
GENERATOR = "tools/hue-gallery/generate.py"
MARKER = "media/generation.json"
PALETTE = ["#521887", "#ff4867", "#ffbd45", "#10d9c5", "#bdefff"]
SAMPLES = {"audiomelt", "audiomeltsparkle", "audiowaterfall", "audiogameoflife",
           "huefade", "huesinglecolor"}
SOURCE_PALETTE_SCRIPTS = {"audiomelt", "audiomeltsparkle", "audioblocks", "audiocrawler",
                          "audiolava", "audiowater", "audiofire"}
MATRIX = {
    "audioaurora", "audiobandsmatrix", "audiobleep", "audioblurz",
    "audiocellular", "audioconcentric", "audiodigitalrain", "audioequalizer2d",
    "audiofire", "audiofireworks", "audioflame", "audioflowfield", "audiogameoflife",
    "audiohierarchy", "audiolava", "audioplasma", "audiopuddles", "audioreaction",
    "audioreactor", "audioshockwave", "audioshot", "audiosmoke", "audiosoap",
    "audiosplittower", "audiospotlight", "audiotunnel", "audiovortex", "audiowater",
    "audiowaterfall",
}
FLASHING = {
    "audiobarcode", "audiobasslaser", "audiobeatcolors", "audiobuildup",
    "audiofireworks", "audioglitch", "audioglitch2", "audiomeltsparkle",
    "audiopower", "audioscanflare", "audioshot", "audiostrobe",
    "huemetro", "huerandomflash",
}
DESCRIPTIONS = {
    "audiomelt": "Layered sine-wave melt. Artistic and LedFx response modes share input and palette assignment.",
    "audiomeltsparkle": "Moving melt background with transient sparkle accents. Contains flashes.",
    "audiowaterfall": "Spectral novelty colors descend through a two-dimensional history.",
    "audiogameoflife": "Conway-style cellular evolution with beat/kick pattern injection and occupancy coloring.",
}
INPUT = (
    "Synthetic 120 BPM, 4-second repeating pattern: 0–0.5 s quiet, "
    "0.5–2 s sustain, 2–4 s transients. Unequal low/mid/full spectral banks, "
    "independent filtered and raw powers, changing pitch, cumulative beat/kick/"
    "onset/bar counters. AudioSnapshot → AudioRenderView → production v6 JS bridge. "
    "Visual-only preview; no microphone, music, live workspace, or DMX."
)


def read_json(path):
    return json.loads(path.read_text())


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n")


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def cache_values(build):
    result = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line and not line.startswith(("//", "#")) and "=" in line:
            name, value = line.split("=", 1)
            result[name.split(":", 1)[0]] = value
    require(Path(result["CMAKE_HOME_DIRECTORY"]).resolve() == ROOT,
            "--build-dir belongs to another source checkout")
    return result


def run(command, log):
    with log.open("w") as output:
        result = subprocess.run([str(arg) for arg in command], cwd=ROOT,
                                stdout=output, stderr=subprocess.STDOUT)
    require(result.returncode == 0, f"Command failed. See {log}")


def safe_output(output, name):
    require(name in ("catalog.json", MARKER) or
            re.fullmatch(r"media/[a-z0-9-]+\.(png|gif)", name),
            f"Not a generated gallery path: {name}")
    path = output / name
    require(not output.is_symlink() and not (output / "media").is_symlink()
            and not path.is_symlink(), f"Refusing a symlink output: {path}")
    return path


def ownership(output):
    path = safe_output(output, MARKER)
    if not path.exists():
        return {}, None
    marker = read_json(path)
    require(marker.get("generator") == GENERATOR, "Output belongs to another generator")
    return marker.get("files", {}), sha256(path)


def preflight(output, names, owned, marker_hash):
    marker = safe_output(output, MARKER)
    require((sha256(marker) if marker.exists() else None) == marker_hash,
            "Generation manifest changed during capture")
    for name in names:
        path = safe_output(output, name)
        if path.exists():
            require(name in owned, f"Refusing to overwrite unowned file: {path}")
            require(sha256(path) == owned[name],
                    f"Generated file was edited; preserve it or use another output: {path}")


def plan_entries(inventory):
    listed = set(re.findall(r"^\s+([a-z0-9]+\.js)$",
                            (ROOT / "resources/huescripts/CMakeLists.txt").read_text(), re.M))
    require({effect["file"] for effect in inventory} == listed - {"hsvutil.js"},
            "Native inventory and installed script list differ")
    entries = []
    for effect in inventory:
        stem = Path(effect["file"]).stem
        primary = [p for p in effect["properties"] if p["name"] in ("mode", "presetMode")]
        require(len(primary) <= 1, f"Ambiguous primary mode: {effect['name']}")
        modes = primary[0]["choices"] if primary else ["Default"]
        require(bool(modes), f"Missing primary mode choices: {effect['name']}")
        for mode in modes:
            ident = stem + "--" + re.sub(r"[^a-z0-9]+", "-", mode.lower()).strip("-")
            rain_pulse = stem == "audiopuddles" and mode == "Rain Pulse"
            description = DESCRIPTIONS.get(stem, effect["name"] + " rendered by the production native HSV engine.")
            accepted_colors = max(0, effect["acceptColors"])
            if accepted_colors == 0:
                description += " Native metadata accepts no palette; built-in/procedural hues replace the assigned palette."
            elif accepted_colors < len(PALETTE):
                description += f" Native metadata accepts only the first {accepted_colors} of {len(PALETTE)} assigned colors; the remaining assigned colors are not passed to the script."
            if stem in SOURCE_PALETTE_SCRIPTS:
                if mode == "Artistic":
                    description += " This Artistic mode uses procedural hues and ignores the injected palette."
                elif mode.startswith("LedFx "):
                    description += f" This LedFx mode uses the first {accepted_colors} assigned colors."
            if rain_pulse:
                description += (
                    " This is LedFx Rain's full-field pulse layer: the selected band triggers "
                    "a uniform flash that fades. Lows defaults to white; separate band-color "
                    "controls set the pulse color, not the assigned palette. The spatial "
                    "droplet animation is not included."
                )
            entries.append({
                "id": ident, "effectName": effect["name"], "modeLabel": mode,
                "sourcePath": "resources/huescripts/" + effect["file"],
                "description": description,
                "layout": {"columns": 32 if stem in MATRIX else 80, "rows": 32 if stem in MATRIX else 4},
                "palette": PALETTE,
                "acceptedColorCount": accepted_colors,
                "settings": {primary[0]["name"]: mode} if primary else {},
                "inputDescription": INPUT if effect["audio"] else
                    "No audio. Production non-audio rgbMap with real-time Date clock after 4 seconds of warmup.",
                "durationMs": 4000, "fps": 10, "playbackScale": 1,
                "flashing": stem in FLASHING or rain_pulse, "poster": None, "animation": None,
                "status": "pending", "error": None,
            })
    require(len({e["id"] for e in entries}) == len(entries), "Duplicate variant IDs")
    require(len({e["effectName"] for e in entries}) == len(inventory), "Duplicate effect identities")
    return entries


def source_hashes():
    files = list((ROOT / "resources/huescripts").glob("*.js"))
    files += [ROOT / "resources/huescripts/CMakeLists.txt"]
    for directory in (ROOT / "engine/src", ROOT / "engine/audio/src", TOOL):
        files += [p for p in directory.iterdir() if p.suffix in (".cpp", ".h", ".py", ".txt")]
    return {str(path.relative_to(ROOT)): sha256(path) for path in sorted(files)}


def validate_pacing(entry, report):
    if report["audio"]:
        return {"clock": "synthetic", "wallMs": report["wallMs"]}
    sample_interval = 1000 / entry["fps"]
    expected_wall = report["warmupMs"] + entry["durationMs"] - report["renderIntervalMs"]
    expected_span = entry["durationMs"] - sample_interval
    timestamps = report.get("frameWallTimesMs", [])
    require(len(timestamps) == report["frames"] == entry["durationMs"] / sample_interval
            and len(timestamps) > 1 and all(math.isfinite(t) for t in timestamps)
            and all(a < b for a, b in zip(timestamps, timestamps[1:])),
            f"Invalid real-time sample timestamps: {entry['id']}")
    measurements = {
        "wall": (report["wallMs"], expected_wall),
        "warmup": (timestamps[0], report["warmupMs"]),
        "capture span": (timestamps[-1] - timestamps[0], expected_span),
    }
    for name, (measured, expected) in measurements.items():
        require(expected > 0 and math.isfinite(measured)
                and abs(measured - expected) <= expected * 0.05,
                f"Non-audio {name} pacing differs by more than 5% for {entry['id']}: "
                f"measured {measured:.3f} ms, expected {expected:.3f} ms. "
                "No real-time preview published; retry without system load.")
    require(report["wallMs"] >= timestamps[-1],
            f"Capture ends after the reported wall time: {entry['id']}")
    frame_errors = []
    for index, measured in enumerate(timestamps):
        target = report["warmupMs"] + index * sample_interval
        error = abs(measured - target)
        require(error <= sample_interval / 2,
                f"Non-audio frame {index} missed its real-time target for {entry['id']}: "
                f"measured {measured:.3f} ms, expected {target:.3f} ms "
                f"(tolerance {sample_interval / 2:.3f} ms). No preview published.")
        frame_errors.append(error)
    return {
        "clock": "wall", "wallMs": report["wallMs"], "expectedWallMs": expected_wall,
        "measuredWarmupMs": timestamps[0], "expectedWarmupMs": report["warmupMs"],
        "measuredCaptureSpanMs": timestamps[-1] - timestamps[0],
        "expectedCaptureSpanMs": expected_span, "sampleIntervalMs": sample_interval,
        "toleranceFraction": 0.05,
        "maximumFrameTargetErrorMs": max(frame_errors),
        "frameTargetToleranceMs": sample_interval / 2,
    }


def encode(entry, report, work, stage):
    from PIL import Image, ImageChops, ImageDraw, ImageStat

    ident = entry["id"]
    pacing = validate_pacing(entry, report)
    size = (report["width"], report["height"])
    raw = (work / ident / "frames.rgb").read_bytes()
    stride = size[0] * size[1] * 3
    require(all(side > 0 for side in size) and report["frames"] > 0
            and len(raw) == report["frames"] * stride, f"Invalid RGB stream: {ident}")
    require(report["paletteOnRegisteredOwner"] == entry["palette"], f"Owner palette mismatch: {ident}")
    require(report["acceptedColorCount"] == entry["acceptedColorCount"],
            f"Native palette acceptance differs: {ident}")
    frames = [Image.frombytes("RGB", size, raw[i * stride:(i + 1) * stride])
              for i in range(report["frames"])]
    comparisons = 0
    for proof in report["nativeMapPixelProof"]:
        for pick in proof["pixels"]:
            rgb = int(pick["rgb"])
            require(frames[proof["frame"]].getpixel((pick["x"], pick["y"])) ==
                    ((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255),
                    f"Native map pixel mismatch: {ident}")
            comparisons += 1
    means = [sum(ImageStat.Stat(f).mean) / 3 for f in frames]
    peak = max(raw)
    require(peak > 0, f"Fully blank native preview: {ident}")
    lit_counts = []
    for frame in frames:
        pixels = frame.tobytes()
        lit_counts.append(sum(any(rgb) for rgb in zip(pixels[::3], pixels[1::3], pixels[2::3])))
    pixel_count = size[0] * size[1]
    appearance = {
        "maximumMeanRGB": max(means), "peakChannel": peak,
        "maximumLitPixels": max(lit_counts), "pixelCount": pixel_count,
        "maximumLitFraction": max(lit_counts) / pixel_count,
        "blankFrameCount": lit_counts.count(0),
    }
    if appearance["maximumMeanRGB"] <= 3:
        entry["description"] += (
            f" Low average brightness in this native sample: mean RGB up to {max(means):.3f}/255; "
            f"peak {peak}/255; up to {max(lit_counts)}/{pixel_count} pixels lit "
            f"({100 * appearance['maximumLitFraction']:.2f}%); "
            f"{appearance['blankFrameCount']}/{len(frames)} fully black frames."
        )
    distinct = len({frame.tobytes() for frame in frames})
    if distinct == 1:
        entry["description"] += " This preview is static at the selected settings."
    poster_index = max(range(len(frames)), key=lambda i:
                       means[i] + sum(ImageStat.Stat(frames[i]).stddev) / 6)
    scale = 12 if size[0] == size[1] else 8
    dimensions = (size[0] * scale, size[1] * scale)
    poster = frames[poster_index].resize(dimensions, Image.Resampling.NEAREST)
    poster_name, gif_name = f"media/{ident}.png", f"media/{ident}.gif"
    poster.save(stage / poster_name, optimize=True)
    encoded = [f.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
               .resize(dimensions, Image.Resampling.NEAREST) for f in frames]
    encoded[0].save(stage / gif_name, save_all=True, append_images=encoded[1:],
                    duration=100, loop=0, optimize=False, disposal=2)
    with Image.open(stage / poster_name) as decoded:
        decoded.load()
        require(decoded.size == dimensions and ImageChops.difference(
            decoded.convert("RGB").resize(size, Image.Resampling.NEAREST),
            frames[poster_index]).getbbox() is None, f"Poster differs from native pixels: {ident}")
    duration = 0
    max_error = 0
    sample_comparisons = 0
    with Image.open(stage / gif_name) as decoded:
        require(decoded.size == dimensions, f"GIF dimensions differ: {ident}")
        count = decoded.n_frames
        for i in range(count):
            decoded.seek(i)
            decoded.load()
            actual = decoded.convert("RGB").resize(size, Image.Resampling.NEAREST)
            frame_duration = decoded.info["duration"]
            require(frame_duration > 0 and frame_duration % 100 == 0
                    and duration + frame_duration <= entry["durationMs"], f"GIF timeline mismatch: {ident}")
            for sample in range(duration // 100, (duration + frame_duration) // 100):
                error = sum(ImageStat.Stat(ImageChops.difference(actual, frames[sample])).mean) / 3
                max_error = max(max_error, error)
                sample_comparisons += 1
            duration += frame_duration
    require(duration == entry["durationMs"] and sample_comparisons == len(frames) and max_error < 10,
            f"GIF timeline or palette mismatch: {ident}")
    entry.update(poster=poster_name, animation=gif_name, status="ready", error=None)
    row_height = 208 if size[0] == size[1] else 96
    row = Image.new("RGB", (1200, row_height), "#0b1020")
    draw = ImageDraw.Draw(row)
    draw.text((8, 5), f"{entry['effectName']} · {entry['modeLabel']} · native {size[0]}×{size[1]}", fill="#e5edf7")
    for col, i in enumerate((0, 8, 16, 20, 30, 39)):
        width = 160 if size[0] == size[1] else 192
        height = round(width * size[1] / size[0])
        row.paste(frames[i].resize((width, height), Image.Resampling.NEAREST),
                  (col * 200 + (200 - width) // 2, 30 + (row_height - 48 - height) // 2))
        draw.text((col * 200 + 8, row_height - 15), f"t={i / 10:.1f}s", fill="#b5c5dd")
    row.save(work / f"{ident}-contact.png", optimize=True)
    return {
        "id": ident, "nativeFrames": len(frames), "distinctNativeFrames": distinct,
        "decodedGifFrames": count, "decodedDurationMs": duration,
        "encodedDimensions": list(dimensions), "posterFrame": poster_index,
        "nativePixelComparisons": comparisons, "posterExactNativeRGB": True,
        "maximumGifMeanAbsoluteRGBError": max_error,
        "gifSampleComparisons": sample_comparisons,
        "acceptedColorCount": report["acceptedColorCount"], "pacing": pacing,
        "appearance": appearance,
    }, row


def verify(output):
    from PIL import Image

    owned, marker_hash = ownership(output)
    require(marker_hash is not None, "No gallery generation manifest")
    preflight(output, owned, owned, marker_hash)
    for name in owned:
        require(safe_output(output, name).exists(), f"Missing generated file: {name}")
    catalog = read_json(output / "catalog.json")
    entries = catalog["entries"]
    require(len(entries) == catalog["plannedVariantCount"] and
            len({e["id"] for e in entries}) == len(entries) and
            len({e["effectName"] for e in entries}) == catalog["plannedEffectCount"],
            "Catalog inventory counts or IDs differ")
    for entry in entries:
        if entry["status"] != "ready":
            require(entry["poster"] is None and entry["animation"] is None,
                    f"Unready entry references assets: {entry['id']}")
            continue
        for field in ("poster", "animation"):
            require(entry[field] in owned, f"Unowned catalog asset: {entry[field]}")
            with Image.open(safe_output(output, entry[field])) as image:
                duration = 0
                for i in range(getattr(image, "n_frames", 1)):
                    image.seek(i)
                    image.load()
                    duration += image.info.get("duration", 0)
                if field == "animation":
                    require(duration == entry["durationMs"] / entry["playbackScale"],
                            f"Incorrect GIF duration: {entry['id']}")
    require(catalog["generation"]["status"] != "complete" or
            all(e["status"] == "ready" for e in entries), "Incomplete gallery marked complete")
    return sum(e["status"] == "ready" for e in entries)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/hue-effects")
    parser.add_argument("--work-dir", type=Path,
                        help="Native build, raw frames and logs (default: HUE_GALLERY_WORK_DIR or BUILD/hue-gallery-artifacts)")
    parser.add_argument("--sample", action="store_true", help="Only the eight tracer variants; refuses to downgrade a complete publication")
    parser.add_argument("--verify-only", action="store_true", help="Check published ownership hashes, decode assets and validate catalog")
    args = parser.parse_args()
    try:
        from PIL import Image
    except ImportError as error:
        raise RuntimeError("Install the image encoder: python3 -m pip install -r tools/hue-gallery/requirements.txt") from error
    output = args.output.absolute()
    if args.verify_only:
        print(f"Verified {verify(output)} ready variants.")
        return
    build = args.build_dir.resolve()
    cache = cache_values(build)
    owned, marker_hash = ownership(output)
    preflight(output, ["catalog.json"], owned, marker_hash)
    if args.sample and (output / "catalog.json").exists():
        require(read_json(output / "catalog.json")["generation"]["status"] != "complete",
                "--sample cannot replace a complete publication; use a different --output directory.")
    work_root = (args.work_dir or Path(os.environ.get("HUE_GALLERY_WORK_DIR",
                                                     str(build / "hue-gallery-artifacts")))).resolve()
    require(not work_root.is_relative_to(output.resolve()),
            "Raw frames and build artifacts must be outside the published output")
    work = work_root / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S") + "-" + uuid.uuid4().hex[:8])
    work.mkdir(parents=True)
    config = cache.get("CMAKE_BUILD_TYPE") or "Release"
    run(["cmake", "--build", build, "--target", "qlcplusengine", "--config", config], work / "engine-build.log")
    native_build = work_root / "native-build"
    run(["cmake", "-S", TOOL, "-B", native_build, f"-DQLC_BUILD_DIR={build}",
         f"-DCMAKE_BUILD_TYPE={config}"], work / "native-configure.log")
    run(["cmake", "--build", native_build, "--config", config], work / "native-build.log")
    native, engine = map(Path, (native_build / f"native-paths-{config}.txt").read_text().splitlines())
    hashes = source_hashes()
    binaries = {"nativeRenderer": sha256(native), "nativeEngine": sha256(engine)}
    run([native, "inventory", ROOT / "resources/huescripts", work / "inventory.json", work],
        work / "inventory.log")
    inventory = read_json(work / "inventory.json")
    entries = plan_entries(inventory)
    selected = [e for e in entries if not args.sample or Path(e["sourcePath"]).stem in SAMPLES]
    names = ["catalog.json"] + [f"media/{e['id']}.{ext}" for e in selected for ext in ("png", "gif")]
    preflight(output, names, owned, marker_hash)
    write_json(work / "capture-plan.json", selected)
    run([native, "capture", ROOT / "resources/huescripts", work / "capture-plan.json", work],
        work / "capture.log")
    reports = {r["id"]: r for r in read_json(work / "native-reports.json")}
    require(set(reports) == {e["id"] for e in selected}, "Native capture omitted variants")
    stage = work / "publish"
    (stage / "media").mkdir(parents=True)
    validation, rows = [], []
    for entry in selected:
        result, row = encode(entry, reports[entry["id"]], work, stage)
        validation.append(result)
        rows.append(row)
    for page in range(0, len(rows), 12):
        batch = rows[page:page + 12]
        sheet = Image.new("RGB", (1200, sum(row.height for row in batch)), "#0b1020")
        y = 0
        for row in batch:
            sheet.paste(row, (0, y))
            y += row.height
        sheet.save(work / f"contact-sheet-{page // 12 + 1}.png", optimize=True)
    require(hashes == source_hashes() and binaries ==
            {"nativeRenderer": sha256(native), "nativeEngine": sha256(engine)},
            "Source or native binary changed during capture; no output published")
    generation = {
        "status": "sample" if args.sample else "complete",
        "sourceCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "createdAt": datetime.now(timezone.utc).isoformat(),
        "renderer": "Production HUEScript/HUEMatrix owner, native Qt QJSEngine, HSV-to-RGB packing. Isolated Doc; no live application.",
        "inputDescription": INPUT,
        "notes": [
            "Native render previews, not desktop screenshots or hardware validation.",
            "Every primary mode/presetMode choice is inventoried. Other property cross-products are excluded.",
            "Unspecified settings use native defaults. Source/helper and native binary hashes are in media/generation.json.",
            "50 Hz rendering, 4-second warmup, 10 fps sampling over 4 seconds. Script steps advance once per 120 BPM beat.",
            "80×4 WLED-style strips; intrinsic 2D effects use 32×32. Mode comparisons share layout, assigned palette, input and timing.",
            "Palette swatches describe assigned owner colors. acceptedColorCount records native acceptance: 0 means built-in/procedural hues, 1–4 means only the first N assigned stops are passed to the script.",
            "No public random seed API: Math.random effects are nondeterministic. Non-audio Date clocks run in real-time.",
            "GIFs may jump when looping; seamless loops are not claimed. playbackScale is a speed multiplier already encoded in the GIF (1 = real-time).",
            "GIFs use adaptive per-frame 256-color palettes. Decoded colors are checked against every native sample, including frames combined into longer holds.",
            "Non-audio real-time warmup, capture span and total wall duration must each remain within 5% of their timing targets. Every sampled frame must also land within half a sample interval of its absolute target; otherwise generation is rejected.",
            "Flashing previews require explicit playback. GIFs contain no audio.",
            "Publication replacement is atomic per file, not across the whole generation. An interruption can leave mixed files that fail verification; restore a consistent generated-file set from backup/version control, or regenerate to a different empty output directory.",
        ],
    }
    catalog = {"schemaVersion": 1, "generation": generation, "plannedEffectCount": len(inventory),
               "plannedVariantCount": len(entries), "entries": entries}
    write_json(stage / "catalog.json", catalog)
    files = {**owned, **{name: sha256(stage / name) for name in names}}
    manifest = {"schemaVersion": 1, "generator": GENERATOR, "generation": generation,
                "sourceSha256": hashes, "binarySha256": binaries, "files": files,
                "validation": validation, "randomSeed": None}
    write_json(stage / MARKER, manifest)
    write_json(work / "capture-manifest.json", manifest)
    preflight(output, names, owned, marker_hash)
    output.mkdir(parents=True, exist_ok=True)
    (output / "media").mkdir(exist_ok=True)
    require(stage.stat().st_dev == output.stat().st_dev,
            "--work-dir must share a filesystem with --output for atomic file replacement")
    for name in [n for n in names if n != "catalog.json"] + ["catalog.json", MARKER]:
        os.replace(stage / name, safe_output(output, name))
    ready = verify(output)
    print(f"{ready}/{len(entries)} variants ready across {len(inventory)} effects. Evidence: {work}")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError) as error:
        sys.exit(f"Gallery generation failed: {error}")
