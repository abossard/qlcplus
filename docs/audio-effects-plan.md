Got it — switching to design-doc mode. The 5 `.js` files I already wrote are still on disk as a reference implementation, but the canonical artifact for the GPT-5.5 review is the plan below. Treat the JS as a sketch, not a commitment.

---

## Effect 1 — Audio Bass Laser

**One-liner:** Bright laser beams streak across the matrix on bass attacks, leaving glowing trails that fade as they cross.

**Visual:** A mostly-dark field with sharp 1-pixel-wide bright lines moving fast (≈3–10 px/frame) across the matrix. Each beam has a 6–20 frame trail with quadratic falloff and a soft halo on the two adjacent rows/cols. On strong bass, multiple beams co-exist; in `CrissCross` mode they shoot diagonally from corners. Highs cause the trail glow to flicker brighter, giving an "energy bloom" feel.

**Audio mapping:**
- `LedFx.lows_power(audio)` — passed through `ExpFilter(0.2, 0.7)`. Rising-edge above threshold spawns a beam; its magnitude sets the beam's speed.
- `LedFx.high_power(audio)` — modulates trail glow multiplier per frame (no per-beam state, just a global brightness scalar).
- Idle activity: when `beams.length < maxBeams/2` AND filtered bass > 0.4, auto-spawn even without a hard attack — prevents a static, all-black screen during sustained but flat bass.

**Algorithm:**
```
beam = { axis: 0|1|2, pos|x,y, speed|vx,vy, perp, hue, life }

each frame:
  bass = filter.update(lows_power(audio)) * (1 + gain*0.5)
  if (bass - prevBass) > thresh OR (beams.length < max/2 AND bass > 0.4):
    spawnBeam(strength = max(0.4, bass))
  prevBass = bass

  for each beam:
    advance pos along axis (or vx/vy for diagonal)
    if beam off-screen by > trailLen: remove
    for t in 0..trailLen-1:
      fall = (1 - t/trailLen)^2          // quadratic falloff
      bri  = fall * glow * beam.life
      paint pixel at (pos - speed*t/|speed|) additively
      paint halo at perp±1 with bri * 0.35
```

Key math: `(1 - t/trail)^2` gives the laser its sharp head + smooth tail. Halo on adjacent rows is the cheapest way to fake bloom without a true blur pass.

**Parameters:**
- `Sensitivity` 1–10 (default 8) — lowers spawn threshold
- `Gain` 1–10 (default 7) — band-power amplifier (separate axis from sensitivity!)
- `Trail Length` 1–20 (default 8)
- `Max Beams` 1–16 (default 6) — hard cap, oldest beam evicted on overflow
- `Direction` Horizontal / Vertical / Both / CrissCross
- `Color Mode` Gradient (uses 2 picker colors) / Rainbow (HSV cycles per beam, hue shifts along trail)

**Coding considerations:**
- **State:** `beams[]` array persists across frames; cap with FIFO eviction (`shift()` when over max).
- **Edge case:** beam spawned just before threshold change → axis stored on beam, never re-evaluated.
- **Performance:** O(beams × trail) per frame. With max=16 and trail=20 that's 320 pixel ops + 640 halo ops, trivial. No per-pixel loop over the whole matrix.
- **Additive blend** (not overwrite) so overlapping beams brighten — important for the "wall of light" feel during heavy bass.
- **Diagonal axis** uses `vx, vy` rather than `pos, perp` — needs a separate spawn branch (corner picker via 2-bit `corner` mask).
- **Filter init lazy** (first frame creates filter) so Sensitivity changes don't require a re-init flag.

---

## Effect 2 — Audio Fireworks

**One-liner:** Particle explosions whose size, color and speed are chosen by which frequency band fired.

**Visual:** Center (or bottom, or random) origin emits expanding rings of particles. Bass → 30–35 fat slow particles in warm reds/oranges with long life. Mids → 24 medium yellow-green particles. Highs → 18 small fast cool sparkles that die quickly. Particles arc under gravity, additive blend so overlaps go white-hot. Multiple bursts can co-exist.

