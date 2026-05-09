# LedFx effect ports to QLC+ v4 RGB scripts

Sources researched from LedFx `main`:

- https://raw.githubusercontent.com/LedFx/LedFx/main/ledfx/effects/scan_and_flare.py
- https://raw.githubusercontent.com/LedFx/LedFx/main/ledfx/effects/energy2.py
- https://raw.githubusercontent.com/LedFx/LedFx/main/ledfx/effects/scan_multi.py
- https://raw.githubusercontent.com/LedFx/LedFx/main/ledfx/effects/glitch.py
- https://raw.githubusercontent.com/LedFx/LedFx/main/ledfx/effects/melt_and_sparkle.py

Assumptions for all ports:

- Use QLC+ v4 RGB scripts with `algo.usesAudio = true` and direct reads from the injected `audio` object.
- Treat LedFx's 1D strip as a primary axis. For 2D matrices, compute one color per primary index and replicate across the secondary axis. Add an `Axis` property when useful.
- Keep persistent state on `algo.*` so repeated `rgbMap(width, height, rgb, step, audio)` calls can carry positions, filters, buffers, and particle lists.
- Use packed RGB via `RGBUtil.rgb(r, g, b)`. For HSV-based ports, add/choose an HSV-to-RGB helper or use existing QLC+ utilities if available in the script environment.
- Clamp channel values before packing. LedFx often relies on NumPy arrays and later output clipping; QLC+ scripts should clamp explicitly.

## Shared helper design

Recommended helpers for the script ports:

- `clamp01(x)` for normalized values.
- `triangle(x) = 1 - Math.abs(2 * (x - Math.floor(x)) - 1)`.
- `sin01(x) = 0.5 + 0.5 * Math.sin(2 * Math.PI * x)`.
- `lerp(a, b, t)` and `smoothFilter(prev, input, rise, decay)`.
- `fillStripToMap(strip, width, height, axis)`.
- `blendAdd(a, b)` for additive scan overlaps.
- Optional small 1D blur for flare/strobe masks.

---

## 1. Scan and Flare (`scan_and_flare.py`)

### LedFx analysis

#### What it does visually

- A colored scanner eye sweeps along a 1D strip.
- The eye can bounce at strip ends or wrap around.
- Loud power events spawn short white flares/sparkles near the scanner edge.
- Sparkles drift away from the scan, fade by age, and wrap around the strip.
- The scanner can use a fixed color or sample from the LedFx gradient at the current scan position.
- Optional color-intensity mode dims the scan color when audio power is low.

#### Audio features used

- LedFx selects one function from `AudioReactiveEffect.POWER_FUNCS_MAPPING` through `frequency_range`.
- The chosen method is called as `getattr(data, self.power_func)()`.
- The returned power is multiplied by `2`.
- No onset, beat, melbank, spectrum, or novelty call is used.

#### Core algorithm

- `audio_data_updated()` computes:
  - `power = selected_power * 2`
  - `bar = power * multiplier`
- Each render:
  - Choose scan color from either fixed `color_scan` or gradient position `scan_pos / pixel_count`.
  - If enabled, multiply color by `min(1, power)`.
  - Compute base scan motion: `step_per_sec = pixel_count / 100 * speed`.
  - Compute frame movement: `step_size = passed_seconds * step_per_sec * bar`.
  - Move `scan_pos` forward/back depending on `returning`.
  - Bounce or wrap at boundaries.
  - Age existing sparkles.
  - If `power > sparkles_threshold * 2` and fewer than `sparkles_max` exist, create one sparkle at the trailing edge of the scan.
  - Fill background, draw scan segment, then add sparkles additively.

#### State between frames

- `scan_pos`: floating scanner position.
- `returning`: bounce direction.
- `power` and `bar`: last audio-driven power/speed multiplier.
- `sparkles`: active flare particles.
- `last_sparkle`: stored but not effectively used for throttling.
- Each sparkle stores `pos`, `width`, `speed`, `die_off`, `born`, `health`, and `alive`.

#### Config options

