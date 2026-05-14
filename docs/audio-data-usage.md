# QLC+ Audio API v2 Reference

42 audio-reactive RGB scripts. 16 flat properties on the `audio` argument.

## API Surface

```js
audio = {
    // Power bands (0–1, post-AGC, smoothed)
    beat,            // sub-bass 0-100Hz energy
    bass,            // bass 100-250Hz energy
    low,             // composite lows ≤250Hz (= (beat+bass)/2)
    mid,             // 250-3000Hz
    high,            // 3000-10000Hz

    // Events (booleans, true for one frame)
    onset,           // transient detected (Schmitt trigger)
    onsetIntensity,  // 0-1 strength of onset
    beatFired,       // tempo grid hit
    downbeat,        // first beat of bar

    // Tempo / timing
    bpm,             // detected BPM (0 if no audio)
    phase,           // 0→1 sawtooth, 1 cycle/beat
    barPhase,        // 0→1 sawtooth, 1 cycle/bar
    dt,              // beats elapsed since last frame (0 if no BPM)
    cosPulse,        // max(0, cos(phase * PI)) — smooth 0→1 beat pulse

    // Meta
    version          // = 5
}
```

## Usage by Property

| Property | Scripts | Count |
|----------|---------|-------|
| `audio.low` | audio_colors, audioaurora, audiobasslaser, audioblocks, audioblurz, audiocellular, audiochaser, audiocrawler, audiodjlight, audioenergy, audioenergy2, audioequalizer, audiofire, audiofireworks, audioflowfield, audioglitch, audioglitch2, audiogravimeter, audiohueshift, audiolava, audiomelt, audiomeltsparkle, audioplasma, audiopower, audiopuddles, audioreaction, audioreactor, audioscanflare, audioscanmulti, audioshockwave, audiosoap, audiospectrum, audiosplittower, audiotunnel, audiovortex, audiowater | 36 |
| `audio.dt` | audioaurora, audiobarcode, audioblocks, audioblurz, audiobuildup, audiocellular, audiochaser, audiocrawler, audiodjlight, audioenergy2, audiofire, audioflowfield, audioglitch, audioglitch2, audiogravimeter, audiolava, audiomelt, audiomeltsparkle, audioplasma, audiopuddles, audioreaction, audioreactor, audioscan, audioscanflare, audioscanmulti, audioshockwave, audiosoap, audiostrobe, audiotunnel, audiovortex | 30 |
| `audio.high` | audio_colors, audioaurora, audiobarcode, audiobasslaser, audioblurz, audiochaser, audiodjlight, audioenergy, audioequalizer, audiofireworks, audioflowfield, audioglitch, audiogravimeter, audiohueshift, audiomeltsparkle, audioplasma, audiopower, audiopuddles, audioreaction, audioreactor, audioscan, audioscanmulti, audiosoap, audiospectrum, audiosplittower, audiotunnel, audiovortex, audiowater | 28 |
| `audio.mid` | audio_colors, audioaurora, audioblurz, audiochaser, audiodjlight, audioenergy, audioequalizer, audiofireworks, audioflowfield, audioglitch, audiogravimeter, audiohueshift, audiomeltsparkle, audioplasma, audiopower, audiopuddles, audioreaction, audioreactor, audioscan, audioscanmulti, audiosoap, audiospectrum, audiosplittower, audiowater | 24 |
| `audio.bpm` | audiobarcode, audiobeatcolors, audioblurz, audiocellular, audiochaser, audiofire, audioflowfield, audioglitch, audioglitch2, audiogravimeter, audiomeltsparkle, audiopuddles, audioreaction, audioscan, audioscanflare, audioscanmulti, audioshockwave, audiosoap, audiostrobe | 19 |
| `audio.beatFired` | audiobasslaser, audiobuildup, audiocellular, audiochaser, audiofireworks, audioflowfield, audioglitch, audiogravimeter, audiopuddles, audioreaction, audioshockwave, audioshot, audiostrobe | 13 |
| `audio.onset` | audiobarcode, audioglitch, audiomeltsparkle, audiopower, audiopuddles, audioreactor, audioshockwave, audioshot, audiostrobe, audiotunnel, audiovortex | 11 |
| `audio.onsetIntensity` | audiobarcode, audiobasslaser, audioblurz, audiofireworks, audioglitch, audiopuddles, audioreactor, audioshockwave, audioshot, audiotunnel, audiovortex | 11 |
| `audio.cosPulse` | audioaurora, audiobeatcolors, audiobuildup, audiochaser, audioenergy, audiohueshift, audioreactor, audiosplittower, audiotunnel, audiovortex | 10 |
| `audio.beat` | audioblurz, audioequalizer, audioplasma, audiopower, audioscan, audiosoap | 6 |
| `audio.bass` | audioblurz, audioequalizer, audioplasma, audiopower, audioscan, audiosoap | 6 |
| `audio.downbeat` | audiobeatcolors, audiocellular, audioequalizer | 3 |
| `audio.barPhase` | audiobeatcolors, audioreactor | 2 |
| `audio.phase` | audiobeatcolors | 1 |