**Audio mapping:**
- 3 separate `ExpFilter` instances on `lows/mids/high_power`.
- Rising-edge per band → corresponding burst type (decoupled, can fire in same frame).
- Ambient sparkle: when `audio.volume > 0.03` and `particles.length < 20`, emit a tiny random-hue burst — keeps the matrix from going dead during sustained quiet sections.

**Algorithm:**
```
particle = { x, y, vx, vy, hue, life, maxLife }

burst(count, speed, hueBase, hueSpread, life):
  for i in count:
    angle = random * 2π
    v     = speed * (0.4 + 0.6*random)        // velocity jitter
    push { x:ox, y:oy, vx:cos(angle)*v, vy:sin(angle)*v,
           hue: (hueBase + (random-0.5)*hueSpread) mod 1,
           life, maxLife: life }

each frame:
  low  = filter * gain (clamp 1)
  mid  = filter * gain
  high = filter * gain
  if (low-prevLow) > thresh OR (low > 0.55 AND rand<0.2):  big warm burst
  if (mid-prevMid) > thresh OR (mid > 0.50 AND rand<0.15): medium burst
  if (high-prevHigh) > thresh*0.7 OR (high>0.45 AND rand<0.25): sparkle burst
  if vol > 0.03 AND particles.length < 20: ambient burst (random hue)

  for each particle (reverse iteration for safe splice):
    x += vx; y += vy; vy += gravity
    life -= 1
    if life<=0 or off-screen: drop
    t  = life/maxLife
    bri = 0.4 + 0.9*t                   // brightness floor of 0.4 → punchy
    color = hsv2rgb(hue, 1, min(1, bri))
    additive blend at round(x), round(y)
```

Key math: gravity = `gravity_preset * 0.012` per frame (≈0.036 default at 25Hz). Brightness floor of 0.4 (instead of pure `t`) is the user's punch fix — particles stay visible until the very last frame.

**Parameters:**
- `Sensitivity` 1–10 (default 8)
- `Gain` 1–10 (default 7)
- `Gravity` 0–10 (default 3) — 0 = particles drift forever; 10 = mortar shells
- `Max Particles` 20–500 (default 200)
- `Origin` Center / Bottom / Random

**Coding considerations:**
- **Reverse iteration** when removing dead particles (`for i = len-1; i >= 0`); critical correctness gotcha.
- **Cap with FIFO** on burst — `while (particles.length > max) shift()` keeps memory bounded; bursts of 35 with cap of 200 means ~5 bursts visible simultaneously.
- **Edge case:** particle spawned outside bounds (e.g. with bottom origin and high vy) — clipped on draw, not on push, so off-screen-going particles still consume a slot until life expires. Acceptable, but noting it.
- **Hue jitter** uses `(hueBase + jitter + 1) % 1` — `+1` matters because jitter can be negative.
- **Additive blend** is mandatory: a bass + mid + high burst at the same origin should saturate to white at the core.
- **No matrix clear** between frames is implicit because we always start from `LedFx.createMap()` (zero-filled). If we ever cache the map for trails, that changes everything — *don't*.

---

## Effect 3 — Audio Hue Shift

**One-liner:** Whole matrix glows one smoothly-shifting color, hue chosen by which frequency range dominates.

**Visual:** A solid wash of color filling every pixel. When bass dominates, screen is amber/red. When mids dominate, green/cyan. When highs dominate, blue/magenta. Transitions are buttery (filter alpha ~0.05). In `Pulse` vignette mode, a soft ring expands from center on each beat; in `Strong` mode there's a permanent radial darkening toward edges.

**Audio mapping:**
- Hue = power-weighted average of three reference hues:
  ```
  targetHue = (0.02*low + 0.35*mid + 0.70*high) / (low + mid + high + ε)
  ```
  Bass weight = 0.02 (red/amber zone in HSV), mids = 0.35 (green/cyan), highs = 0.70 (blue/magenta).
- `hueFilter = ExpFilter(α, α)` where α derived from Smoothing preset; same alpha for rise and decay so it's symmetric.
- Brightness rides `audio.volume` through a separate filter; floor at `Min Brightness / 10`.
- Beat → vignette ring radius (in `Pulse` mode), filtered to soft expansion.