- `blur` inherited/available in schema, but not directly used by this file's render body.
- `mirror` inherited/available, not directly used in this render body.
- `bounce`
- `scan_width`
- `speed`
- `sparkles_max`
- `sparkles_size`
- `sparkles_time`
- `sparkles_threshold`
- `color_scan`
- `frequency_range`
- `multiplier`
- `color_intensity`
- `use_grad`
- Also relies on common gradient/background settings from `GradientEffect`, such as `background_color` and `background_brightness`.

#### Key math

- `power = selected_power * 2`
- `bar = power * multiplier`
- `step_per_sec = pixel_count / 100 * speed`
- `step_size = passed_seconds * step_per_sec * bar`
- `scan_width_pixels = max(1, pixel_count / 100 * scan_width)`
- Sparkle fade: `health = 1 - ((now - born) / die_off)`
- Sparkle drift: `pos += speed * frame_time * health`
- Sparkle trigger threshold: `power > sparkles_threshold * 2`

### QLC+ v4 implementation design

#### QLC+ audio API fields to use

Expose a `Frequency Range` property and map it to direct fields:

- `Low`: `audio.power.low`
- `Mid`: `audio.power.mid`
- `High`: `audio.power.high`
- `Total`: `audio.power.total`
- `Dominant`: `audio.power.dominantValue`
- `Bass trigger` alternative: `audio.bands.low.value` or `audio.power.detail.bass`

Recommended default: `audio.power.low`, because the LedFx default is `Lows (beat+bass)`.

Optional QLC-specific enhancement:

- Use `audio.bands.low.fired` as a sparkle gate if plain low power is too continuous, but the closest LedFx port should use the power threshold only.

#### Rendering approach

- Use primary length `N = axis === "Vertical" ? height : width`.
- Maintain a 1D `strip[N]` initialized to `backgroundColor * Background Brightness`.
- Draw the scan segment from `scan_pos` to `scan_pos + scanWidthPx`.
- Add sparkles into `strip` with wrapping.
- Copy the strip into the 2D map:
  - Horizontal: `map[y][x] = strip[x]` for all rows.
  - Vertical: `map[y][x] = strip[y]` for all columns.

#### State management

Store on `algo`:

- `algo.scanPos`
- `algo.returning`
- `algo.sparkles`
- `algo.lastMs`
- `algo.power`
- `algo.axis`
- `algo.lastSize` to reset state when dimensions change.

Each sparkle object:

- `{ pos, width, speed, bornMs, dieMs, health }`

#### Config properties

- `Axis`: `Horizontal|Vertical`
- `Bounce`: boolean
- `Scan Width`: `1..100`
- `Speed`: `0..100`
- `Sparkles Max`: `1..20`
- `Sparkles Size`: `1..30` percent of scan width
- `Sparkles Time`: `10..2000` ms
- `Sparkles Threshold`: `10..90`
- `Frequency Range`: `Low|Mid|High|Total|Dominant`
- `Multiplier`: `0..500` as `0..5`
- `Color Intensity`: boolean
- `Use Gradient`: boolean
- `Scan Color`
- `Background Color`
- `Background Brightness`

#### Key differences from LedFx

- LedFx has common `blur`/`mirror` properties from effect bases; this script can omit them initially unless QLC+ already has shared postprocessing.
- LedFx's sparkles are class instances; QLC+ stores plain JS objects in `algo.sparkles`.
- QLC+ should explicitly throttle sparkles if the threshold produces too many flares at 25 Hz. LedFx comments mention the lack of sparkle-per-second limiting.
- LedFx uses NumPy color arrays and implicit clipping; QLC+ should clamp additive sparkles to 255.
- If QLC+ gradients are unavailable as LedFx gradients, use `audio.colors.gradient[]` or script color properties and `RGBUtil.gradientColorAt(colors, t)`.

#### Estimated complexity

- Difficulty: medium.
- Estimated size: 120-180 lines.
- Main risk: tuning threshold and particle rate so it does not create a flare every frame during sustained bass.

---

## 2. Energy 2 (`energy2.py`)

### LedFx analysis

#### What it does visually

- A single bright triangular energy hump moves around the strip.
- The brightness peak is squared, so the center is sharp and tails are dim.
- Hue is uniform across the whole strip and drifts over time plus bass power.
- Saturation becomes false/low in the brightest region as bass increases, producing white-hot highlights.
- The look is a simple rolling pulse or wave rather than a particle effect.

