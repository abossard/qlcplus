# HUE effects visual booklet

Open [index.html](index.html) for static posters, click-to-play GIFs and
side-by-side main modes. The page reads `catalog.json` and `media/` from this
directory. It uses no external fonts, scripts or services.

## Open locally

From the repository root:

```sh
python3 -m http.server 8765 --bind 127.0.0.1 --directory docs
```

Open <http://127.0.0.1:8765/hue-effects/>. Stop the server with Ctrl+C.
Python's standard library is enough.

You can also open `index.html` as a file. If your browser blocks the JSON
request, use **Select catalog.json** on the page. Keep the HTML, catalog and
`media/` together. Missing catalog data produces an error message, not sample
images. Use local HTTP to load the detailed provenance manifest.

## Find and compare effects

- Search by effect name, mode or description. Filter by mode, capture status,
  or **Hide flashing**.
- Use **Contents** to jump to an effect. Names sort alphabetically without the
  common `Audio` prefix.
- Compare mode cards in the same effect group. Check **Capture settings** for
  differences in input, layout, palette and parameters.
- Choose **Play preview** to animate one card. Playing another stops the first.
  Use **Stop preview**, Escape, or the same card's button to stop.
- Flashing entries have a warning and a separate **Play flashing preview**
  label. Nothing autoplays, including with reduced motion enabled.

Artistic, Reference and named LedFx modes select built-in behavior variants.
Create separate named HUEMatrix functions to store different configurations
of one script. Saving a `.qxw` workspace
preserves mode, parameters, palette and timing, not running particle buffers,
history or animation phase.

**Rain Pulse** is only LedFx Rain's full-field flash layer, not its spatial
droplet animation. The selected audio band triggers a pulse that fades.
Lows defaults to white; its separate band-color controls set the pulse color,
not the assigned palette.

See [Audio analysis and script data](../audio-data-usage.md) for audio fields,
mode boundaries and measured runtime limitations. The earlier
[algorithm technical review](../rgb-matrix-algorithm-review.md) records its own
historical inventory and validation scope.

## Read the capture record

The capture header reports the generation status and counts from the loaded
manifest. Planned totals describe scope, not completed work. Entries can be
`ready`, `pending` or `error`; incomplete captures stay visible.
An absent or unreadable media file gets a separate warning even if its
manifest entry says `ready`.

| Field | Meaning |
|---|---|
| Source commit | HEAD recorded by the capture tool. It does not include uncommitted edits. |
| Working-tree and native hashes | Capture-time source bytes and compiled artifacts from `media/generation.json`. Open **Source, renderer and input** for binary hashes and expandable per-file hashes. |
| Renderer | Native rendering path recorded by the capture tool. |
| Input | Synthetic sequence supplied during the capture. |
| Layout | Logical pixel columns and rows, not physical fixture dimensions. |
| Assigned palette | Colors assigned to the HUEMatrix owner. Scripts accepting no colors use built-in or procedural hues; check the card description. |
| Accepted colors | `acceptedColorCount` gives the script's native palette capacity. Zero means no palette; values 1–4 pass only the first N of the five assigned colors. Cards distinguish assigned colors from those the script receives. |
| Settings | Script properties used for that variant. |
| Duration | Sampled simulation length, not measured render latency or scaled GIF duration. |
| FPS | Capture sampling rate, not the application's achieved frame rate. |
| Playback speed | Speed multiplier in `playbackScale`: `1×` is real-time; `0.5×` doubles GIF duration. The GIF already includes the adjustment. |

Use source and native-binary hashes to identify captures made during in-flight
edits. HEAD alone cannot identify that build. The page displays additional
generation metadata, including nested provenance, as supplied by the tool.
Missing metadata does not establish a clean working tree or an up-to-date build.
The page checks that the manifest's commit and generation timestamp match the
catalog. It displays recorded hashes without rechecking the current checkout.

## Representation limits

These are offline native-rendered captures with synthetic input. They do not
represent microphone capture, a musical listening test, live hardware output,
fixture brightness, DMX transport, or runtime-budget acceptance.