## Per-Script Summary

| Script | File | Properties used |
|--------|------|----------------|
| Audio Aurora | `audioaurora.js` | cosPulse, dt, high, low, mid |
| Audio Barcode | `audiobarcode.js` | bpm, dt, high, onset, onsetIntensity |
| Audio Bass Laser | `audiobasslaser.js` | beatFired, high, low, onsetIntensity |
| Audio Beat Colors | `audiobeatcolors.js` | barPhase, bpm, cosPulse, downbeat, phase |
| Audio Blocks | `audioblocks.js` | dt, low |
| Audio Blurz | `audioblurz.js` | bass, beat, bpm, dt, high, low, mid, onsetIntensity |
| Audio Buildup | `audiobuildup.js` | beatFired, cosPulse, dt |
| Audio Cellular | `audiocellular.js` | beatFired, bpm, downbeat, dt, low |
| Audio Chaser | `audiochaser.js` | beatFired, bpm, cosPulse, dt, high, low, mid |
| Audio Crawler | `audiocrawler.js` | dt, low |
| Audio DJ Light | `audiodjlight.js` | dt, high, low, mid |
| Audio Energy | `audioenergy.js` | cosPulse, high, low, mid |
| Audio Energy 2 | `audioenergy2.js` | dt, low |
| Audio Equalizer | `audioequalizer.js` | bass, beat, downbeat, high, low, mid |
| Audio Fire | `audiofire.js` | bpm, dt, low |
| Audio Fireworks | `audiofireworks.js` | beatFired, high, low, mid, onsetIntensity |
| Audio Flow Field | `audioflowfield.js` | beatFired, bpm, dt, high, low, mid |
| Audio Glitch | `audioglitch.js` | beatFired, bpm, dt, high, low, mid, onset, onsetIntensity |
| Audio Glitch 2 | `audioglitch2.js` | bpm, dt, low |
| Audio Gravimeter | `audiogravimeter.js` | beatFired, bpm, dt, high, low, mid |
| Audio Hue Shift | `audiohueshift.js` | cosPulse, high, low, mid |
| Audio Lava Lamp | `audiolava.js` | dt, low |
| Audio Melt | `audiomelt.js` | dt, low |
| Audio Melt and Sparkle | `audiomeltsparkle.js` | bpm, dt, high, low, mid, onset |
| Audio Plasma | `audioplasma.js` | bass, beat, dt, high, low, mid |
| Audio Power | `audiopower.js` | bass, beat, high, low, mid, onset |
| Audio Puddles | `audiopuddles.js` | beatFired, bpm, dt, high, low, mid, onset, onsetIntensity |
| Audio Reaction-Diffusion | `audioreaction.js` | beatFired, bpm, dt, high, low, mid |
| Audio Reactor | `audioreactor.js` | barPhase, cosPulse, dt, high, low, mid, onset, onsetIntensity |
| Audio Scan | `audioscan.js` | bass, beat, bpm, dt, high, mid |
| Audio Scan and Flare | `audioscanflare.js` | bpm, dt, low |
| Audio Scan Multi | `audioscanmulti.js` | bpm, dt, high, low, mid |
| Audio Shockwave | `audioshockwave.js` | beatFired, bpm, dt, low, onset, onsetIntensity |
| Audio Shot | `audioshot.js` | beatFired, onset, onsetIntensity |
| Audio Soap | `audiosoap.js` | bass, beat, bpm, dt, high, low, mid |
| Audio Spectrum | `audiospectrum.js` | high, low, mid |
| Audio Split Tower | `audiosplittower.js` | cosPulse, high, low, mid |
| Audio Strobe | `audiostrobe.js` | beatFired, bpm, dt, onset |
| Audio Tunnel | `audiotunnel.js` | cosPulse, dt, high, low, onset, onsetIntensity |
| Audio Vortex | `audiovortex.js` | cosPulse, dt, high, low, onset, onsetIntensity |
| Audio Water | `audiowater.js` | high, low, mid |

## Common Patterns

### Beat-locked oscillator
```js
state.phase = (state.phase + audio.dt * speed) % 1.0;
```

### Smooth beat pulse
```js
var brightness = 1.0 + amplitude * audio.cosPulse;
```

### Onset flash
```js
if (audio.onset) spawnEffect(audio.onsetIntensity);
```

### Power-weighted color blend
```js
var total = audio.low + audio.mid + audio.high + 0.001;
var h = colors[0].h * (audio.low/total) + colors[1].h * (audio.mid/total) + colors[2].h * (audio.high/total);
```