#### Audio features used

- `data.lows_power(filtered=False)`.
- A LedFx smoothing filter created with `alpha_decay=0.1` and `alpha_rise=0.1`.
- No beat, onset, melbank, or full spectrum data.

#### Core algorithm

- On config update, initialize low-power filter.
- On audio update, store filtered low power.
- In `render_hsv()`:
  - `t1 = time(speed)`.
  - Create a ramp `v = linspace(0, 1, pixel_count)`.
  - Add a moving offset: `v += 2 * sin(t1 + reactivity * lows_power)`.
  - Wrap with modulo 1.
  - Apply triangle wave.
  - Square the result for brightness shaping.
  - Saturation mask is true where brightness is below a bass-dependent threshold.
  - Set hue to `lows_power + t1`, saturation to the mask, value to `v`.

#### State between frames

- `_lows_power`: filtered low-frequency power.
- `_lows_filter`: rise/decay smoothing filter.
- No positional state; time derives from LedFx helper.

#### Config options

- `speed`: `0.00001..1.0`, default `0.1`.
- `reactivity`: `0.00001..1.0`, default `0.2`.

#### Key math

- `v0 = (i / (N - 1) + 2 * sin(t1 + reactivity * lows_power)) % 1`
- `v = triangle(v0)^2`
- `saturation = v < (0.9 - (reactivity + 0.3) * lows_power)`
- `hue = lows_power + t1`

### QLC+ v4 implementation design

#### QLC+ audio API fields to use

- `audio.power.low` for low-frequency power.
- Optional alternative for a more LedFx-like bass value: `audio.power.detail.bass`.
- `audio.timing.consumerDtMs` for stable local phase advance, falling back to `Date.now()` deltas if missing.

#### Rendering approach

- Compute one HSV color per primary-axis index.
- Use `algo.phase = (algo.phase + dtSeconds * SpeedScale) % 1`.
- Convert HSV to RGB and replicate across the secondary axis.
- Keep the LedFx look as a 1D pulse on a 2D matrix; do not make it radial unless a later enhancement asks for true 2D.

#### State management

Store on `algo`:

- `algo.lowFiltered`
- `algo.phase`
- `algo.lastMs`
- `algo.lastSize`

Filter:

- `lowFiltered += (audio.power.low - lowFiltered) * filterAlpha`
- Use one property or fixed alpha around `0.1` to match LedFx.

#### Config properties

- `Axis`: `Horizontal|Vertical`
- `Speed`: `1..100` mapped to a small phase rate.
- `Reactivity`: `0..100`
- `Bass Smoothing`: optional, default matching `0.1`.
- Optional `Brightness`: `0..100` QLC enhancement.

#### Key differences from LedFx

- LedFx `time(speed)` handles phase internally; QLC+ must maintain `algo.phase`.
- LedFx saturation is a boolean array. QLC+ can use exact binary saturation (`0` or `1`) or soften it for smoother white highlights. Exact port should use binary.
- LedFx HSV hue can exceed `1`; QLC+ should wrap hue with `% 1`.
- The original recreates the ramp array each frame; QLC+ loops over indices directly.

#### Estimated complexity

- Difficulty: easy.
- Estimated size: 60-90 lines.
- Main risk: HSV helper availability and speed scaling.

---

## 3. Scan Multi (`scan_multi.py`)

### LedFx analysis

#### What it does visually

- Three independent scanner bars move on the same strip.
- Low, mid, and high bands each drive one scanner.
- Default colors are red for lows, green for mids, and blue for highs.
- Scanners are additively blended, so overlaps produce mixed colors.
- Each scanner can bounce or wrap.
- Optional gradient mode changes each scanner color by its current position.
- Optional color-intensity mode dims each scanner by its own band power.

#### Audio features used

Two selectable input modes:

- `Power` mode:
  - `data.lows_power() * 2`
  - `data.mids_power() * 2`
  - `data.high_power() * 2`
- `Melbank` mode:
  - `self.melbank_thirds(filtered=False)`
  - For each third: `2 * np.mean(third)`