**Algorithm:**
```
each frame:
  low, mid, high = lows/mids/high_power(audio)
  hue   = hueFilter.update(targetHue(low, mid, high))
  vol   = brightFilter.update( min(1, audio.volume * gain) )
  bri   = min(1, minB + vol*(1-minB)*1.8)        // lifted floor
  beat  = beatFilter.update(audio.beat ? 1 : 0)

  for each pixel (x, y):
    b = bri
    if vignette == Strong:
       b *= 1 - 0.7 * dist_from_center / maxR
    elif vignette == Pulse:
       ring = abs(d/maxR - beat)
       b   *= (1 - min(1, ring*1.2)) * 0.5 + b * 0.5
    rgb = hsv2rgb(hue, sat, b)
    map[y][x] = pack(rgb)
```

Key math: weighted average over hue **values** is wrong if any of low/mid/high are negative or if total approaches 0. The `+ε` denominator handles the latter. The 0.02/0.35/0.70 reference points are spaced unevenly to skip the brown/yellow no-man's-land between red and green.

**Parameters:**
- `Smoothing` 1–10 (default 6) → α = 0.05 + (10−smoothing)*0.05
- `Saturation` 0–10 (default 10)
- `Vignette` Off / Pulse / Strong
- `Min Brightness` 0–10 (default 5)
- `Gain` 1–10 (default 7)

**Coding considerations:**
- **Filter dirty flag** — Smoothing change requires re-creating ExpFilter (different α). Implement `filterDirty = true` in setter; check at top of `rgbMap`.
- **No state arrays** — this is the only one of the 5 with O(width × height) per-frame and no persistent per-pixel state. Simplest of the five.
- **HSV wraparound:** when hue smooths from 0.95 → 0.05 the filter would interpolate through 0.5 (the wrong way around the wheel). For now we accept this — it's a quick "swing through opposite color." A circular filter (filter `sin(hue*2π)` and `cos(hue*2π)` separately) would fix this; flag for rubber-duck whether the swing is artifact or feature.
- **Vignette in Pulse mode** has an odd formula `(1 - min(1, ring*1.2)) * 0.5 + b * 0.5` — it's a 50/50 mix of "ring darkness" and base brightness so the beat doesn't black out the rest of the screen. Worth reviewing.
- **Performance:** width*height pixel ops + sqrt per pixel. On a 64×32 = 2048 pixels at 25 Hz = 51k ops/sec. Fine. Could precompute `dist[y][x]/maxR` if matrix size doesn't change.

---

## Effect 4 — Audio Shockwave

**One-liner:** Bass hits emit expanding rings (or bars) that ripple outward and overlap additively.

**Visual:** Concentric rings of glowing color expand from center. Each ring has a soft thickness (2–8 px) with peak brightness on the ring line, falling off to either side. As the wave expands, color shifts from `startColor` to `endColor` (radius-driven). Multiple waves overlap → heavy bass produces intense crossing-rings interference. Modes: `Circular` (rings), `Horizontal` (left+right bars expanding from center column), `Vertical` (up+down bars).

**Audio mapping:**
- `LedFx.lows_power(audio) * gain` filtered through `ExpFilter(0.2, 0.8)` (fast attack, slow decay).
- Rising-edge spawns a wave OR auto-spawn when no waves alive + bass > 0.35 (idle keep-alive).
- Wave color/intensity at any frame = function of `radius` and `strength` at spawn.
- Ambient center glow scales with `audio.volume` — center pixel never fully dies during playback.

**Algorithm:**
```
wave = { radius, strength }   // strength frozen at spawn

each frame:
  bass = filter.update(lows_power(audio)) * gain
  if (bass-prev) > thresh OR (waves==0 AND bass>0.35) OR
     (waves<max/2 AND bass>0.55):
    waves.push({ radius:0, strength: max(0.5, bass) })

  if vol > 0.02:
    paint center glow:                                   // never-black floor
      for y, x: blend additively color * vol * (1 - d/maxR/2)

  for each wave (reverse):
    wave.radius += speed
    if radius > maxRad + thickness: drop
    t = radius / maxRad
    color = lerp(startColor, endColor, t)
    amp   = strength * (1 - t)                           // fade as it expands

    if mode == Circular:
      for each pixel (y, x):
        d  = sqrt((x-cx)^2 + (y-cy)^2)
        dr = abs(d - radius)
        if dr <= thickness:
          f = (1 - dr/thickness) * amp                    // triangle falloff
          additive blend color * f
    elif mode == Horizontal:
      for x in 0..width:
        dx = abs(x - cx); ddx = abs(dx - radius)
        if ddx <= thickness:
          paint full column with amp * (1 - ddx/thickness)
```