PNG posters show one frame. GIFs reduce color precision and sample motion;
short pulses may disappear or look different. Your browser scales the logical
pixel grid to fit a card. The GIF loop can restart particles, history or phase,
so a visible loop boundary does not establish a runtime effect defect.
GIF encoders can merge identical frames while preserving duration, so the
encoded frame count can differ from the capture sample count.

## Print a booklet

Clear filters for the full catalog, or filter first for a smaller selection.
Choose **Print visible effects** or your browser's Print command. The print
layout uses a light theme, a contents list, static posters and page breaks
between effects. Expanded settings do not print.

Wait for posters to load before printing. Missing files retain their warning.
Printer and browser color handling can change the output.

## Regenerate captures

Use an existing CMake build of this checkout with `qmlui=ON`. For a new build:

```sh
cmake -S . -B build -Dqmlui=ON
```

The generator needs Python and Pillow for PNG/GIF encoding. If Pillow is
missing, install the declared dependency:

```sh
python3 -m pip install -r tools/hue-gallery/requirements.txt
```

Generate the eight-variant tracer sample before the full batch. It includes
Hue Fade and Hue Single Color alongside the audio and cellular comparisons:

```sh
python3 tools/hue-gallery/generate.py --build-dir build --output docs/hue-effects --sample
```

`--sample` refuses to replace a complete publication. Use a different
`--output` directory for a sample once the full catalog exists.

Review that sample, then generate the full catalog:

```sh
python3 tools/hue-gallery/generate.py --build-dir build --output docs/hue-effects
```

The generator rebuilds `qlcplusengine` and its standalone
`hue_gallery_native` helper. It reuses the Qt/compiler configuration from
`build/CMakeCache.txt` and rejects a build from another checkout. It renders
in an isolated process without opening or controlling the live application.
For non-audio captures, it checks ordered native frame timestamps and rejects
warmup, capture-span or total-wall-duration deviations beyond 5% of their
timing targets. Each sample must also land within half a sampling interval
of its scheduled time (50 ms at 10 fps). This catches mid-capture stalls even
when later frames catch up. Rejected captures do not reach publication.

Entirely black clips fail validation. Review sparse or dim clips using the
measured notes on their cards; the generator leaves their native brightness
and particle density unchanged.

Validate published output without recapturing:

```sh
python3 tools/hue-gallery/generate.py --output docs/hue-effects --verify-only
```

`--verify-only` checks generated-file ownership hashes, catalog counts, image
decoding and GIF duration. It does not compare recorded source hashes against
the current checkout. Regenerate after source or engine changes.

Build artifacts, raw frames, logs and contact sheets go to
`build/hue-gallery-artifacts` by default. Set `--work-dir PATH` or
`HUE_GALLERY_WORK_DIR` to keep them elsewhere, outside the published output
and on the same filesystem. Native random effects have no public seed API,
so repeated captures can differ.

The capture tool owns `catalog.json` (schema version 1) and `media/`.
It records generation provenance, planned counts and one entry per effect
variant. Each entry supplies native PNG/GIF paths or an explicit pending/error
status. Regenerate those outputs rather than editing individual cards or
maintaining asset paths by hand, then choose **Reload catalog**. Keep the
generator's source hashes and native build provenance with the outputs.
It checks for source/binary changes during capture and refuses to overwrite
edited or unowned generated files. Preserve those files or choose another
output directory rather than changing the ownership manifest. It leaves
`index.html` and this README untouched.

### Interrupted publication

Replacement is atomic per file, not for the whole generation. An interruption
between file replacements can leave mixed assets, catalog and manifest data.
Verification can then fail the recorded hashes.

Restore a consistent generated-file set from backup or version control, or
regenerate to a different empty output directory. The generator does not
repair a mixed publication automatically. Preserve the inconsistent files
before recovery; do not edit ownership hashes to make verification pass.

The HTML template stays handwritten. It derives cards, modes, settings,
contents and counts from the generated JSON, grouping entries by `effectName`.
It never invents missing previews.