Optional per-band damping filter:

- `create_filter(alpha_decay=decay, alpha_rise=attack)`

No onset or beat API calls.

#### Core algorithm

- Construct three `Scan` objects, one per band.
- On audio update:
  - Read powers from either power functions or melbank thirds.
  - Optionally filter each power.
  - Set `scan.bar = scan.power * multiplier`.
  - Choose color from fixed band color or gradient at current position.
  - Optionally multiply color by `min(1, scan.power)`.
- On render:
  - Compute shared `step_per_sec` and `scan_width_pixels`.
  - Fill background.
  - For each scan, move its position according to power and direction.
  - Bounce or wrap.
  - Add its color to the strip segment.

Important source quirk:

- `step_size` is initialized before the loop, then multiplied by each scan's `bar` inside the loop without being reset per scan. This means later scans may receive a movement value already scaled by previous scans. A clean port should probably compute `scanStep = baseStep * scan.bar` for each scan unless bug-for-bug compatibility is required.

#### State between frames

Global:

- `scans`: three scan objects.
- `last_time`.

Per scan:

- `scan_pos`
- `returning`
- `bar`
- `power_func`
- `power`
- `_p_filter`
- cached color and current color

#### Config options

- `blur` inherited/available, not used directly here.
- `mirror` inherited/available, not used directly here.
- `bounce`
- `scan_width`
- `speed`
- `color_low`
- `color_mid`
- `color_high`
- `multiplier`
- `color_intensity`
- `use_grad`
- `input_source`: `Power|Melbank`
- `attack`
- `decay`
- `filter`
- Common background/gradient settings.

#### Key math

- Power mode: `bandPower = powerCall() * 2`
- Melbank mode: `bandPower = 2 * mean(melbankThird)`
- Filtered mode: `bandPower = attackDecayFilter.update(bandPower)`
- `bar = bandPower * multiplier`
- `baseStep = passed_seconds * (pixel_count / 100 * speed)`
- Intended per-scan movement: `scanStep = baseStep * bar`
- `scan_width_pixels = max(1, pixel_count / 100 * scan_width)`

### QLC+ v4 implementation design

#### QLC+ audio API fields to use

For `Power` mode:

- Low scan: `audio.power.low`
- Mid scan: `audio.power.mid`
- High scan: `audio.power.high`

For `Spectrum` mode replacing LedFx melbank thirds:

- Low scan: average `audio.spectrum.low.values[]`
- Mid scan: average `audio.spectrum.mid.values[]`
- High scan: average `audio.spectrum.high.values[]`

Optional faster approximate fields:

- `audio.spectrum.low.mean`
- `audio.spectrum.mid.mean`
- `audio.spectrum.high.mean`

Timing:

- `audio.timing.consumerDtMs` or `Date.now()` delta.

#### Rendering approach

- Maintain `algo.scans = [{pos, returning, filteredPower}, ...]`.
- Initialize `strip` with background.
- For each band:
  - Get raw power.
  - Optional attack/decay filter.
  - Move scanner by `baseStep * power * multiplier`.
  - Add band color or gradient color into `strip` over `scanWidthPx`.
- Copy strip to all rows/columns based on `Axis`.

Use additive blending with clamping so overlaps remain visible:

- red + green = yellow
- green + blue = cyan
- red + blue = magenta

#### State management

Store on `algo`:

- `algo.scans[0..2].pos`
- `algo.scans[0..2].returning`
- `algo.scans[0..2].powerFiltered`
- `algo.lastMs`
- `algo.lastSize`

#### Config properties

- `Axis`: `Horizontal|Vertical`
- `Bounce`: boolean
- `Scan Width`: `1..100`
- `Speed`: `0..100`
- `Low Color`
- `Mid Color`
- `High Color`
- `Multiplier`: `0..500` mapped to `0..5`
- `Color Intensity`: boolean
- `Use Gradient`: boolean
- `Input Source`: `Power|Spectrum`
- `Filter`: boolean
- `Attack`: `1..100`
- `Decay`: `1..100`
- `Background Color`
- `Background Brightness`

#### Key differences from LedFx