Key math: `dr = abs(d - radius)` is the perpendicular distance to the ring; triangle falloff `1 - dr/thickness` is cheaper than gaussian and looks fine. The two-bar mirror in `Horizontal` mode is automatic from `dx = abs(x - cx)` — no need to track left/right separately.

**Parameters:**
- `Sensitivity` 1–10 (default 8)
- `Gain` 1–10 (default 7)
- `Wave Speed` 1–10 (default 5) → 0.55–2.8 px/frame
- `Thickness` 1–8 (default 2)
- `Mode` Circular / Horizontal / Vertical
- `Max Waves` 1–12 (default 6)

**Coding considerations:**
- **Circular mode is O(width × height × waves)** — on 64×32 with 6 waves that's 12k ops/frame. Still cheap, but if matrix gets to 128×128, reconsider (early-exit by computing radius bounds: only iterate annulus `radius±thickness`).
- **Strength frozen at spawn** is intentional — if we kept reading current bass, waves would change brightness mid-flight, which looks weird.
- **Ambient center glow** runs O(width × height) every frame regardless of wave count. Acceptable; can be disabled by setting volume threshold higher.
- **Edge case:** with very small matrix (e.g. 4×4), `maxRad` is small, so waves cycle through quickly. Don't cap minimum maxRad — feature, not bug.
- **Mode switch mid-flight:** waves spawned in Circular mode are still rendered correctly in Horizontal mode because rendering reads `algo.presetMode` each frame (not stored on wave). Confirmed by re-reading my code.
- **Color interpolation in linear RGB**, not sRGB — fine for this use case but flag for review if a perfectionist wants gamma-correct lerp.

---

## Effect 5 — Audio Split Tower

**One-liner:** Three vertical sections of the matrix display bass / mids / highs as colored bars rising from a baseline, with peak-hold.

**Visual:** Matrix divided into thirds horizontally. Left third = red bars rising with bass, middle = green with mids, right = blue with highs. Bars rise from bottom (default) / fall from top / mirror from center. A bright peak-hold dot hangs at the recent maximum of each section, slowly drifting down. In `Bands` mode each column within a section reflects per-column mel data, so the section becomes a mini-spectrum with a tinted color, not a flat block.

**Audio mapping:**
- Three powers (`lows`, `mids`, `high`) filtered together via `ExpFilter.updateArray([low, mid, high])` (single filter, 3-element vector).
- Each scaled by `(sensitivity/5) * (1 + gain*0.5)`, clamped to [0,1], then floored to `minFloor` when audio detected.
- Per-column variant: `LedFx.melbank(audio, sectionWidth)` gives a per-column mel value used as `lev` in `Bands` mode.
- Peak-hold = max of (peak − decayRate, currentLevel) per section.

**Algorithm:**
```
sections = [ [0..s1], [s1..s2], [s2..width] ]   // s1=w/3, s2=2w/3
sectionColors = [ red, green, blue ]
peaks = [0, 0, 0]   // persistent across frames

each frame:
  levels = filter.updateArray([ lows_power, mids_power, high_power ])
  scale  = (sensitivity/5) * (1 + gain*0.5)
  hasAudio = audio.volume > 0.02 OR sum(levels) > 0.05
  for i in 0..2:
    levels[i] = clamp(levels[i] * scale)
    if hasAudio: levels[i] = max(levels[i], floor)

  for i in 0..2:
    peaks[i] = max(peaks[i] - decayRate, levels[i])

  if bandsMode:
    melLow  = melbank(audio, s1)
    melMid  = melbank(audio, s2-s1)
    melHigh = melbank(audio, width-s2)

  for sec in 0..2:
    color = sectionColors[sec]
    for x in section[sec]:
      lev = bandsMode ? mel[sec][x - sec.start] * scale : levels[sec]
      h   = round(lev * height)
      paintColumn(x, h, origin, color)
      if peakHold:
        ph = round(peaks[sec] * height)
        paintPeak(x, ph, origin, color + 80,80,80)        // brighter dot

paintColumn(x, h, origin, color):
  for dy in 0..h-1:
    y = origin==Bottom ? height-1-dy
      : origin==Top    ? dy
      : center-mirror(dy, h)
    bri = 1 - (dy/h)*0.25                                  // lightly fade tip
    set pixel
```