- LedFx melbank thirds are not the same as QLC+ spectrum bands. Use `audio.spectrum.*.values`/`mean` as a practical equivalent.
- Fix the step-size quirk by computing each scanner movement independently. This is more predictable and likely what the LedFx author intended.
- LedFx base options `blur` and `mirror` are not essential for a first QLC+ script. Mirror can be added by copying primary index `i` to `N - 1 - i`.
- LedFx uses NumPy additive arrays; QLC+ should clamp after each add or before packing.

#### Estimated complexity

- Difficulty: medium.
- Estimated size: 120-170 lines.
- Main risk: tuning power normalization in spectrum mode.

---

## 4. Glitch (`glitch.py`)

### LedFx analysis

#### What it does visually

- A full-brightness, saturated 1D pattern of broken digital color bands.
- Hue is generated by modular arithmetic, producing discontinuous blocks and jumps.
- Saturation is shaped by two moving triangle-wave masks, producing white or pastel glitch streaks depending on the saturation threshold.
- Bass energy speeds up the internal timeline, so the pattern stutters or jumps faster on low-frequency energy.
- Brightness is always full (`value = 1`).

#### Audio features used

- `data.lows_power()`.
- No smoothing filter is applied in this effect.
- No beat, onset, melbank, or spectrum calls.

#### Core algorithm

On activation:

- Build normalized index arrays:
  - `i`: centered ramp from approximately `-0.5` to `0.5`.
  - `i2`: ramp from `0` to `5`.
  - `i3`: ramp from `-1` to `0`.
- Initialize `timestep` and real-time delta.

On audio update:

- Store `lows_power`.

On render:

- Advance `timestep` by elapsed nanoseconds.
- Add bass-reactive time: `lows_power * reactivity / speed * 1e9`.
- Compute multiple time values at different speed multipliers.
- Hue:
  - Start from centered index `i`.
  - Compute `m = 0.3 + triangle(t2) * 0.2`.
  - Compute `c = triangle(t3) * 10 + 4 * sin(t4)`.
  - `h = (i * c) % m + sin(t1)`.
- Saturation:
  - `s1 = triangle((i2 + t5) % 1)^2`
  - `s2 = triangle((t6 - i3) % 1)^4`
  - `s = 1 - triangle(s1 * s2)`
  - Clamp `s` to `[saturation_threshold, 1]`.
- Value is `1` everywhere.

#### State between frames

- Precomputed ramps `i`, `i2`, `i3`.
- `timestep`, `last_time`, `dt`.
- `_lows_power`.

#### Config options

- `speed`: `0.00001..10.0`, default `0.5`.
- `reactivity`: `0.00001..1.0`, default `0.2`.
- `saturation_threshold`: `0..1`, default `1`.

#### Key math

- Bass time boost: `timestep += lows_power * reactivity / speed * 1e9`.
- Modulus width: `m = 0.3 + triangle(t2) * 0.2`.
- Hue scale: `c = triangle(t3) * 10 + 4 * sin(t4)`.
- Hue: `h = ((centeredIndex * c) % m) + sin(t1)`.
- Saturation mask: `s = 1 - triangle(triangle((i2 + t5) % 1)^2 * triangle((t6 - i3) % 1)^4)`.

### QLC+ v4 implementation design

#### QLC+ audio API fields to use

- `audio.power.low` for bass speed modulation.
- Optional stronger transient modulation: `audio.bands.low.value`.
- Timing from `audio.timing.consumerDtMs` or `Date.now()` delta.

#### Rendering approach

- Compute the same procedural HSV expression per primary index.
- Replicate the 1D result across the secondary matrix dimension.
- Because value is always 1, optionally apply `audio.gate.brightnessFloor` or a `Brightness` property if needed for QLC show safety. For a faithful port, keep value at full.

#### State management

Store on `algo`:

- `algo.timestepMs` or normalized phase.
- `algo.lastMs`.
- `algo.lastSize`.
- Optional precomputed arrays are unnecessary in JS; compute `u`, `i2`, and `i3` from the loop index. If performance matters, cache `algo.ramps` per size.

#### Config properties

- `Axis`: `Horizontal|Vertical`
- `Speed`: `1..1000` mapped to `0.00001..10.0`
- `Reactivity`: `0..100`
- `Saturation Threshold`: `0..100`
- Optional `Brightness`: `0..100`

#### Key differences from LedFx

- LedFx default `saturation_threshold = 1` forces full saturation, hiding much of the white/pastel mask. A QLC default around `0.2..0.5` may be more visually useful, but exact compatibility should default to `1`.
- Python modulo for negative values yields non-negative remainders. JS `%` can be negative, so use `mod1(x) = ((x % 1) + 1) % 1` or a general positive modulo helper.
- LedFx uses nanosecond timestep and `HSVEffect.time()`. QLC+ should use normalized phase functions, not raw huge millisecond values, to avoid floating precision drift.

#### Estimated complexity

- Difficulty: medium.
- Estimated size: 90-130 lines.
- Main risk: matching LedFx's time helper and Python modulo behavior closely.

---

## 5. Melt and Sparkle (`melt_and_sparkle.py`)

### LedFx analysis

#### What it does visually

- A flowing lava/melt background with rainbow hue folds and dark gaps between molten sections.
- Bass rolls the hue and speeds/reverses the melt motion.
- Mid power widens or closes dark space between lava chunks.
- Percussive high-frequency onsets create bright white sparkle/strobe spans.
- Sparkle overlays blur and decay, leaving short white trails.

#### Audio features used

- `data.lows_power(filtered=False)`, smoothed with a filter.
- `data.mids_power(filtered=True)`, smoothed with a filter.
- `self.melbank_thirds()`, using `max() ** 2` for each third and clipping to `0..1`.
- `data.onset()` to trigger strobes.
- Random low-power direction flip when bass exceeds strobe cutoff.

#### Core algorithm

- On activation:
  - Precompute hue ramp.
  - Initialize time, direction, strobe overlay, and onset queue.
- On audio update:
  - Filter low and mid powers.
  - Occasionally reverse direction when low power exceeds cutoff.
  - Compute melbank third intensities.
  - If onset fires, cooldown has elapsed, and high intensity exceeds cutoff, enqueue a strobe.
- On render:
  - Advance `timestep` by real time and low-power reactive time, multiplied by direction.
  - Compute `t1 = time(speed * 20, timestep)`.
  - Compute `bass_factor = lows_power * reactivity * 0.5`.
  - Build hue/value fields from a reversed ramp passed through sine and triangle waves.
  - Shape the value field with a power curve controlled by `lava_width` and mid power.
  - Dim background lava by `bg_bright`.
  - If a strobe is queued, choose random position and width, set overlay to `1`.
  - Reduce saturation by overlay and add overlay to value, capped at `1`.
  - Decay and blur overlay.

#### State between frames

- `timestep`
- `last_time`
- `dt`
- `_lows_power`
- `_last_lows_power`
- `_mids_power`
- `_direction`
- `strobe_overlay` array
- `onsets_queue`
- `last_strobe_time`
- Derived config state: `strobe_cutoff`, `strobe_wait_time`, `strobe_decay_rate`, `strobe_blur`.

#### Config options

- `speed`
- `reactivity`
- `bg_bright`
- `lava_width`
- `strobe_threshold`
- `strobe_rate`
- `strobe_width`
- `strobe_decay_rate`
- `strobe_blur`

#### Key math

- Effective strobe cutoff: `strobe_threshold / 10`.
- Strobe cooldown: `1 - strobe_rate` seconds.
- Strobe decay multiplier: `1 - strobe_decay_rate`.
- Time advance:
  - `timestep += dt * direction`
  - `timestep += lows_power * reactivity * speed * 500000000 * direction`
- `bass_factor = lows_power * reactivity * 0.5`
- Hue:
  - reversed ramp -> sine -> add bass roll -> `triangle()` twice.
- Value:
  - repeated sine with `t1` offsets -> triangle -> bass offset -> triangle.
- Lava chunk width:
  - `width_factor = (1 - lava_width)^2`
  - `power = 30 * width_factor - mids_power * width_factor`
  - `v = v ^ power`
- Strobe width:
  - `int(strobe_width ^ 3 * pixel_count)`, clipped to `1..pixel_count-1`.