Key math: center-mirror needs `half = ceil(h/2)`; first `half` pixels go up from center, rest go down. Off-by-one on odd `h` lands cleanly with `floor(height/2) + dy` upward and `floor(height/2) - 1 - (dy-half)` downward.

**Parameters:**
- `Sensitivity` 1–10 (default 8)
- `Gain` 1–10 (default 7)
- `Peak Decay` 1–10 (default 4) → 0.005–0.05 per frame
- `Bar Origin` Bottom / Top / Center
- `Peak Hold` Off / On
- `Per-Section Detail` Solid (one level per section) / Bands (per-column melbank)

**Coding considerations:**
- **Section width with non-divisible widths:** `s1 = floor(width/3)`. On width=10 → sections are [0,3] [3,6] [6,10] — last one is 4 wide. Acceptable asymmetry; never a section of zero (verify width ≥ 3).
- **Edge case width < 3:** sections collapse and `melbank(audio, 0)` would crash. Guard with `Math.max(1, sectionWidth)`.
- **Filter is a 3-element vector** via `updateArray` — confirmed `LedFx.ExpFilter.updateArray()` supports this.
- **Peak decay** is an absolute rate per frame, not multiplicative, so peaks decay linearly (looks more like a real VU meter than exp decay).
- **`hasAudio` floor** is the user's punch fix: floor of `2/10 = 0.2` means even quiet audio shows ~20% bar height in every section.
- **Center mode + odd height:** mirror is asymmetric (one extra pixel goes up). Acceptable; documenting.
- **Performance:** O(width × maxBarHeight) ≈ O(width × height). On 32×16 = 512 ops, trivial.
- **Color customization:** currently hardcoded R/G/B per section. Question for rubber-duck: should we expose 3 colors via `acceptColors = 3`? Argument for: user customization. Argument against: bass=red is a strong convention; 3 color pickers clutter UI.

---

## Cross-cutting design questions for GPT-5.5 review

1. **Gain vs Sensitivity** — split into two presets in all 5. Is that the right axis? Or should we collapse to one "Reactivity" knob with a non-linear curve?
2. **Idle/ambient activity** — is auto-spawning beams/waves/particles when audio is quiet-but-not-silent a feature or a hack? Especially for Bass Laser, where the laser-show metaphor traditionally implies discrete triggers.
3. **HSV hue filter wraparound** in Hue Shift (interpolates the long way) — fix with sin/cos circular filter, or accept as visual quirk?
4. **Peak decay rate** in Split Tower — linear vs exponential. Linear gives the classic VU look; exponential is smoother but less dramatic.
5. **Color picker contracts** — Bass Laser uses 2 colors (gradient ends), Shockwave 2 colors, Hue Shift 0 (HSV-derived), Fireworks 0 (HSV per particle), Split Tower 0 (hardcoded R/G/B). Should we standardize, or is "color picker behavior depends on effect" acceptable?
6. **Threading** — all 5 use module-level `var` for persistent state (beams[], particles[], peaks[], filter). Each instance of an RGB Matrix using the script gets its own JS context, right? Worth confirming with QLC+ engine docs before shipping; if shared, state collisions between matrices will be ugly.

The 5 JS files are on disk at `resources/rgbscripts/audio{basslaser,fireworks,hueshift,shockwave,splittower}.js` and registered in CMakeLists.txt — they validate the design end-to-end and can be discarded/rewritten after rubber-duck without losing the algorithmic content captured above.