- Strobe compositing:
  - `s *= 1 - overlay`
  - `v = min(1, v + overlay)`

### QLC+ v4 implementation design

#### QLC+ audio API fields to use

- Bass motion and direction flips: `audio.power.low`.
- Mid lava shaping: `audio.power.mid`.
- Sparkle trigger: `audio.onset.fired`.
- Sparkle strength/cutoff: `audio.onset.intensity`.
- High-band gate equivalent to LedFx melbank high third:
  - Prefer `audio.spectrum.high.max` if populated.
  - Otherwise compute `max(audio.spectrum.high.values[])` and square it.
- Timing: `audio.timing.consumerDtMs` or `Date.now()` delta.

Optional helpful fields:

- `audio.gate.closed` to optionally keep only dim lava when no useful audio is present.
- `audio.colors.gradient[]` if a QLC-specific gradient mode is desired.

#### Rendering approach

- Primary-axis 1D strip, replicated to 2D.
- Compute HSV per primary index:
  - `u = 1 - i / max(1, N - 1)`.
  - `hBase = sin01(u)`.
  - Apply bass hue roll and double triangle fold.
  - Compute value from repeated `sin01`, time offsets, triangle folds, and power shaping.
  - Apply strobe overlay to saturation/value.
- Convert HSV to RGB.
- Copy strip into `rgbMap` output.
- Use a small blur radius for `strobeOverlay`; a simple box blur is acceptable.

#### State management

Store on `algo`:

- `algo.timestep`
- `algo.lastMs`
- `algo.direction`
- `algo.lowFiltered`
- `algo.midFiltered`
- `algo.strobeOverlay`
- `algo.lastStrobeMs`
- `algo.lastSize`

No queue is required because QLC+ receives a current audio snapshot during render; trigger the strobe inline when `audio.onset.fired` is true and cooldown has elapsed.

#### Config properties

- `Axis`: `Horizontal|Vertical`
- `Speed`: `1..100`
- `Reactivity`: `0..100`
- `Background Brightness`: `0..100`
- `Lava Width`: `0..100`
- `Strobe Threshold`: `0..100`
- `Strobe Rate`: `0..100`
- `Strobe Width`: `0..100`
- `Strobe Decay`: `0..100`
- `Strobe Blur`: `0..100`
- Optional `Direction Flip Chance`: QLC enhancement to tune the LedFx random `1/200` bass-triggered reversal.

#### Key differences from LedFx

- LedFx uses a producer/consumer onset queue because audio and render are separate callbacks. QLC+ can handle onset directly in `rgbMap()`.
- LedFx divides `strobe_threshold` by `10`, so a UI value of `0.75` becomes `0.075`. A QLC+ script should either document this compatibility behavior or use a true `0..1` threshold. For user clarity, prefer true `0..1`.
- LedFx uses `smooth()` for blur. QLC+ can approximate with one or more box-blur passes.
- LedFx melbank high intensity is not exactly QLC+ high spectrum max. `audio.spectrum.high.max` is the closest direct field.
- Random direction reversal should be rate-limited or based on `dt` so behavior is stable across frame rates.

#### Estimated complexity

- Difficulty: hard relative to the other four.
- Estimated size: 170-240 lines.
- Main risks: matching the lava field visually and keeping strobe blur efficient in JavaScript.

---

## Recommended implementation order

1. `Energy 2` — smallest, validates HSV helpers and 1D-to-2D mapping.
2. `Glitch` — pure procedural HSV with simple bass time modulation.
3. `Scan Multi` — validates additive RGB scanner state and audio band mapping.
4. `Scan and Flare` — adds particle lifecycle on top of scanner motion.
5. `Melt and Sparkle` — most complex combination of HSV fields, onset strobes, blur, random reversal, and multiple audio features.

## Testing notes for future script implementation

- Add deterministic helper tests for `triangle`, positive modulo, filter response, and 1D-to-2D mapping if RGB script tests exist.
- Use non-trivial audio fixtures: low-only, mid-only, high-only, onset-only, and combined bass+onset. Ensure the five effects react differently to those inputs.
- Manually preview on both narrow strips and rectangular matrices because replication can look different at low width.
- Verify that state resets when matrix dimensions change.
