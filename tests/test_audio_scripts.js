#!/usr/bin/env node
/*
  Boundary harness for the installed QLC+ audio-reactive Hue scripts.
  It mirrors the current hsvutil.js, flat audio object, palette, and
  Float32Array HSV contracts used by RGBScript.
*/

'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const SCRIPTS_DIR = path.join(__dirname, '..', 'resources', 'huescripts');
const HSV_UTIL = fs.readFileSync(path.join(SCRIPTS_DIR, 'hsvutil.js'), 'utf8');
const EXPECTED_SCRIPT_SPECS = Object.freeze({
    'audiobands.js': { name: 'Audio Bands', usesAudio: true },
    'audiobandsmatrix.js': { name: 'Audio Bands Matrix', usesAudio: true },
    'audiobladepower.js': { name: 'Audio Blade Power', usesAudio: true },
    'audiobleep.js': { name: 'Audio Bleep', usesAudio: true },
    'audiobpmbar.js': { name: 'Audio BPM Bar', usesAudio: true },
    'audioconcentric.js': { name: 'Audio Concentric', usesAudio: true },
    'audioaurora.js': { name: 'Audio Aurora', usesAudio: true },
    'audiobarcode.js': { name: 'Audio Barcode', usesAudio: true },
    'audiobasslaser.js': { name: 'Audio Bass Laser', usesAudio: true },
    'audiobeatcolors.js': { name: 'Audio Beat Colors', usesAudio: true },
    'audioblocks.js': { name: 'Audio Blocks', usesAudio: true },
    'audioblurz.js': { name: 'Audio Blurz', usesAudio: true },
    'audiobuildup.js': { name: 'Audio Buildup', usesAudio: true },
    'audiocellular.js': { name: 'Audio Cellular', usesAudio: true },
    'audiochaser.js': { name: 'Audio Chaser', usesAudio: true },
    'audiocrawler.js': { name: 'Audio Crawler', usesAudio: true },
    'audiodigitalrain.js': { name: 'Audio Digital Rain', usesAudio: true },
    'audiodjlight.js': { name: 'Audio DJ Light', usesAudio: true },
    'audioenergy.js': { name: 'Audio Energy', usesAudio: true },
    'audioenergy2.js': { name: 'Audio Energy 2', usesAudio: true },
    'audioequalizer.js': { name: 'Audio Equalizer', usesAudio: true },
    'audioequalizer2d.js': { name: 'Audio Equalizer 2D', usesAudio: true },
    'audiofilter.js': { name: 'Audio Filter', usesAudio: true },
    'audiofire.js': { name: 'Audio Fire', usesAudio: true },
    'audioflame.js': { name: 'Audio Flame', usesAudio: true },
    'audiofireworks.js': { name: 'Audio Fireworks', usesAudio: true },
    'audioflowfield.js': { name: 'Audio Flow Field', usesAudio: true },
    'audiogameoflife.js': { name: 'Audio Game of Life', usesAudio: true },
    'audioglitch.js': { name: 'Audio Glitch', usesAudio: true },
    'audioglitch2.js': { name: 'Audio Glitch 2', usesAudio: true },
    'audiogravimeter.js': { name: 'Audio Gravimeter', usesAudio: true },
    'audiohierarchy.js': { name: 'Audio Hierarchy', usesAudio: true },
    'audiohueshift.js': { name: 'Audio Hue Shift', usesAudio: true },
    'audiolava.js': { name: 'Audio Lava Lamp', usesAudio: true },
    'audiomagnitude.js': { name: 'Audio Magnitude', usesAudio: true },
    'audiomarching.js': { name: 'Audio Marching', usesAudio: true },
    'audiomelt.js': { name: 'Audio Melt', usesAudio: true },
    'audiomeltsparkle.js': { name: 'Audio Melt and Sparkle', usesAudio: true },
    'audiomulticolorbar.js': { name: 'Audio Multicolor Bar', usesAudio: true },
    'audionoise.js': { name: 'Audio Noise', usesAudio: true },
    'audiopitchspectrum.js': { name: 'Audio Pitch Spectrum', usesAudio: true },
    'audioplasma.js': { name: 'Audio Plasma', usesAudio: true },
    'audiopower.js': { name: 'Audio Power', usesAudio: true },
    'audiopuddles.js': { name: 'Audio Puddles', usesAudio: true },
    'audioreaction.js': { name: 'Audio Reaction-Diffusion', usesAudio: true },
    'audioreactor.js': { name: 'Audio Reactor', usesAudio: true },
    'audioscan.js': { name: 'Audio Scan', usesAudio: true },
    'audioscanflare.js': { name: 'Audio Scan and Flare', usesAudio: true },
    'audioscanmulti.js': { name: 'Audio Scan Multi', usesAudio: true },
    'audioshockwave.js': { name: 'Audio Shockwave', usesAudio: true },
    'audioshot.js': { name: 'Audio Shot', usesAudio: true },
    'audiosoap.js': { name: 'Audio Soap', usesAudio: true },
    'audiosmoke.js': { name: 'Audio Smoke', usesAudio: true },
    'audiospectralblocks.js': { name: 'Audio Spectral Blocks', usesAudio: true },
    'audiospotlight.js': { name: 'Audio Spotlight', usesAudio: true },
    'audiospectrum.js': { name: 'Audio Spectrum Bars', usesAudio: true },
    'audiosplittower.js': { name: 'Audio Split Tower', usesAudio: true },
    'audiostrobe.js': { name: 'Audio Strobe', usesAudio: true },
    'audiotunnel.js': { name: 'Audio Tunnel', usesAudio: true },
    'audiovumeter.js': { name: 'Audio VuMeter', usesAudio: true },
    'audiovortex.js': { name: 'Audio Vortex', usesAudio: true },
    'audiowater.js': { name: 'Audio Water', usesAudio: true },
    'audiowaterfall.js': { name: 'Audio Waterfall', usesAudio: true },
    'huefade.js': { name: 'Hue Fade', usesAudio: false },
    'huegradient.js': { name: 'Hue Gradient', usesAudio: false },
    'huemetro.js': { name: 'Hue Metro', usesAudio: false },
    'huepixels.js': { name: 'Hue Pixels', usesAudio: false },
    'huerainbow.js': { name: 'Hue Rainbow', usesAudio: false },
    'huerandomflash.js': { name: 'Hue Random Flash', usesAudio: false },
    'huesinglecolor.js': { name: 'Hue Single Color', usesAudio: false }
});
const EXPECTED_SCRIPTS = Object.keys(EXPECTED_SCRIPT_SPECS)
    .filter(file => EXPECTED_SCRIPT_SPECS[file].usesAudio)
    .sort();
const EXPECTED_NON_AUDIO_SCRIPTS = Object.keys(EXPECTED_SCRIPT_SPECS)
    .filter(file => !EXPECTED_SCRIPT_SPECS[file].usesAudio)
    .sort();
const REQUIRED_EXECUTED_CASE_IDS = Object.freeze([
    'identity-bijection',
    'existing-mode-manifest',
    'manifest-coverage',
    'melt-speed-zero',
    'melt-sparkle-aging',
    'pitch-spectrum-selection',
    'waterfall-selection',
    'digital-rain-motion',
    'rain-pulse-selection',
    'scroll-plus-fractional',
    'shared-helper-boundaries',
    'reference-core-vectors',
    'nonaudio-clock-boundary'
]);
const EXPECTED_EXISTING_MODE_OPTIONS = Object.freeze({
    'audiobarcode.js': ['Artistic', 'Reference', 'Scroll+'],
    'audioblocks.js': ['Artistic', 'LedFx Block Reflections'],
    'audiocrawler.js': ['Artistic', 'LedFx Crawler'],
    'audioenergy.js': ['Artistic', 'Reference'],
    'audioenergy2.js': ['Artistic', 'Reference', 'LedFx Energy 2'],
    'audioequalizer.js': ['Artistic', 'Segment Equalizer'],
    'audiofire.js': ['Artistic', 'LedFx Fire'],
    'audioglitch.js': ['Artistic', 'LedFx Glitch'],
    'audiolava.js': ['Artistic', 'LedFx Lava Lamp'],
    'audiomelt.js': ['Artistic', 'LedFx Melt'],
    'audiomeltsparkle.js': ['Artistic', 'LedFx Melt and Sparkle'],
    'audioplasma.js': ['Artistic', 'Plasma2d', 'PlasmaWled2d'],
    'audiopower.js': ['Artistic', 'LedFx Power'],
    'audiopuddles.js': ['Artistic', 'Rain Pulse'],
    'audioscan.js': ['Artistic', 'LedFx Scan'],
    'audioscanflare.js': ['Artistic', 'LedFx Scan and Flare'],
    'audioscanmulti.js': ['Artistic', 'LedFx Scan Multi'],
    'audiosoap.js': ['Artistic', 'LedFx Soap'],
    'audiospectrum.js': ['Artistic', 'Reference'],
    'audiostrobe.js': ['Artistic', 'Reference', 'Percussive RGB'],
    'audiowater.js': ['Artistic', 'LedFx Water']
});
const PINNED_REGISTRATION_MANIFEST = Object.freeze([
    { row: 1, id: 'bands', status: 'full', caseIds: ['row01.align-count-gradient', 'row01.split-cap', 'worker-a.output-metadata', 'worker-a.output-flip-mirror-order', 'worker-a.output-additive-before-brightness', 'worker-a.output-overdrive', 'worker-a.output-blur', 'worker-a.output-brightness', 'worker-a.regression.blade-state-key', 'worker-a.regression.blade-identity-retain-empty', 'worker-a.regression.blade-identity-reset-new-epoch', 'worker-a.regression.blade-source-double-tail', 'worker-a.regression.bandsmatrix-mirror', 'worker-a.regression.spectral-overdrive'] },
    { row: 2, id: 'bands_matrix', status: 'full', caseIds: ['row02.matrix-branches', 'worker-a.output-metadata', 'worker-a.output-flip-mirror-order', 'worker-a.output-additive-before-brightness', 'worker-a.output-overdrive', 'worker-a.output-blur', 'worker-a.output-brightness', 'worker-a.regression.blade-state-key', 'worker-a.regression.blade-identity-retain-empty', 'worker-a.regression.blade-identity-reset-new-epoch', 'worker-a.regression.blade-source-double-tail', 'worker-a.regression.bandsmatrix-mirror', 'worker-a.regression.spectral-overdrive'] },
    { row: 3, id: 'bar', status: 'full', caseIds: ['row03-bar', 'row03-bar-x', 'row03-bpm-reset-geometry', 'row03-bpm-flattened-n'] },
    { row: 4, id: 'blade_power_plus', status: 'full', caseIds: ['row04.blade-branches', 'worker-a.output-metadata', 'worker-a.output-flip-mirror-order', 'worker-a.output-additive-before-brightness', 'worker-a.output-overdrive', 'worker-a.output-blur', 'worker-a.output-brightness', 'worker-a.regression.blade-state-key', 'worker-a.regression.blade-identity-retain-empty', 'worker-a.regression.blade-identity-reset-new-epoch', 'worker-a.regression.blade-source-double-tail', 'worker-a.regression.bandsmatrix-mirror', 'worker-a.regression.spectral-overdrive'] },
    { row: 5, id: 'bleep', status: 'full', caseIds: ['row05-bleep'] },
    { row: 6, id: 'blender', status: 'excluded', caseIds: [] },
    { row: 7, id: 'block_reflections', status: 'full', caseIds: ['row07.blocks-formula-rgb', 'row07.blocks-flattened-n', 'row07.blocks-raw-low-source', 'row07.blocks-artistic-decoy', 'hreg.lava.artistic-state-isolated', 'hreg.crawler.t3-wall-field-clock', 'hreg.blocks.zero-dt-stable', 'hreg.blocks.elapsed-alpha-cadence', 'hreg.water.no-drift-at-zero-time', 'hreg.water.tiny-n-transforms-applied'] },
    { row: 8, id: 'blocks', status: 'full', caseIds: ['row08.blocks-products', 'worker-a.output-metadata', 'worker-a.output-flip-mirror-order', 'worker-a.output-additive-before-brightness', 'worker-a.output-overdrive', 'worker-a.output-blur', 'worker-a.output-brightness', 'worker-a.regression.blade-state-key', 'worker-a.regression.blade-identity-retain-empty', 'worker-a.regression.blade-identity-reset-new-epoch', 'worker-a.regression.blade-source-double-tail', 'worker-a.regression.bandsmatrix-mirror', 'worker-a.regression.spectral-overdrive'] },
    { row: 9, id: 'clone', status: 'excluded', caseIds: [] },
    { row: 10, id: 'concentric', status: 'full', caseIds: ['row10-concentric-field-travel'] },
    { row: 11, id: 'crawler', status: 'full', caseIds: ['row11.crawler-chop-independence', 'row11.crawler-raw-low', 'row11.crawler-artistic-decoy', 'row11.crawler-speed0-safe', 'hreg.lava.artistic-state-isolated', 'hreg.crawler.t3-wall-field-clock', 'hreg.blocks.zero-dt-stable', 'hreg.blocks.elapsed-alpha-cadence', 'hreg.water.no-drift-at-zero-time', 'hreg.water.tiny-n-transforms-applied', 'row11.workerM.source-golden-small-grid', 'row11.workerM.raw-low-vs-decoy', 'row11.workerM.zero-dt-duplicate-stable', 'row11.workerM.matched-timeconstant-inputfilter', 'row11.workerM.mode-isolation-art-ledfx-art', 'row11.workerM.controls-and-guards'] },
    { row: 12, id: 'digitalrain2d', status: 'full', caseIds: ['row12-digitalrain2d', 'row12-digitalrain2d-output-x'] },
    { row: 13, id: 'energy', status: 'full', caseIds: ['worker-g-row13-energy-lengths-mixing', 'worker-g-row13-energy-sensitivity-cycler', 'worker-g-x-forwarding-all-five', 'worker-g-g-range-roll-forwarding'] },
    { row: 14, id: 'energy2', status: 'full', caseIds: ['worker-g-row14-energy2-source-triangle', 'worker-g-x-forwarding-all-five', 'worker-g-g-range-roll-forwarding'] },
    { row: 15, id: 'equalizer', status: 'full', caseIds: ['row15-equalizer-segment'] },
    { row: 16, id: 'equalizer2d', status: 'full', caseIds: ['row16-equalizer2d'] },
    { row: 17, id: 'fade', status: 'full', caseIds: ['nonaudio-clock-boundary'] },
    { row: 18, id: 'filter', status: 'full', caseIds: ['row18-filter-core', 'D-output-metadata-unique', 'D-output-magnitude-flip-mirror', 'D-output-filter-additive-before-brightness', 'D-output-pitch-blur-nonzero', 'D-output-identity-defaults', 'D-output-overdrive-brightness', 'row18-filter-identity-transient-vs-epoch-reset'] },
    { row: 19, id: 'fire', status: 'full', caseIds: ['row19.fire-lifecycle', 'row19.fire-raw-low-decoy', 'row19.fire-small-n-guard'] },
    { row: 20, id: 'flame2d', status: 'full', caseIds: ['C20/flame/populations+bands+dynamics+bounds'] },
    { row: 21, id: 'frontend', status: 'excluded', caseIds: [] },
    { row: 22, id: 'game_of_life', status: 'full', caseIds: ['C22/life/blinker-flip-return-wrap', 'C22/life/four-pattern-kicks+duplicate-guard', 'C22/life/health-source-decay-occupancy'] },
    { row: 23, id: 'gifplayer', status: 'excluded', caseIds: [] },
    { row: 24, id: 'glitch', status: 'full', caseIds: ['row24.glitch-saturation-threshold', 'row24.glitch-ignore-onset', 'row24.glitch-motion-and-speed0'] },
    { row: 25, id: 'gradient', status: 'full', caseIds: ['nonaudio-clock-boundary'] },
    { row: 26, id: 'hierarchy', status: 'full', caseIds: ['row26-hierarchy-retention'] },
    { row: 27, id: 'imagespin', status: 'excluded', caseIds: [] },
    { row: 28, id: 'keybeat2d', status: 'excluded', caseIds: [] },
    { row: 29, id: 'lava_lamp', status: 'full', caseIds: ['row29.lava-formula-rgb', 'row29.lava-contrast', 'row29.lava-raw-low', 'row29.lava-seconds-invariance', 'hreg.lava.artistic-state-isolated', 'hreg.crawler.t3-wall-field-clock', 'hreg.blocks.zero-dt-stable', 'hreg.blocks.elapsed-alpha-cadence', 'hreg.water.no-drift-at-zero-time', 'hreg.water.tiny-n-transforms-applied'] },
    { row: 30, id: 'magnitude', status: 'full', caseIds: ['row30-magnitude-s-g-x', 'D-output-metadata-unique', 'D-output-magnitude-flip-mirror', 'D-output-filter-additive-before-brightness', 'D-output-pitch-blur-nonzero', 'D-output-identity-defaults', 'D-output-overdrive-brightness'] },
    { row: 31, id: 'marching', status: 'full', caseIds: ['row31-marching', 'row31-marching-x', 'row31-marching-reset-geometry', 'row31-marching-palette-saturation'] },
    { row: 32, id: 'melt', status: 'full', caseIds: ['row32.melt-raw-motion-stable-brightness', 'row32.melt-speed0-finite'] },
    { row: 33, id: 'melt_and_sparkle', status: 'full', caseIds: ['row33.melt-sparkle-hit', 'row33.melt-sparkle-no-onset', 'row33.melt-sparkle-spectral-threshold', 'row33.melt-sparkle-artistic-threshold'] },
    { row: 34, id: 'metro', status: 'full', caseIds: ['nonaudio-clock-boundary'] },
    { row: 35, id: 'multiBar', status: 'full', caseIds: ['row35-multibar', 'row35-multibar-x', 'row35-multibar-reset-geometry', 'row35-multibar-flattened-n'] },
    { row: 36, id: 'noise2d', status: 'full', caseIds: ['C36/noise/3d-pixel-oracle+decay+unavailable', 'C36+C54/output/x-metadata-defaults-identity', 'C36+C54/identity/audio-key-retain-unavailable-reset-on-epoch', 'C36/output/flip-then-paired-max-mirror', 'C36/output/nonzero-blur-gaussian', 'C36/output/overdrive-preserved-under-brightness'] },
    { row: 37, id: 'number', status: 'excluded', caseIds: [] },
    { row: 38, id: 'pitchSpectrum', status: 'full', caseIds: ['row38-pitch-spectrum-bin-blend', 'D-output-metadata-unique', 'D-output-magnitude-flip-mirror', 'D-output-filter-additive-before-brightness', 'D-output-pitch-blur-nonzero', 'D-output-identity-defaults', 'D-output-overdrive-brightness', 'row38-pitch-identity-transient-vs-epoch-reset', 'row38-workerL-zero-dt-stable', 'row38-workerL-constant-input-cadence-25-50-60', 'row38-workerL-nominal-one-step-formula', 'row38-workerL-blend-then-fade-order', 'row38-workerL-fade1-black', 'row38-workerL-fresh-positive-and-stale-p0-decay', 'row38-workerL-equal-p-different-valid-pitch-nonuniform-g', 'row38-workerL-stale-source-and-true-identity-reset', 'row38-workerL-metadata-and-bounds', 'row38-nontrivial-affine-cadence', 'row38-nontrivial-midi-ema-cadence'] },
    { row: 39, id: 'pixels', status: 'full', caseIds: ['nonaudio-clock-boundary'] },
    { row: 40, id: 'plasma2d', status: 'full', caseIds: ['row40-plasma2d'] },
    { row: 41, id: 'plasmawled', status: 'full', caseIds: ['row41-plasmawled2d', 'row41-plasmawled-bounds'] },
    { row: 42, id: 'power', status: 'full', caseIds: ['row42.power-source-components-and-events', 'workerI.row42-regression-decay-2x20ms-vs-1x40ms', 'workerI.row42-regression-filter-2x20ms-vs-1x40ms', 'workerI.row42-regression-zero-time-duplicate'] },
    { row: 43, id: 'radial', status: 'excluded', caseIds: [] },
    { row: 44, id: 'rain', status: 'partial', caseIds: ['row44.rain-pulse-enum-colors-regions-scaling', 'workerI.row44-regression-independent-band-threshold-controls'] },
    { row: 45, id: 'rainbow', status: 'full', caseIds: ['nonaudio-clock-boundary'] },
    { row: 46, id: 'random_flash', status: 'full', caseIds: ['nonaudio-clock-boundary'] },
    { row: 47, id: 'real_strobe', status: 'full', caseIds: ['worker-g-row47-percussive-rgb-branches', 'worker-g-x-forwarding-all-five', 'worker-g-g-range-roll-forwarding', 'worker-g-reg-row47-identity-transient-vs-rebind', 'worker-g-reg-row47-height-reset'] },
    { row: 48, id: 'scan', status: 'full', caseIds: ['row48.scan-wrap-bounce-width-input-g-x'] },
    { row: 49, id: 'scan_and_flare', status: 'full', caseIds: ['row49.scanflare-moving-particles-rgboverlap'] },
    { row: 50, id: 'scan_multi', status: 'full', caseIds: ['row50.scanmulti-band-speeds-melbank-cumulative-filter'] },
    { row: 51, id: 'scroll', status: 'full', caseIds: ['worker-g-row51-scroll-maxima-threshold-retention', 'worker-g-x-forwarding-all-five', 'worker-g-g-range-roll-forwarding', 'worker-g-reg-row51-seed-stable-at-zero-dt'] },
    { row: 52, id: 'scroll_plus', status: 'full', caseIds: ['worker-g-row52-scroll-plus-fractional-delta', 'worker-g-row52-scroll-plus-nondefault-x-forwarding', 'worker-g-x-forwarding-all-five', 'worker-g-g-range-roll-forwarding', 'worker-g-reg-row52-split-vs-single', 'worker-g-reg-row52-invalid-timing-recovery'] },
    { row: 53, id: 'singleColor', status: 'full', caseIds: ['nonaudio-clock-boundary'] },
    { row: 54, id: 'smoke2d', status: 'full', caseIds: ['C54/smoke/fbm4-oracle+zoom+decay+unavailable', 'C36+C54/output/x-metadata-defaults-identity', 'C36+C54/identity/audio-key-retain-unavailable-reset-on-epoch', 'C54/output/additive-before-brightness'] },
    { row: 55, id: 'soap2d', status: 'full', caseIds: ['row55-ledfx-soap'] },
    { row: 56, id: 'spectrum', status: 'full', caseIds: ['worker-g-row56-spectrum-six-permutations', 'worker-g-x-forwarding-all-five', 'worker-g-g-range-roll-forwarding'] },
    { row: 57, id: 'spotlight', status: 'full', caseIds: ['row57-spotlight', 'row57-spotlight-x', 'row57-spotlight-delta-alias-dedupe', 'row57-spotlight-carry-cap'] },
    { row: 58, id: 'strobe', status: 'full', caseIds: ['worker-g-row58-bpm-strobe-reference-controls', 'worker-g-x-forwarding-all-five', 'worker-g-g-range-roll-forwarding'] },
    { row: 59, id: 'texter2d', status: 'excluded', caseIds: [] },
    { row: 60, id: 'vumeter', status: 'full', caseIds: ['row60-vumeter-raw-zones-markers', 'D-output-metadata-unique', 'D-output-magnitude-flip-mirror', 'D-output-filter-additive-before-brightness', 'D-output-pitch-blur-nonzero', 'D-output-identity-defaults', 'D-output-overdrive-brightness'] },
    { row: 61, id: 'water', status: 'full', caseIds: ['row61.water-fractional-speed-floor', 'row61.water-processed-thirds', 'row61.water-propagate-before-insert', 'row61.water-small-n-guard', 'hreg.lava.artistic-state-isolated', 'hreg.crawler.t3-wall-field-clock', 'hreg.blocks.zero-dt-stable', 'hreg.blocks.elapsed-alpha-cadence', 'hreg.water.no-drift-at-zero-time', 'hreg.water.tiny-n-transforms-applied'] },
    { row: 62, id: 'waterfall2d', status: 'full', caseIds: ['row62-waterfall2d', 'row62-waterfall2d-output-x'] },
    { row: 63, id: 'wavelength', status: 'full', caseIds: ['worker-g-row63-wavelength-golden-roll', 'worker-g-x-forwarding-all-five', 'worker-g-g-range-roll-forwarding'] }
]);
const EDGE_BEHAVIOR_SCRIPTS = new Set([
    'audiomelt.js',
    'audiomeltsparkle.js',
    'audiodigitalrain.js',
    'audiopitchspectrum.js',
    'audiowaterfall.js'
]);
const DIMENSIONS = [[1, 1], [1, 9], [9, 1], [7, 11]];
const RESIZE_DIMENSIONS = [[7, 11], [12, 5]];
const PALETTE = [
    { h: 0.96, s: 1.0, v: 0.95 },
    { h: 0.33, s: 0.85, v: 0.75 },
    { h: 0.61, s: 0.70, v: 0.88 },
    { h: 0.12, s: 0.65, v: 0.80 },
    { h: 0.78, s: 0.90, v: 0.68 }
];
const BASE_AUDIO = Object.freeze({
    beat: 0, bass: 0, low: 0, mid: 0, high: 0,
    onset: false, onsetIntensity: 0, beatFired: false, downbeat: false,
    bpm: 120, phase: 0.37, barPhase: 0.59, dt: 0, cosPulse: 0.40,
    version: 5
});
const STIMULI = Object.freeze({
    silence: {},
    isolatedLow: { low: 0.83 },
    isolatedMid: { mid: 0.57 },
    isolatedHigh: { high: 0.31 },
    mixedUnequal: {
        beat: 0.91, bass: 0.73, low: 0.82, mid: 0.47, high: 0.23,
        dt: 0.04
    },
    onset: { high: 0.54, onset: true, onsetIntensity: 0.76, dt: 0.08 },
    beat: { low: 0.65, beatFired: true, dt: 0.08, cosPulse: 1 },
    clockedBeat: {
        version: 6,
        dt: 0.04,
        timing: { deltaSeconds: 0.02 },
        tempo: { valid: true, bpm: 120, beatPhase: 0.37, barPhase: 0.59 }
    },
    downbeat: {
        low: 0.62, downbeat: true, beatFired: true, phase: 0,
        barPhase: 0, dt: 0.08, cosPulse: 1
    },
    lowLevel: {
        beat: 0.03, bass: 0.04, low: 0.05, mid: 0.03, high: 0.02,
        dt: 0.02
    },
    nominal: {
        beat: 0.51, bass: 0.39, low: 0.45, mid: 0.32, high: 0.18,
        dt: 0.04
    },
    peak: {
        beat: 1.0, bass: 0.86, low: 0.93, mid: 0.74, high: 0.61,
        dt: 0.08, cosPulse: 1
    },
    bpm60: { low: 0.52, mid: 0.31, high: 0.17, bpm: 60, dt: 0.02 },
    bpm120: { low: 0.52, mid: 0.31, high: 0.17, bpm: 120, dt: 0.04 },
    bpm180: { low: 0.52, mid: 0.31, high: 0.17, bpm: 180, dt: 0.06 },
    vumeter: {
        dt: 0.04,
        timing: { deltaSeconds: 1 / 50 },
        volume: { rawRms: 0.31622776601683794 }
    },
    pitchSpectrum: {
        dt: 0.04,
        timing: { deltaSeconds: 1 / 50 },
        sourceId: 'pitch-source',
        profileId: 7,
        sourceEpoch: 1,
        configRevision: 1,
        pitch: { valid: true, hz: 440, midi: 69, confidence: 0.8 },
        banks: { full: {
            count: 8,
            processed: [0.8, 0.6, 0.4, 0.2, 0.5, 0.7, 0.3, 0.1],
            novelty: [0.4, 0.3, 0.2, 0.1, 0.25, 0.35, 0.15, 0.05]
        } }
    },
    waterfall: {
        dt: 0.04,
        timing: { deltaSeconds: 1 / 50 },
        sourceId: 'waterfall-source',
        profileId: 8,
        sourceEpoch: 1,
        configRevision: 1,
        banks: { full: {
            count: 8,
            processed: [0.1, 0.2, 0.6, 0.4, 0.2, 0.1, 0.7, 0.3],
            novelty: [0.1, 0.2, 0.6, 0.4, 0.2, 0.1, 0.7, 0.3]
        } }
    }
});
const EVENT_STIMULUS = Object.freeze({
    'audiobarcode.js': 'onset',
    'audiobeatcolors.js': 'downbeat',
    'audiobpmbar.js': 'clockedBeat',
    'audioblurz.js': 'onset',
    'audiobuildup.js': 'beat',
    'audiofireworks.js': 'beat',
    'audiogameoflife.js': 'beat',
    'audioglitch.js': 'onset',
    'audiomulticolorbar.js': 'beat',
    'audiopitchspectrum.js': 'pitchSpectrum',
    'audiopuddles.js': 'onset',
    'audiovumeter.js': 'vumeter',
    'audiowaterfall.js': 'waterfall',
    'audioshockwave.js': 'onset',
    'audioshot.js': 'onset',
    'audiostrobe.js': 'onset'
});
const UNIFORM_BY_DESIGN = new Set([
    'audiobeatcolors.js',
    'audiofilter.js',
    'audiohierarchy.js',
    'audiostrobe.js'
]);
const WATER_BAND_CASES = Object.freeze([
    { width: 7, band: 'low', responsive: false },
    { width: 7, band: 'mid', responsive: false },
    { width: 7, band: 'high', responsive: true },
    { width: 12, band: 'low', responsive: true },
    { width: 12, band: 'mid', responsive: false },
    { width: 12, band: 'high', responsive: true },
    { width: 100, band: 'low', responsive: true },
    { width: 100, band: 'mid', responsive: true },
    { width: 100, band: 'high', responsive: true }
]);
const totals = {
    contractCases: 0,
    parameterWrites: 0,
    responseChecks: 0,
    contrastChecks: 0,
    deterministicChecks: 0,
    resizeChecks: 0,
    waterBandCases: 0
};
const executedCases = new Set();

function markCase(caseId) {
    executedCases.add(caseId);
}

function assertExecutedCaseCoverage(required, actual) {
    const missing = required.filter(id => !actual.has(id));
    assert.strictEqual(
        missing.length,
        0,
        `missing executed behavior cases: ${missing.join(', ')}`
    );
}

function validatePinnedManifest(manifest, actualCases) {
    assert.strictEqual(manifest.length, 63, 'pinned manifest must have 63 rows');
    const byId = new Set(manifest.map(entry => entry.id));
    assert.strictEqual(byId.size, 63, 'pinned manifest ids must be unique');
    const counts = manifest.reduce((acc, entry) => {
        acc[entry.status] = (acc[entry.status] || 0) + 1;
        return acc;
    }, {});
    assert.strictEqual(counts.full || 0, 53, 'pinned manifest full count mismatch');
    assert.strictEqual(counts.partial || 0, 1, 'pinned manifest partial count mismatch');
    assert.strictEqual(counts.excluded || 0, 9, 'pinned manifest excluded count mismatch');
    for (const entry of manifest) {
        assert(Array.isArray(entry.caseIds), `row ${entry.row}/${entry.id}: caseIds must be array`);
        if (entry.status === 'excluded') {
            assert.strictEqual(
                entry.caseIds.length,
                0,
                `row ${entry.row}/${entry.id}: excluded rows must not claim executable cases`
            );
            continue;
        }
        assert(entry.caseIds.length > 0, `row ${entry.row}/${entry.id}: missing executable case ids`);
        for (const caseId of entry.caseIds)
            assert(actualCases.has(caseId),
                `row ${entry.row}/${entry.id}: executable case id not run: ${caseId}`);
    }
}

function assertExistingModeManifest() {
    for (const [scriptFile, expectedModes] of Object.entries(EXPECTED_EXISTING_MODE_OPTIONS)) {
        const modeDescriptor = loadScript(scriptFile).properties
            .map(parseProperty)
            .find(property => property.name === 'mode');
        assert(modeDescriptor, `${scriptFile}: missing mode selector`);
        assert.strictEqual(modeDescriptor.type, 'list', `${scriptFile}: mode must be list`);
        const actualModes = modeDescriptor.values.split(',');
        assert.deepStrictEqual(
            actualModes,
            expectedModes,
            `${scriptFile}: mode list mismatch`
        );
        const algo = loadScript(scriptFile);
        for (const mode of expectedModes) {
            assert.strictEqual(typeof algo.setMode, 'function', `${scriptFile}: setMode missing`);
            algo.setMode(mode);
            assert.strictEqual(algo.getMode(), mode, `${scriptFile}: set/get mode mismatch for ${mode}`);
        }
    }
    markCase('existing-mode-manifest');
}

function audio(overrides = {}) {
    return Object.assign({}, BASE_AUDIO, overrides);
}

function seededRandom(seed) {
    let value = seed >>> 0;
    return function() {
        value = (value * 1664525 + 1013904223) >>> 0;
        return value / 0x100000000;
    };
}

function sandboxDate(clock) {
    if (!clock) return Date;
    class ScriptDate extends Date {
        constructor(...args) {
            super(...(args.length ? args : [clock.now()]));
        }
        static now() {
            return clock.now();
        }
    }
    ScriptDate.UTC = Date.UTC;
    ScriptDate.parse = Date.parse;
    return ScriptDate;
}

function loadScript(scriptFile, seed = 0x5eed, directory = SCRIPTS_DIR, options = true) {
    const normalized = typeof options === 'boolean'
        ? { expectedUsesAudio: options, clock: null }
        : {
            expectedUsesAudio: options && options.expectedUsesAudio !== undefined
                ? options.expectedUsesAudio : true,
            clock: options && options.clock ? options.clock : null
        };
    const math = Object.create(Math);
    const random = options && typeof options === 'object' && options.random !== undefined
        ? options.random : seededRandom(seed);
    assert.strictEqual(typeof random, 'function', `${scriptFile}: random override must be a function`);
    Object.defineProperty(math, 'random', { value: random });
    const sandbox = {
        Math: math,
        Date: sandboxDate(normalized.clock),
        Float32Array,
        console: { log() {}, warn() {}, error() {} },
        testAlgo: null
    };
    vm.createContext(sandbox);
    vm.runInContext(HSV_UTIL, sandbox, { filename: 'hsvutil.js' });
    vm.runInContext(
        fs.readFileSync(path.join(directory, scriptFile), 'utf8'),
        sandbox,
        { filename: scriptFile }
    );

    const algo = sandbox.testAlgo;
    assert(algo, `${scriptFile}: testAlgo was not exported`);
    assert.strictEqual(algo.apiVersion, 3, `${scriptFile}: apiVersion`);
    assert.strictEqual(algo.usesAudio, normalized.expectedUsesAudio, `${scriptFile}: usesAudio`);
    assert.strictEqual(typeof algo.rgbMap, 'function', `${scriptFile}: rgbMap`);
    assert.strictEqual(
        typeof algo.rgbMapStepCount,
        'function',
        `${scriptFile}: rgbMapStepCount`
    );
    assert.strictEqual(
        typeof algo.rgbMapSetColors,
        'function',
        `${scriptFile}: rgbMapSetColors`
    );
    assert.strictEqual(
        typeof algo.rgbMapGetColors,
        'function',
        `${scriptFile}: rgbMapGetColors`
    );
    assert(
        Number.isInteger(algo.acceptColors) && algo.acceptColors >= 0,
        `${scriptFile}: acceptColors`
    );
    algo.colors = PALETTE.slice(0, Math.max(1, algo.acceptColors));
    algo.color = algo.colors[0];
    algo.hasUserColors = true;
    return algo;
}

function assertMapContract(scriptFile, map, width, height, label) {
    assert.strictEqual(
        Object.prototype.toString.call(map),
        '[object Float32Array]',
        `${scriptFile} ${label}: rgbMap must return Float32Array`
    );
    assert.strictEqual(
        map.length,
        width * height * 3,
        `${scriptFile} ${label}: wrong HSV map length`
    );
    for (let index = 0; index < map.length; index++) {
        const value = map[index];
        assert(
            Number.isFinite(value),
            `${scriptFile} ${label}: non-finite value at ${index}`
        );
        assert(
            value >= 0 && value <= 1,
            `${scriptFile} ${label}: HSV value ${value} outside [0,1] at ${index}`
        );
    }
    totals.contractCases++;
}

function render(algo, scriptFile, width, height, frame, label, step = 0) {
    const map = algo.rgbMap(width, height, algo.color, step, frame);
    assertMapContract(scriptFile, map, width, height, label);
    return Array.from(map);
}

function parseProperty(descriptor) {
    const result = {};
    for (const field of descriptor.split('|')) {
        const separator = field.indexOf(':');
        assert(separator > 0, `malformed property field: ${field}`);
        result[field.slice(0, separator)] = field.slice(separator + 1);
    }
    return result;
}

function propertyValues(property) {
    if (property.type === 'range') {
        const values = property.values.split(',').map(Number);
        assert.strictEqual(values.length, 2, `${property.name}: range needs min,max`);
        assert(values.every(Number.isFinite), `${property.name}: range values`);
        return values;
    }
    if (property.type === 'list') {
        const separator = property.values.includes(';') ? ';' : ',';
        return property.values.split(separator);
    }
    return [];
}

function assertPropertyContracts(scriptFile) {
    const metadataAlgo = loadScript(scriptFile);
    assert(
        Array.isArray(metadataAlgo.properties),
        `${scriptFile}: properties must be an array`
    );
    for (const descriptor of metadataAlgo.properties) {
        const property = parseProperty(descriptor);
        assert(
            property.name && property.type && property.write && property.read,
            `${scriptFile}: incomplete property descriptor`
        );
        const algo = loadScript(scriptFile);
        assert.strictEqual(
            typeof algo[property.write],
            'function',
            `${scriptFile}: missing ${property.write}`
        );
        assert.strictEqual(
            typeof algo[property.read],
            'function',
            `${scriptFile}: missing ${property.read}`
        );
        const initial = algo[property.read]();
        if (property.type === 'float' || property.type === 'string') {
            if (property.type === 'float') {
                assert(
                    Number.isFinite(Number(initial)),
                    `${scriptFile} ${property.name}: finite default`
                );
                algo[property.write](initial);
                let actual = Number(algo[property.read]());
                assert.strictEqual(
                    actual,
                    Number(initial),
                    `${scriptFile} ${property.name}: default round-trip`
                );
                const edgeValues = [];
                if (property.values) {
                    const parsed = property.values.split(',').map(Number).filter(Number.isFinite);
                    if (parsed.length === 2) edgeValues.push(parsed[0], parsed[1]);
                }
                if (edgeValues.length === 0) {
                    edgeValues.push(
                        0,
                        Number(initial) === 0 ? 1 : Number(initial) * 2,
                        -Math.abs(Number(initial) || 1)
                    );
                }
                for (const edge of edgeValues) {
                    algo[property.write](edge);
                    actual = Number(algo[property.read]());
                    assert(Number.isFinite(actual),
                        `${scriptFile} ${property.name}: edge write produced non-finite value`);
                    if (EDGE_BEHAVIOR_SCRIPTS.has(scriptFile))
                        render(
                            algo, scriptFile, 7, 11, audio(STIMULI.nominal),
                            `property ${property.name} edge ${edge}`
                        );
                    totals.parameterWrites++;
                }
                algo[property.write](initial);
            } else {
                algo[property.write](initial);
                const actual = algo[property.read]();
                assert.strictEqual(
                    String(actual),
                    String(initial),
                    `${scriptFile} ${property.name}: default round-trip`
                );
            }
            render(
                algo, scriptFile, 7, 11, audio(STIMULI.nominal),
                `property ${property.name} default`
            );
            totals.parameterWrites++;
        } else {
            const values = propertyValues(property);
            assert(values.length > 0, `${scriptFile} ${property.name}: declared values`);
            if (property.type === 'range') {
                assert(
                    Number(initial) >= values[0] &&
                    Number(initial) <= values[1],
                    `${scriptFile} ${property.name}: default in range`
                );
            } else {
                assert(
                    values.includes(String(initial)),
                    `${scriptFile} ${property.name}: default in list`
                );
            }
            for (const value of [values[0], values[values.length - 1]]) {
                algo[property.write](value);
                const actual = algo[property.read]();
                if (property.type === 'range') {
                    assert.strictEqual(
                        Number(actual), Number(value),
                        `${scriptFile} ${property.name}: range round-trip`
                    );
                } else {
                    assert.strictEqual(
                        String(actual), String(value),
                        `${scriptFile} ${property.name}: list round-trip`
                    );
                }
                render(
                    algo, scriptFile, 7, 11, audio(STIMULI.nominal),
                    `property ${property.name}=${value}`
                );
                totals.parameterWrites++;
            }
        }
    }
}

function sequence(
    scriptFile,
    stimulusName,
    seed = 0x9e3779b9,
    width = 7,
    height = 11
) {
    const algo = loadScript(scriptFile, seed);
    const event = stimulusName === 'onset' || stimulusName === 'beat' ||
        stimulusName === 'downbeat';
    const maps = [];
    for (let frameIndex = 0; frameIndex < 8; frameIndex++) {
        let overrides = stimulusName === 'silence'
            ? STIMULI.silence
            : STIMULI[stimulusName];
        if (event && frameIndex % 2 === 0) {
            overrides = Object.assign(
                {},
                overrides,
                { onset: false, beatFired: false, downbeat: false }
            );
        }
        maps.push(render(
            algo,
            scriptFile,
            width,
            height,
            audio(overrides),
            `${stimulusName} frame ${frameIndex}`
        ));
    }
    return maps;
}

function mapsDiffer(left, right) {
    for (let frame = 0; frame < left.length; frame++) {
        for (let index = 0; index < left[frame].length; index++) {
            if (Math.abs(left[frame][index] - right[frame][index]) > 1e-6)
                return true;
        }
    }
    return false;
}

function maxSpatialContrast(maps) {
    let contrast = 0;
    let peak = 0;
    for (const map of maps) {
        for (let channel = 0; channel < 3; channel++) {
            let min = Infinity;
            let max = -Infinity;
            for (let index = channel; index < map.length; index += 3) {
                min = Math.min(min, map[index]);
                max = Math.max(max, map[index]);
                if (channel === 2)
                    peak = Math.max(peak, map[index]);
            }
            contrast = Math.max(contrast, max - min);
        }
    }
    return { contrast, peak };
}

function assertWaterBandCoverage() {
    const details = [];
    let responsive = 0;
    for (const testCase of WATER_BAND_CASES) {
        const silence = render(
            loadScript('audiowater.js'),
            'audiowater.js',
            testCase.width,
            5,
            audio(),
            `isolated ${testCase.band} width ${testCase.width} silence`
        );
        const active = render(
            loadScript('audiowater.js'),
            'audiowater.js',
            testCase.width,
            5,
            audio({ [testCase.band]: 1 }),
            `isolated ${testCase.band} width ${testCase.width}`
        );
        const silencePeak = maxSpatialContrast([silence]).peak;
        const activePeak = maxSpatialContrast([active]).peak;
        const didRespond = mapsDiffer([silence], [active]);
        assert.strictEqual(
            didRespond,
            testCase.responsive,
            `audiowater.js: width ${testCase.width} isolated ` +
                `${testCase.band} response`
        );
        responsive += didRespond ? 1 : 0;
        totals.waterBandCases++;
        details.push(
            `${testCase.width}/${testCase.band}=` +
            `${activePeak.toFixed(9)}:${didRespond ? 'responsive' : 'silence'}`
        );
    }

    console.log(
        `WATER: isolated-band cases=${totals.waterBandCases} ` +
        `responsive=${responsive} ` +
        `nonresponsive=${totals.waterBandCases - responsive} ${details.join(' ')}`
    );
}

function assertScriptIdentityBijection() {
    const installed = fs.readdirSync(SCRIPTS_DIR)
        .filter(file => file.endsWith('.js') && file !== 'hsvutil.js')
        .sort();
    const expected = Object.keys(EXPECTED_SCRIPT_SPECS).sort();
    assert.deepStrictEqual(
        installed,
        expected,
        `installed huescripts inventory differs from the explicit ${expected.length}-script contract`
    );
    const names = new Set();
    for (const file of expected) {
        const spec = EXPECTED_SCRIPT_SPECS[file];
        const algo = loadScript(file, 0x5eed, SCRIPTS_DIR, {
            expectedUsesAudio: spec.usesAudio
        });
        assert.strictEqual(algo.name, spec.name, `${file}: algo.name mismatch`);
        assert(!names.has(spec.name), `duplicate algorithm name: ${spec.name}`);
        names.add(spec.name);
        const propertyNames = new Set();
        const displays = new Set();
        for (const descriptor of algo.properties) {
            const property = parseProperty(descriptor);
            const display = property.display || property.name;
            assert(!propertyNames.has(property.name), `${file}: duplicate property ${property.name}`);
            assert(!displays.has(display), `${file}: duplicate property display ${display}`);
            propertyNames.add(property.name);
            displays.add(display);
            if (property.values !== undefined) {
                assert(['list', 'range', 'float'].includes(property.type),
                    `${file}/${property.name}: unsupported native values descriptor`);
                assert(descriptor.indexOf('type:') < descriptor.indexOf('values:'),
                    `${file}/${property.name}: type must precede values`);
                if (property.type === 'range') {
                    const bounds = property.values.split(',').map(value => value.trim());
                    assert(bounds.length === 2 && bounds.every(value =>
                        /^[+-]?\d+$/.test(value) && Number(value) >= -2147483648 &&
                        Number(value) <= 2147483647) && Number(bounds[0]) <= Number(bounds[1]),
                    `${file}/${property.name}: native range bounds must be ordered integers`);
                } else if (property.type === 'float') {
                    const bounds = property.values.split(',').map(value => value.trim());
                    assert(bounds.length === 2 && bounds.every(value =>
                        /^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/.test(value) &&
                        Number.isFinite(Number(value))) && Number(bounds[0]) < Number(bounds[1]),
                    `${file}/${property.name}: native float bounds must be finite and increasing`);
                }
            }
        }
    }
    markCase('identity-bijection');
}

function assertFireworksTriggers() {
    const metadata = loadScript('audiofireworks.js');
    const triggerProperty = metadata.properties
        .map(parseProperty)
        .find(property => property.name === 'triggerMode');
    assert(triggerProperty, 'audiofireworks.js: triggerMode metadata');
    assert.strictEqual(
        triggerProperty.values,
        'Beat,Onset',
        'audiofireworks.js: only flat audio events may be declared'
    );
    for (const trigger of ['Beat', 'Onset']) {
        const run = eventActive => {
            const algo = loadScript('audiofireworks.js', 0x1234);
            algo.setTriggerMode(trigger);
            const maps = [];
            for (let frameIndex = 0; frameIndex < 4; frameIndex++) {
                const event = eventActive && frameIndex === 1
                    ? (trigger === 'Beat'
                        ? { beatFired: true }
                        : { onset: true, onsetIntensity: 0.8 })
                    : {};
                maps.push(render(
                    algo,
                    'audiofireworks.js',
                    7,
                    11,
                    audio(Object.assign({ dt: 0.08 }, event)),
                    `${trigger} trigger frame ${frameIndex}`
                ));
            }
            return maps;
        };
        assert(
            mapsDiffer(run(false), run(true)),
            `audiofireworks.js: ${trigger} list option did not reach its flat event`
        );
        totals.responseChecks++;
    }
}

function assertScript(scriptFile) {
    for (const [width, height] of DIMENSIONS) {
        const algo = loadScript(scriptFile);
        render(
            algo, scriptFile, width, height, audio(STIMULI.nominal),
            `dimension ${width}x${height}`
        );
    }
    const resizeAlgo = loadScript(scriptFile);
    for (const [width, height] of RESIZE_DIMENSIONS) {
        render(
            resizeAlgo, scriptFile, width, height, audio(STIMULI.peak),
            `resize ${width}x${height}`
        );
        totals.resizeChecks++;
    }
    for (const [name, stimulus] of Object.entries(STIMULI)) {
        const algo = loadScript(scriptFile);
        render(
            algo, scriptFile, 7, 11, audio(stimulus),
            `stimulus ${name}`
        );
    }
    if (scriptFile === 'audiofireworks.js')
        assertFireworksTriggers();
    assertPropertyContracts(scriptFile);

    const activeStimulus = EVENT_STIMULUS[scriptFile] || 'peak';
    const silent = sequence(scriptFile, 'silence');
    const active = sequence(scriptFile, activeStimulus);
    assert(
        mapsDiffer(silent, active),
        `${scriptFile}: promised audio hook did not change output for ${activeStimulus}`
    );
    totals.responseChecks++;
    const repeated = sequence(scriptFile, activeStimulus);
    assert.deepStrictEqual(
        active,
        repeated,
        `${scriptFile}: fresh seeded contexts produced order-dependent output`
    );
    totals.deterministicChecks++;

    const structureMaps = scriptFile === 'audiowater.js'
        ? sequence(scriptFile, activeStimulus, 0x9e3779b9, 12, 5)
        : active;
    const shape = maxSpatialContrast(structureMaps);
    assert(
        shape.peak > 1e-6,
        `${scriptFile}: active stimulus produced only black output`
    );
    if (!UNIFORM_BY_DESIGN.has(scriptFile)) {
        assert(
            shape.contrast > 1e-6,
            `${scriptFile}: spatial effect produced no HSV contrast`
        );
        totals.contrastChecks++;
    }
    markCase(`script:${scriptFile}`);
    return { activeStimulus, contrast: shape.contrast, peak: shape.peak };
}

async function compareReferenceEffects() {
    const zlib = require('zlib');
    const readline = require('readline');
    const directory = process.argv[process.argv.indexOf('--reference-effects') + 1];
    const requested = process.argv.includes('--only')
        ? process.argv[process.argv.indexOf('--only') + 1].split(',') : null;
    const manifest = JSON.parse(fs.readFileSync(path.join(__dirname, 'audio_reference_manifest.json')));
    const scripts = ['audiospectrum.js', 'audioenergy.js', 'audiobarcode.js',
        'audioenergy2.js', 'audiostrobe.js'];
    const reports = [];
    for (const file of fs.readdirSync(directory).filter(f => f.endsWith('.jsonl.gz'))) {
        if (requested && !requested.includes(file.replace('.jsonl.gz', ''))) continue;
        const lines = readline.createInterface({
            input: fs.createReadStream(path.join(directory, file)).pipe(zlib.createGunzip())
        });
        let header, algorithms, previous = {}, frames = 0;
        const statistics = {};
        for await (const line of lines) {
            const record = JSON.parse(line);
            if (!header) {
                header = record;
                if (!header.effects.length) break;
                algorithms = header.effects.map(definition => {
                    const algo = loadScript(scripts[manifest.effects.indexOf(definition.name)]);
                    assert.strictEqual(typeof algo.setMode, 'function',
                        `${definition.name}: explicit faithful mode missing`);
                    algo.setMode('Reference');
                    if (definition.name === 'strobe') algo.setReferenceClock('Extrapolated');
                    algo.referenceDiagnostics = true;
                    algo.setReferenceBlur(definition.config.blur);
                    algo.setReferenceMirror(definition.config.mirror ? 'On' : 'Off');
                    algo.setReferenceBrightness(definition.config.brightness);
                    if (definition.palette && algo.setReferencePalette) algo.setReferencePalette(definition.palette.join(','));
                    return algo;
                });
                continue;
            }
            const events = {};
            for (const [output, input] of Object.entries({onset: 'onset', beat: 'tempo', kick: 'kick', bar: 'bar_wrap'}))
                events[output] = record.events[input] - (previous[input] || 0);
            const frame = audio({
                version: 6, low: (record.powers[0] + record.powers[1]) / 2,
                mid: record.powers[2], high: record.powers[3], bpm: record.tempo_bpm || 120,
                phase: record.beat_phase, barPhase: record.bar_phase,
                timing: {deltaSeconds: frames ? 1 / 60 : 0},
                sourceEpoch: 1, frameSequence: frames + 1,
                tempo: {valid: record.tempo_bpm > 0, bpm: record.tempo_bpm,
                    beatPhase: record.beat_phase, barPhase: record.bar_phase},
                events: {delta: events},
                banks: {full: {count: 24, processed: record.banks[2].processed,
                    novelty: record.banks[2].novelty}}
            });
            record.effects.forEach((reference, index) => {
                const algo = algorithms[index];
                const map = algo.rgbMap(reference.width, reference.height, algo.color, 0, frame);
                assertMapContract(scripts[index >> 2], map, reference.width, reference.height, 'reference replay');
                const segment = header.segments.find(s => frames >= s.start_frame && frames < s.end_frame);
                for (const boundary of ['pre', 'transformed', 'final']) {
                    const actual = boundary === 'final' ? hsvPixels(map) : algo.referenceFrame[boundary];
                    assert.strictEqual(actual.length, reference[boundary].length);
                    const key = `${segment.start_frame}:${segment.end_frame}/${reference.name}/${reference.width}x${reference.height}/${boundary}`;
                    const stat = statistics[key] ||= {square: 0, errors: []};
                    actual.forEach((pixel, p) => pixel.forEach((channel, c) => {
                        const error = Math.abs(channel - reference[boundary][p][c]);
                        stat.errors.push(error);
                        stat.square += error * error;
                    }));
                }
            });
            previous = record.events;
            frames++;
        }
        if (!frames) continue;
        assert.strictEqual(frames, header.frame_count);
        const metrics = Object.entries(statistics).map(([key, stat]) => {
            stat.errors.sort((a, b) => a - b);
            const rmse = Math.sqrt(stat.square / stat.errors.length);
            const p99 = stat.errors[Math.floor((stat.errors.length - 1) * .99)];
            return {key, rmse, p99, max: stat.errors[stat.errors.length - 1],
                passed: rmse <= manifest.tolerances.linear_rgb_rmse && p99 <= manifest.tolerances.linear_rgb_p99};
        });
        const result = {fixture: header.fixture, frames, passed: metrics.every(m => m.passed), metrics};
        reports.push(result);
        console.log(JSON.stringify({...result, metrics: undefined,
            failures: metrics.filter(m => !m.passed).slice(0, 6)}));
    }
    assert(reports.length > 0, 'No real reference effects compared');
    const report = {producer: 'actual-hue-javascript-node', nativeQtVerified: false,
        passed: reports.every(r => r.passed), fixtures: reports.length, results: reports};
    if (process.argv.includes('--report'))
        fs.writeFileSync(process.argv[process.argv.indexOf('--report') + 1], JSON.stringify(report, null, 2) + '\n');
    if (!report.passed) process.exitCode = 1;
}

function hsvPixels(map) {
    const result = [];
    for (let i = 0; i < map.length; i += 3) {
        const h = map[i] * 6, s = map[i + 1], v = map[i + 2];
        const c = v * s, x = c * (1 - Math.abs(h % 2 - 1)), m = v - c;
        const rgb = [[c,x,0], [x,c,0], [0,c,x], [0,x,c], [x,0,c], [c,0,x]][Math.floor(h) % 6];
        result.push(rgb.map(channel => channel + m));
    }
    return result;
}

function assertReferenceCadence() {
    const scripts = ['audiospectrum.js', 'audioenergy.js', 'audiobarcode.js',
        'audioenergy2.js', 'audiostrobe.js'];
    const fresh = file => {
        const algo = loadScript(file);
        assert.strictEqual(algo.getMode(), 'Artistic', `${file}: saved default changed`);
        algo.setMode('Reference');
        algo.setReferenceBlur(0);
        algo.setReferenceMirror('Off');
        algo.setReferenceBrightness(1);
        algo.referenceDiagnostics = true;
        return algo;
    };
    const frame = (seconds, level, sequence) => audio({
        version: 6, sourceEpoch: 1, frameSequence: sequence,
        timing: {deltaSeconds: seconds}, banks: {full: {
            count: 24, processed: new Array(24).fill(level), novelty: new Array(24).fill(level * .2)
        }}, tempo: {bpm: 120, valid: true, beatPhase: .2, barPhase: .6}
    });
    for (const file of scripts) {
        const output = [30, 50, 60].map(hz => {
            const algo = fresh(file);
            algo.rgbMap(1000, 1, algo.color, 0, frame(0, .8, 1));
            let map, input;
            for (let n = 1; n <= hz / 5; n++) {
                input = frame(1 / hz, file === 'audioenergy2.js' ? .3 : 0, n + 1);
                input.tempo.beatPhase = n * 2 / hz;
                input.tempo.barPhase = n / (2 * hz);
                map = algo.rgbMap(1000, 1, algo.color, 0, input);
            }

            const pre = algo.referenceFrame.pre;
            // Trail position and brightness are distinct physical observations.
            const brightness = pre.map(p => Math.max(...p));
            const occupied = brightness.map((v, i) => v > 1e-9 ? i : -1).filter(i => i >= 0);
            const result = {amplitude: Math.max(...brightness), position: occupied.length ? occupied[0] : 0};
            input.timing.deltaSeconds = 0;
            const repeated = algo.rgbMap(1000, 1, algo.color, 0, input);
            assert.deepStrictEqual(Array.from(repeated), Array.from(map), `${file}: repeated read advances state`);
            assert(result.amplitude > 0, `${file}: cadence oracle must observe real energy`);
            return result;
        });
        for (const metric of ['amplitude', 'position']) {
            const values = output.map(o => o[metric]), largest = Math.max(...values), smallest = Math.min(...values);
            assert(largest === 0 || (largest - smallest) / largest <= .05,
                `${file}: ${metric} differs across 30/50/60Hz: ${values}`);
        }
        console.log(`REFERENCE_CADENCE ${file} ${JSON.stringify(output)}`);
    }
    const energy2 = fresh('audioenergy2.js');
    energy2.setReferencePalette('#ff0000,#00ff00,#0000ff');
    const midpointFrame = audio({
        version: 6,
        sourceEpoch: 9,
        frameSequence: 1,
        timing: { deltaSeconds: 0 },
        banks: { full: { count: 256, processed: new Array(256).fill(0), novelty: new Array(256).fill(1) } },
        tempo: { bpm: 120, valid: true, beatPhase: 0, barPhase: 0 }
    });
    energy2.rgbMap(256, 1, energy2.color, 0, midpointFrame);
    const midpoint = energy2.referenceFrame.pre[64];
    assert(Math.abs(midpoint[0] - 0.497) < 0.02 && Math.abs(midpoint[1] - 0.503) < 0.02 && midpoint[2] < 0.02,
        `audioenergy2.js: reference midpoint must be RGB interpolation, got ${JSON.stringify(midpoint)}`);

    const strobe = fresh('audiostrobe.js'), unclocked = frame(1 / 60, .8, 1);
    unclocked.tempo.valid = false;
    const dark = strobe.rgbMap(24, 1, strobe.color, 0, unclocked);
    assert(hsvPixels(dark).every(pixel => pixel.every(channel => channel === 0)),
        'Reference strobe must not treat fallback BPM as detected tempo');
    strobe.setReferenceClock('Extrapolated');
    assert(hsvPixels(strobe.rgbMap(24, 1, strobe.color, 0, unclocked)).some(pixel => pixel.some(channel => channel > 0)),
        'Explicit extrapolated reference clock must be exercised');
    if (process.argv.includes('--artistic-entry')) {
        const directory = process.argv[process.argv.indexOf('--artistic-entry') + 1];
        for (const file of scripts) {
            const before = loadScript(file, 0x5eed, directory), after = loadScript(file);
            for (const dimensions of DIMENSIONS)
                for (const stimulus of Object.values(STIMULI)) {
                    const input = audio(stimulus);
                    const left = before.rgbMap(...dimensions, before.color, 0, input);
                    const right = after.rgbMap(...dimensions, after.color, 0, input);
                    assert.deepStrictEqual(Array.from(left), Array.from(right),
                        `${file}: artistic output differs from scoped entry`);
                }
        }
        console.log('ARTISTIC_ENTRY: 5 exact preserved scripts across dimensions and unequal stimuli');
    }
    markCase('reference-core-vectors');
}

function assertMeltSpeedZeroStability() {
    const algo = loadScript('audiomelt.js', 0x51eed);
    algo.setSpeed(0);
    algo.setReactivity(0.7);
    const dimensions = [[1, 1], [7, 2], [9, 1]];
    for (const [width, height] of dimensions) {
        const initial = render(algo, 'audiomelt.js', width, height, audio({
            version: 6,
            low: 0.7,
            dt: 0.04,
            timing: { deltaSeconds: 1 / 50 },
            sourceId: 'melt-source',
            profileId: 1,
            sourceEpoch: 1
        }), `${width}x${height} initial`);
        const repeated = render(algo, 'audiomelt.js', width, height, audio({
            version: 6,
            low: 0.7,
            dt: 0,
            timing: { deltaSeconds: 0 },
            sourceId: 'melt-source',
            profileId: 1,
            sourceEpoch: 1
        }), `${width}x${height} repeated`);
        assert.deepStrictEqual(
            repeated,
            initial,
            `audiomelt.js ${width}x${height}: zero-speed duplicate read changed output`
        );
    }
    const positive = loadScript('audiomelt.js', 0x51eed);
    positive.setSpeed(0.5);
    positive.setReactivity(0.4);
    const map = render(positive, 'audiomelt.js', 7, 1, audio({
        version: 6,
        low: 0.6,
        dt: 0.04,
        timing: { deltaSeconds: 1 / 50 }
    }), 'positive speed finite');
    assert(map.every(Number.isFinite), 'audiomelt.js: positive speed produced non-finite values');
    markCase('melt-speed-zero');
}

function assertMeltSparkleBehavior() {
    const peakOf = map => {
        let peak = 0;
        for (let i = 2; i < map.length; i += 3)
            peak = Math.max(peak, map[i]);
        return peak;
    };
    const peakSaturation = map => {
        let peak = -1;
        let saturation = 1;
        for (let i = 0; i < map.length / 3; i++) {
            const value = map[i * 3 + 2];
            if (value > peak) {
                peak = value;
                saturation = map[i * 3 + 1];
            }
        }
        return saturation;
    };
    const decayAfter = hz => {
        const algo = loadScript('audiomeltsparkle.js', 0x5bade);
        algo.setBgBright(0);
        algo.setStrobeThreshold(0.5);
        algo.setStrobeDecay(0.25);
        algo.setStrobeBlur(4);
        let map = render(algo, 'audiomeltsparkle.js', 37, 1, audio({
            version: 6,
            low: 0.3,
            mid: 0.3,
            high: 0.9,
            onset: true,
            dt: 2 / hz,
            timing: { deltaSeconds: 1 / hz },
            events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } },
            sourceId: 'sparkle-source',
            profileId: 4,
            sourceEpoch: 7,
            configRevision: 1
        }), `${hz}Hz event`);
        const initial = map.slice();
        const repeated = render(algo, 'audiomeltsparkle.js', 37, 1, audio({
            version: 6,
            low: 0.3,
            mid: 0.3,
            high: 0.9,
            onset: false,
            dt: 0,
            timing: { deltaSeconds: 0 },
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
            sourceId: 'sparkle-source',
            profileId: 4,
            sourceEpoch: 7,
            configRevision: 1
        }), `${hz}Hz zero-delta repeat`);
        assert.deepStrictEqual(repeated, map, `audiomeltsparkle.js ${hz}Hz: zero-delta repeat changed output`);
        const steps = Math.round(0.2 * hz);
        for (let n = 0; n < steps; n++) {
            map = render(algo, 'audiomeltsparkle.js', 37, 1, audio({
                version: 6,
                low: 0.3,
                mid: 0.3,
                high: 0.1,
                onset: false,
                dt: 2 / hz,
                timing: { deltaSeconds: 1 / hz },
                events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
                sourceId: 'sparkle-source',
                profileId: 4,
                sourceEpoch: 7,
                configRevision: 1
            }), `${hz}Hz decay ${n}`);
        }
        return { peak: peakOf(map), sat: peakSaturation(map), map, initial };
    };
    const peaks = [25, 30, 50, 60].map(hz => ({ hz, value: decayAfter(hz).peak }));
    const max = Math.max(...peaks.map(p => p.value));
    const min = Math.min(...peaks.map(p => p.value));
    assert(max - min < 0.02, `audiomeltsparkle.js: 200ms decay differs across cadence ${JSON.stringify(peaks)}`);

    const visual = decayAfter(50);
    assert(Math.abs(peakOf(visual.initial) - 1) < 1e-6, 'audiomeltsparkle.js: sparkle peak is not white');
    assert(peakSaturation(visual.initial) <= 1e-6,
        `audiomeltsparkle.js: sparkle peak saturation expected 0, got ${peakSaturation(visual.initial)}`);
    assert(visual.initial.some((value, index) => index % 3 === 2 && value < 1e-6),
        'audiomeltsparkle.js: expected dark exterior around sparkle');

    const reset = loadScript('audiomeltsparkle.js', 0x5bade);
    reset.setBgBright(0);
    const oldEpoch = render(reset, 'audiomeltsparkle.js', 11, 7, audio({
        version: 6,
        low: 0.2,
        mid: 0.2,
        high: 0.9,
        onset: true,
        dt: 0.04,
        timing: { deltaSeconds: 1 / 50 },
        events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } },
        sourceId: 'sparkle-source',
        profileId: 2,
        sourceEpoch: 1,
        configRevision: 1
    }), 'old epoch event');
    const changed = render(reset, 'audiomeltsparkle.js', 11, 7, audio({
        version: 6,
        low: 0.2,
        mid: 0.2,
        high: 0.1,
        onset: false,
        dt: 0,
        timing: { deltaSeconds: 0 },
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
        sourceId: 'sparkle-source',
        profileId: 2,
        sourceEpoch: 2,
        configRevision: 1
    }), 'epoch reset');
    const fresh = loadScript('audiomeltsparkle.js', 0x5bade);
    fresh.setBgBright(0);
    const freshEpoch = render(fresh, 'audiomeltsparkle.js', 11, 7, audio({
        version: 6,
        low: 0.2,
        mid: 0.2,
        high: 0.1,
        onset: false,
        dt: 0,
        timing: { deltaSeconds: 0 },
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
        sourceId: 'sparkle-source',
        profileId: 2,
        sourceEpoch: 2,
        configRevision: 1
    }), 'fresh epoch');
    assert.notDeepStrictEqual(oldEpoch, changed, 'audiomeltsparkle.js: epoch update kept stale overlay');
    assert.deepStrictEqual(changed, freshEpoch, 'audiomeltsparkle.js: epoch reset diverges from fresh state');
    markCase('melt-sparkle-aging');
}

function assertPitchSpectrumUsesPitch() {
    const frame = pitchHz => audio({
        version: 6,
        dt: 0.04,
        timing: { deltaSeconds: 1 / 50 },
        sourceId: 'pitch-source',
        profileId: 5,
        sourceEpoch: 1,
        configRevision: 9,
        pitch: {
            valid: pitchHz > 0,
            hz: pitchHz,
            midi: pitchHz > 0 ? 69 + 12 * Math.log2(pitchHz / 440) : 0,
            confidence: pitchHz > 0 ? 0.8 : 0
        },
        banks: {
            full: {
                count: 8,
                processed: [0.8, 0.6, 0.4, 0.2, 0.5, 0.7, 0.3, 0.1],
                novelty: [0.4, 0.3, 0.2, 0.1, 0.25, 0.35, 0.15, 0.05]
            }
        }
    });
    const lowPitch = loadScript('audiopitchspectrum.js', 0xface);
    lowPitch.setFade(0);
    lowPitch.setResponsiveness(1);
    const low = render(lowPitch, 'audiopitchspectrum.js', 9, 1, frame(220), 'pitch low');
    const high = render(lowPitch, 'audiopitchspectrum.js', 9, 1, frame(880), 'pitch high');
    assert.notDeepStrictEqual(low, high, 'audiopitchspectrum.js: distinct valid pitches produced identical output');
    const hold = render(lowPitch, 'audiopitchspectrum.js', 9, 1, frame(0), 'pitch invalid');
    assert(hold.some((value, index) => index % 3 === 2 && value > 0),
        'audiopitchspectrum.js: invalid pitch unexpectedly cleared output');
    const decoyA = loadScript('audiopitchspectrum.js', 0xface);
    const decoyB = loadScript('audiopitchspectrum.js', 0xface);
    decoyA.setFade(0);
    decoyA.setResponsiveness(1);
    decoyB.setFade(0);
    decoyB.setResponsiveness(1);
    const baseline = render(decoyA, 'audiopitchspectrum.js', 9, 1, Object.assign(frame(440), {
        low: 1, mid: 0, high: 1, beat: 1, bass: 1, onset: true
    }), 'pitch decoy baseline');
    const decoyChanged = render(decoyB, 'audiopitchspectrum.js', 9, 1, Object.assign(frame(440), {
        low: 0, mid: 1, high: 0, beat: 0, bass: 0, onset: false
    }), 'pitch decoy changed');
    assert.deepStrictEqual(baseline, decoyChanged,
        'audiopitchspectrum.js: decoy flat scalars changed pitch output');
    markCase('pitch-spectrum-selection');
}

function assertWaterfallHistory() {
    const algo = loadScript('audiowaterfall.js', 0x51ea);
    algo.setBands(4);
    algo.setAggregation('Max');
    algo.setCenterMode('Off');
    algo.setDropSeconds(3);
    algo.setFade('Off');
    const frame = (values, seconds) => audio({
        version: 6,
        dt: seconds * 2,
        timing: { deltaSeconds: seconds },
        sourceId: 'waterfall-source',
        profileId: 3,
        sourceEpoch: 1,
        configRevision: 2,
        banks: { full: { count: values.length, novelty: values, processed: values } }
    });
    const first = render(algo, 'audiowaterfall.js', 12, 5, frame([1, 0, 0, 0, 0, 0, 0, 0], 0), 'waterfall first');
    const duplicate = render(algo, 'audiowaterfall.js', 12, 5, frame([1, 0, 0, 0, 0, 0, 0, 0], 0), 'waterfall duplicate');
    const second = render(algo, 'audiowaterfall.js', 12, 5, frame([0, 0, 0, 0, 0, 0, 0, 1], 0.7), 'waterfall second');
    const rowPeak = (map, row) => {
        let peak = 0;
        for (let x = 0; x < 12; x++)
            peak = Math.max(peak, map[(row * 12 + x) * 3 + 2]);
        return peak;
    };
    assert(rowPeak(first, 0) > 0.9, 'audiowaterfall.js: first frame has no foreground energy');
    assert.deepStrictEqual(duplicate, first, 'audiowaterfall.js: zero elapsed frame advanced history');
    assert(rowPeak(second, 0) > 0.9, 'audiowaterfall.js: second frame top row has no new foreground energy');
    assert(rowPeak(second, 1) > 0.9, 'audiowaterfall.js: history row did not persist');
    assert.notDeepStrictEqual(first.slice(0, 12 * 3), second.slice(0, 12 * 3),
        'audiowaterfall.js: distinct frequency peaks produced identical front row');
    const decoyA = loadScript('audiowaterfall.js', 0x51ea);
    const decoyB = loadScript('audiowaterfall.js', 0x51ea);
    [decoyA, decoyB].forEach(instance => {
        instance.setBands(4);
        instance.setAggregation('Max');
        instance.setCenterMode('Off');
        instance.setDropSeconds(3);
        instance.setFade('Off');
    });
    const processedOnlyA = render(decoyA, 'audiowaterfall.js', 12, 5, audio({
        version: 6,
        dt: 0,
        timing: { deltaSeconds: 0 },
        sourceId: 'waterfall-source',
        profileId: 3,
        sourceEpoch: 2,
        configRevision: 2,
        banks: { full: { count: 8, novelty: [0.6, 0.2, 0.1, 0, 0, 0, 0, 0], processed: [1, 1, 1, 1, 1, 1, 1, 1] } }
    }), 'waterfall decoy processed baseline');
    const processedOnlyB = render(decoyB, 'audiowaterfall.js', 12, 5, audio({
        version: 6,
        dt: 0,
        timing: { deltaSeconds: 0 },
        sourceId: 'waterfall-source',
        profileId: 3,
        sourceEpoch: 2,
        configRevision: 2,
        banks: { full: { count: 8, novelty: [0.6, 0.2, 0.1, 0, 0, 0, 0, 0], processed: [0, 0, 0, 0, 0, 0, 0, 0] } }
    }), 'waterfall decoy processed changed');
    assert.deepStrictEqual(processedOnlyA, processedOnlyB,
        'audiowaterfall.js: processed decoy changed novelty-based output');
    markCase('waterfall-selection');
}

function assertDigitalRainMotion() {
    const algo = loadScript('audiodigitalrain.js', 0xbead);
    algo.setBirthsPerSecond(20);
    algo.setMaxStreams(8);
    algo.setTail(0.5);
    algo.setSpeed(8);
    const frame = seconds => audio({
        version: 6,
        dt: seconds * 2,
        timing: { deltaSeconds: seconds },
        sourceId: 'rain-source',
        profileId: 6,
        sourceEpoch: 1,
        configRevision: 1,
        low: 0.2,
        mid: 0.2,
        high: 0.8,
        powers: { raw: { low: 0.1, mid: 0.2, high: 0.9 } },
        tempo: { beatPhase: 0.35 }
    });
    const first = render(algo, 'audiodigitalrain.js', 12, 7, frame(0.1), 'digital rain first');
    const duplicate = render(algo, 'audiodigitalrain.js', 12, 7, frame(0), 'digital rain duplicate');
    const next = render(algo, 'audiodigitalrain.js', 12, 7, frame(0.2), 'digital rain motion');
    assert.deepStrictEqual(duplicate, first, 'audiodigitalrain.js: zero elapsed frame advanced state');
    assert.notDeepStrictEqual(next, first, 'audiodigitalrain.js: positive elapsed frame failed to move streams');
    assert(next.some((value, index) => index % 3 === 2 && value > 0),
        'audiodigitalrain.js: produced no visible output');
    markCase('digital-rain-motion');
}

function assertRainPulseSelection() {
    const peakOf = map => {
        let peak = 0;
        for (let i = 2; i < map.length; i += 3)
            peak = Math.max(peak, map[i]);
        return peak;
    };
    const frame = (processed, seconds) => audio({
        version: 6,
        dt: seconds * 2,
        timing: { deltaSeconds: seconds },
        sourceId: 'rain-pulse',
        profileId: 12,
        sourceEpoch: 3,
        configRevision: 1,
        banks: { full: { count: processed.length, processed, novelty: processed } }
    });

    const mids = loadScript('audiopuddles.js', 0x1144);
    mids.setMode('Rain Pulse');
    mids.setRainBand('Mids');
    mids.setRainSensitivity('Low');
    render(mids, 'audiopuddles.js', 12, 1, frame([0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 0.02), 'rain baseline');
    const midHit = render(mids, 'audiopuddles.js', 12, 1, frame([0, 0, 0.9, 0.8, 0.7, 0, 0, 0, 0, 0], 0.02), 'rain mids trigger');
    assert(peakOf(midHit) > 0.6, 'audiopuddles.js rain pulse: selected mid transient did not fill strip');
    const decayed = render(mids, 'audiopuddles.js', 12, 1, frame([0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 0.02), 'rain decay');
    assert(peakOf(decayed) < peakOf(midHit), 'audiopuddles.js rain pulse: strip did not decay');
    const repeated = render(mids, 'audiopuddles.js', 12, 1, frame([0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 0), 'rain zero-delta repeat');
    assert.deepStrictEqual(repeated, decayed, 'audiopuddles.js rain pulse: zero elapsed changed output');

    const highOnly = loadScript('audiopuddles.js', 0x1144);
    highOnly.setMode('Rain Pulse');
    highOnly.setRainBand('Mids');
    highOnly.setRainSensitivity('Low');
    render(highOnly, 'audiopuddles.js', 12, 1, frame([0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 0.02), 'rain high baseline');
    const highHit = render(highOnly, 'audiopuddles.js', 12, 1, frame([0, 0, 0, 0, 0, 0, 0, 0.8, 0.9, 1], 0.02), 'rain high decoy');
    assert(peakOf(highHit) < 1e-6, 'audiopuddles.js rain pulse: unselected high transient leaked into mids mode');
    markCase('rain-pulse-selection');
}

function assertScrollPlusFractionalShift() {
    const peakOf = map => {
        let peak = 0;
        for (let i = 2; i < map.length; i += 3)
            peak = Math.max(peak, map[i]);
        return peak;
    };
    const frame = (processed, seconds) => audio({
        version: 6,
        dt: seconds * 2,
        timing: { deltaSeconds: seconds },
        sourceId: 'scroll-plus',
        profileId: 13,
        sourceEpoch: 1,
        configRevision: 1,
        banks: { full: { count: processed.length, processed, novelty: processed } }
    });

    const algo = loadScript('audiobarcode.js', 0x3344);
    algo.setMode('Scroll+');
    algo.setReferencePalette('#ff0000,#00ff00,#0000ff');
    algo.setReferenceBlur(0);
    algo.setReferenceMirror('Off');
    algo.setReferenceBrightness(1);
    algo.setScrollPlusSpeed(0.5);
    algo.setScrollPlusDecay(0.5);
    algo.setScrollPlusThreshold(0.1);

    const first = render(algo, 'audiobarcode.js', 10, 1, frame([0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0], 0.1), 'scroll+ half pixel');
    assert(peakOf(first) < 1e-6, 'audiobarcode.js scroll+: first half-step should not inject pixel');
    const second = render(algo, 'audiobarcode.js', 10, 1, frame([0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0], 0.1), 'scroll+ full pixel');
    assert(peakOf(second) > 0.4, 'audiobarcode.js scroll+: second half-step should inject one pixel');
    const third = render(algo, 'audiobarcode.js', 10, 1, frame(new Array(10).fill(0), 0.05), 'scroll+ half decay 1');
    const fourth = render(algo, 'audiobarcode.js', 10, 1, frame(new Array(10).fill(0), 0.05), 'scroll+ half decay 2');
    const ratio = fourth[2] / second[2];
    assert(Math.abs(ratio - 0.855625) < 1e-6,
        `audiobarcode.js scroll+: expected two half-step linear decays, got ${ratio}`);
    const repeated = render(algo, 'audiobarcode.js', 10, 1, frame(new Array(10).fill(0), 0), 'scroll+ zero repeat');
    assert.deepStrictEqual(repeated, fourth, 'audiobarcode.js scroll+: zero elapsed changed state');
    assert(peakOf(third) < peakOf(second), 'audiobarcode.js scroll+: first decay did not reduce brightness');
    markCase('scroll-plus-fractional');
}

function assertSharedHueHelperBoundaries() {
    const helpers = vm.runInNewContext(HSV_UTIL + '\nHSVUtil;', { Float32Array });
    const input = {
        beat: 0.1, bass: 0.2, low: 0.15, mid: 0.3, high: 0.4,
        powers: { raw: { beat: 0.9, bass: 0.5, low: 0.7, mid: 0.8, high: 0.6 } }
    };
    const selectors = [
        ['Beat', 0.1, 0.9], ['Bass', 0.2, 0.5], ['Lows', 0.15, 0.7],
        ['Mids', 0.3, 0.8], ['High', 0.4, 0.6], ['Beat+Bass', 0.15, 0.7]
    ];
    for (const [selector, filtered, raw] of selectors) {
        assert(Math.abs(helpers.powerForRange(input, selector, false) - filtered) < 1e-12,
            `${selector}: filtered selector read a different source`);
        assert(Math.abs(helpers.powerForRange(input, selector, true) - raw) < 1e-12,
            `${selector}: raw selector read a filtered source`);
    }
    const overdrive = new Float32Array([0, 1, 1.96]);
    helpers.applyStripTransforms(overdrive, 1, 1, { brightness: 0.5 });
    assert(Math.abs(overdrive[2] - 0.98) < 1e-6,
        'X brightness must apply before final output clipping');
    const intermediate = helpers.rgbToHsvUnclipped(2, 1, 0);
    const overlap = new Float32Array([intermediate.h, intermediate.s, intermediate.v]);
    helpers.applyStripTransforms(overlap, 1, 1, { brightness: 0.5 });
    const channels = helpers.hsvToRgb(overlap[0], overlap[1], overlap[2]);
    for (const [index, expected] of [1, 0.5, 0].entries())
        assert(Math.abs(channels[index] - expected) < 1e-6,
            `RGB overlap channel ${index} clipped before brightness`);
    const stops = [{ h: 0, s: 1, v: 1 }, { h: 0.5, s: 1, v: 1 }];
    for (const channel of helpers.gradientRgbAt(stops, 0.5, ''))
        assert(Math.abs(channel - 0.5) < 1e-12, 'G midpoint must interpolate RGB, not HSV');
    const identity = new Float32Array([0.42, 0, 1, 0.7, 0.25, 0.3]);
    const before = Array.from(identity);
    helpers.applyStripTransforms(identity, 2, 1, {});
    assert.deepStrictEqual(Array.from(identity), before, 'identity X changed valid HSV bytes');
    const controls = { properties: [] };
    const readOutput = helpers.attachStripTransformControls(controls);
    assert.strictEqual(controls.properties.length, 7);
    assert.strictEqual(new Set(controls.properties.map(parseProperty).map(p => p.name)).size, 7);
    controls.setBrightness(0.5);
    controls.setMirror('On');
    controls.setBackgroundMode('Additive');
    controls.setBackgroundColor('#204060');
    assert.strictEqual(readOutput().brightness, 0.5);
    assert.strictEqual(readOutput().mirror, 'On');
    assert.strictEqual(readOutput().backgroundColor, '#204060');
    assert.throws(() => controls.setBrightness('not-a-number'), /brightness/i);
    assert.throws(() => controls.setMirror('sideways'), /mirror/i);
    helpers.attachStripTransformControls(controls);
    assert.strictEqual(controls.properties.length, 7, 'reattachment duplicated descriptors');
    assert.strictEqual(readOutput().brightness, 0.5, 'reattachment reset an existing control');
    const reference = { properties: [] };
    const readReference = helpers.attachStripTransformControls(reference, {
        prefix: 'reference', display: 'Reference ', defaults: { brightness: 0.75 }
    });
    assert(reference.properties.some(p => p.startsWith('name:referenceBrightness|')));
    reference.setReferenceBrightness(0.25);
    assert.strictEqual(readReference().brightness, 0.25);
    const source = {
        available: true, sourceId: 'mic-a', profileId: 7, sourceEpoch: 3, configRevision: 2
    };
    const sourceKey = helpers.audioIdentityKey(source);
    assert.strictEqual(helpers.audioIdentityKey({
        available: false, sourceId: '', profileId: 7, sourceEpoch: 0,
        configRevision: 0, status: 'reset'
    }, sourceKey), sourceKey, 'transient absence reset the source identity');
    assert.notStrictEqual(helpers.audioIdentityKey(Object.assign({}, source, {
        sourceEpoch: 4
    }), sourceKey), sourceKey, 'a new source epoch reused old identity');
    assert.notStrictEqual(helpers.audioIdentityKey({
        available: false, sourceId: '', profileId: 8, status: 'reset'
    }, sourceKey), sourceKey, 'a different profile retained the previous identity');
    markCase('shared-helper-boundaries');
}

function assertNonAudioScripts() {
    const makeClock = (nowMs = 1000) => ({ nowMs, now() { return this.nowMs; } });
    const decoyAudio = Object.freeze({
        timing: { deltaSeconds: 99, absoluteSeconds: 777 },
        events: { delta: { onset: 9, beat: 8, kick: 7, bar: 6 } },
        powers: { raw: { low: 0.99, mid: 0.01 } },
        banks: { full: { count: 3, processed: [0.9, 0.5, 0.1], novelty: [0.4, 0.3, 0.2] } }
    });
    const countLit = map => map.filter((value, index) => index % 3 === 2 && value > 0).length;
    const litIndices = map => {
        const out = [];
        for (let i = 0; i < map.length / 3; i++)
            if (map[i * 3 + 2] > 0) out.push(i);
        return out;
    };
    const assertIgnoresAudioPayload = (file, seed, configure, width, height) => {
        const clockA = makeClock(4000);
        const clockB = makeClock(4000);
        const a = loadScript(file, seed, SCRIPTS_DIR, { expectedUsesAudio: false, clock: clockA });
        const b = loadScript(file, seed, SCRIPTS_DIR, { expectedUsesAudio: false, clock: clockB });
        configure(a);
        configure(b);
        const first = render(a, file, width, height, {}, `${file} no audio`);
        const second = render(b, file, width, height, decoyAudio, `${file} decoy audio`);
        assert.deepStrictEqual(first, second, `${file}: decoy audio changed nonaudio output`);
        clockA.nowMs += 250;
        clockB.nowMs += 250;
        const nextA = render(a, file, width, height, {}, `${file} no audio advanced`);
        const nextB = render(b, file, width, height, decoyAudio, `${file} decoy audio advanced`);
        assert.deepStrictEqual(nextA, nextB, `${file}: decoy audio changed nonaudio animation`);
    };

    const fadeClock = makeClock(1000);
    const fade = loadScript('huefade.js', 0x1111, SCRIPTS_DIR, {
        expectedUsesAudio: false,
        clock: fadeClock
    });
    fade.colors = [{ h: 0, s: 0, v: 1 }, { h: 0.5, s: 1, v: 1 }];
    const wallStart = render(fade, 'huefade.js', 5, 1, {}, 'fade wall start');
    const wallRepeat = render(fade, 'huefade.js', 5, 1, {}, 'fade wall repeat');
    fadeClock.nowMs += 1000;
    const wallAdvanced = render(fade, 'huefade.js', 5, 1, {}, 'fade wall advanced');
    assert.deepStrictEqual(wallStart, wallRepeat, 'huefade.js: fixed wall clock changed output');
    assert.notDeepStrictEqual(wallStart, wallAdvanced, 'huefade.js: advanced wall clock did not animate');
    fade.colors = [{ h: 0, s: 1, v: 1 }, { h: 0.5, s: 1, v: 1 }];
    fade.setSpeed(0.1);
    fade.position = 0.5;
    const fadeMid = render(fade, 'huefade.js', 1, 1, {}, 'fade RGB midpoint');
    assert(fadeMid[1] < 0.05 && fadeMid[2] > 0.49 && fadeMid[2] < 0.51,
        'huefade.js: midpoint must use RGB blend, not HSV hue arc');
    fade.colors = [{ h: 0, s: 0, v: 0.2 }];
    fade.setBackgroundMode('Additive');
    fade.setBackgroundColor('#400000');
    fade.setBackgroundBrightness(1);
    fade.setBrightness(1);
    const withBackground = render(fade, 'huefade.js', 1, 1, {}, 'fade additive background');
    assert(withBackground[2] > 0.2, 'huefade.js: additive background missing');
    fade.setBackgroundMode('Off');
    fade.colors = [{ h: 0, s: 1, v: 1 }, { h: 0.3, s: 1, v: 1 }];
    fade.position = 0.1;
    const fadeBase = render(fade, 'huefade.js', 5, 1, {}, 'fade base transform');
    fade.setFlip('On');
    const fadeFlipped = render(fade, 'huefade.js', 5, 1, {}, 'fade flipped');
    assert(Math.abs(fadeFlipped[0] - fadeBase[(5 - 1) * 3]) < 1e-6, 'huefade.js: flip missing');
    fade.setFlip('Off');
    fade.setBlur(2.5);
    const fadeBlurred = render(fade, 'huefade.js', 5, 1, {}, 'fade blurred');
    assert.notDeepStrictEqual(fadeBlurred, fadeBase, 'huefade.js: blur missing');
    fade.setBlur(0);
    fade.setBrightness(0.4);
    const dimmed = render(fade, 'huefade.js', 5, 1, {}, 'fade brightness');
    assert(Math.max(...dimmed.filter((_, i) => i % 3 === 2)) < Math.max(...fadeBase.filter((_, i) => i % 3 === 2)),
        'huefade.js: brightness scaling missing');
    fade.setBrightness(1);
    fade.setPositions('0,0.2,1');
    fade.colors = [{ h: 0, s: 1, v: 1 }, { h: 1 / 3, s: 1, v: 1 }, { h: 2 / 3, s: 1, v: 1 }];
    fade.position = 0.5;
    const positioned = render(fade, 'huefade.js', 1, 1, {}, 'fade positioned stops');
    fade.setPositions('');
    const evenStops = render(fade, 'huefade.js', 1, 1, {}, 'fade even stops');
    assert(Math.abs(positioned[0] - evenStops[0]) > 0.05, 'huefade.js: positions did not alter stop interpolation');

    const rainbowClock = makeClock(2000);
    const rainbow = loadScript('huerainbow.js', 0x2222, SCRIPTS_DIR, {
        expectedUsesAudio: false,
        clock: rainbowClock
    });
    rainbow.setFrequency(2);
    rainbow.setSpeed(1);
    const rainbowStart = render(rainbow, 'huerainbow.js', 8, 1, {}, 'rainbow start');
    const hueAt = (map, i) => map[i * 3];
    assert(Math.abs(hueAt(rainbowStart, 1) - hueAt(rainbowStart, 0) - 0.25) < 1e-6,
        'huerainbow.js: frequency spacing mismatch');
    rainbowClock.nowMs += 100;
    const rainbowMoved = render(rainbow, 'huerainbow.js', 8, 1, {}, 'rainbow move');
    assert(Math.abs(hueAt(rainbowMoved, 0) - hueAt(rainbowStart, 0) - 0.01) < 1e-6,
        'huerainbow.js: speed to hue phase mismatch');
    rainbow.setMirror('On');
    const mirrored = render(rainbow, 'huerainbow.js', 8, 1, {}, 'rainbow mirror');
    assert(Math.abs(mirrored[0] - mirrored[(8 - 1) * 3]) < 1e-6,
        'huerainbow.js: mirror did not fold strip symmetrically');

    const singleClock = makeClock(3000);
    const single = loadScript('huesinglecolor.js', 0x3333, SCRIPTS_DIR, {
        expectedUsesAudio: false,
        clock: singleClock
    });
    single.colors = [{ h: 0, s: 0, v: 1 }];
    single.setModulation('Sine');
    const sine = render(single, 'huesinglecolor.js', 5, 1, {}, 'single sine');
    const expected = [0.4, 0.6121320343559642, 0.7, 0.6121320343559642, 0.4];
    for (let i = 0; i < 5; i++)
        assert(Math.abs(sine[i * 3 + 2] - expected[i]) < 1e-6, `huesinglecolor.js: sine mismatch ${i}`);
    single.setModulation('Breath');
    const breath = render(single, 'huesinglecolor.js', 5, 1, {}, 'single breath start');
    assert.strictEqual(countLit(breath), 1, 'huesinglecolor.js: breath should light floor(0.2*n) at start');

    const randomClock = makeClock(5000);
    const randomFlash = loadScript('huerandomflash.js', 0x4444, SCRIPTS_DIR, {
        expectedUsesAudio: false,
        clock: randomClock
    });
    randomFlash.colors = [{ h: 0.2, s: 1, v: 1 }];
    randomFlash.setProbability(1);
    randomFlash.setDuration(1);
    randomFlash.setSize(25);
    render(randomFlash, 'huerandomflash.js', 20, 1, {}, 'flash init');
    randomClock.nowMs += 1000;
    const flashStart = render(randomFlash, 'huerandomflash.js', 20, 1, {}, 'flash start');
    assert.strictEqual(countLit(flashStart), 5, 'huerandomflash.js: wrong flash size');
    randomClock.nowMs += 500;
    const flashHalf = render(randomFlash, 'huerandomflash.js', 20, 1, {}, 'flash half');
    assert(Math.abs(Math.max(...flashHalf.filter((_, i) => i % 3 === 2)) - 0.5) < 1e-6,
        'huerandomflash.js: flash half-life mismatch');
    randomFlash.active = { start: 12, size: 5, remaining: 2 };
    const resizedZeroDt = render(randomFlash, 'huerandomflash.js', 8, 1, {}, 'flash resize zero dt');
    assert.strictEqual(countLit(resizedZeroDt), 0, 'huerandomflash.js: geometry resize did not clear stale active span');
    randomClock.nowMs += 1000;
    const resizedSpawn = render(randomFlash, 'huerandomflash.js', 8, 1, {}, 'flash resize spawn');
    const resizedLit = litIndices(resizedSpawn);
    assert.strictEqual(resizedLit.length, 2, 'huerandomflash.js: resized spawn size mismatch');
    assert(resizedLit[0] >= 0 && resizedLit[1] < 8, 'huerandomflash.js: resized spawn escaped new geometry');
    randomFlash.setProbability(0);
    randomClock.nowMs += 1200;
    const flashDone = render(randomFlash, 'huerandomflash.js', 8, 1, {}, 'flash done');
    assert.strictEqual(countLit(flashDone), 0, 'huerandomflash.js: flash did not expire');

    const pixelsClock = makeClock(7000);
    const pixels = loadScript('huepixels.js', 0x5555, SCRIPTS_DIR, {
        expectedUsesAudio: false,
        clock: pixelsClock
    });
    pixels.colors = [{ h: 0, s: 1, v: 1 }, { h: 0.6, s: 1, v: 1 }];
    pixels.setPeriod(0.1);
    pixels.setChunk(3);
    pixels.setBuildUp('On');
    render(pixels, 'huepixels.js', 7, 1, {}, 'pixels init');
    pixelsClock.nowMs += 100;
    const p1 = render(pixels, 'huepixels.js', 7, 1, {}, 'pixels step 1');
    pixelsClock.nowMs += 100;
    const p2 = render(pixels, 'huepixels.js', 7, 1, {}, 'pixels step 2');
    pixelsClock.nowMs += 100;
    const p3 = render(pixels, 'huepixels.js', 7, 1, {}, 'pixels step 3');
    pixelsClock.nowMs += 100;
    const p4 = render(pixels, 'huepixels.js', 7, 1, {}, 'pixels wrap');
    assert.strictEqual(countLit(p1), 3, 'huepixels.js: first chunk size mismatch');
    assert.strictEqual(countLit(p2), 6, 'huepixels.js: buildup did not retain prior chunk');
    assert.strictEqual(countLit(p3), 7, 'huepixels.js: terminal chunk mismatch');
    assert.strictEqual(countLit(p4), 3, 'huepixels.js: wrap should restart build-up');
    assert(Math.abs(p4[0] - 0.6) < 1e-6, 'huepixels.js: wrap should advance color');
    const quickClock = makeClock(8000);
    const quickPixels = loadScript('huepixels.js', 0x55aa, SCRIPTS_DIR, {
        expectedUsesAudio: false,
        clock: quickClock
    });
    quickPixels.colors = [{ h: 0.1, s: 1, v: 1 }, { h: 0.6, s: 1, v: 1 }];
    quickPixels.setPeriod(0.01);
    quickPixels.setChunk(1);
    quickPixels.setBuildUp('Off');
    render(quickPixels, 'huepixels.js', 10, 1, {}, 'pixels quick init');
    quickClock.nowMs += 10;
    render(quickPixels, 'huepixels.js', 10, 1, {}, 'pixels initialize');
    quickClock.nowMs += 60000;
    const stalled = render(quickPixels, 'huepixels.js', 10, 1, {}, 'pixels long stall');
    quickClock.nowMs += 10;
    const afterStall = render(quickPixels, 'huepixels.js', 10, 1, {}, 'pixels after stall');
    const stalledIndex = litIndices(stalled)[0];
    const nextIndex = litIndices(afterStall)[0];
    assert.strictEqual(nextIndex, (stalledIndex + 1) % 10, 'huepixels.js: long stall backlog leaked into future frames');

    const gradientClock = makeClock(9000);
    const gradient = loadScript('huegradient.js', 0x6666, SCRIPTS_DIR, {
        expectedUsesAudio: false,
        clock: gradientClock
    });
    gradient.colors = [{ h: 0, s: 1, v: 1 }, { h: 0.5, s: 1, v: 1 }];
    gradient.setRollSpeed(0);
    gradient.setModulation('Off');
    const g0 = render(gradient, 'huegradient.js', 5, 1, {}, 'gradient base');
    assert(g0[0] < g0[(5 - 1) * 3], 'huegradient.js: base gradient did not increase across strip');
    gradient.setRollSpeed(1);
    gradientClock.nowMs += 1000;
    const g1 = render(gradient, 'huegradient.js', 5, 1, {}, 'gradient roll');
    assert.notDeepStrictEqual(g1, g0, 'huegradient.js: roll did not move gradient');
    gradient.setRollSpeed(0);
    gradient.roll = 0;
    gradient.phase = 0.5;
    gradient.colors = [{ h: 0, s: 1, v: 1 }, { h: 0.5, s: 1, v: 1 }];
    const gradientMid = render(gradient, 'huegradient.js', 3, 1, {}, 'gradient RGB midpoint');
    assert(gradientMid[4] < 0.05 && gradientMid[5] > 0.49 && gradientMid[5] < 0.51,
        'huegradient.js: midpoint must use RGB blend, not HSV hue arc');
    gradient.colors = [{ h: 0, s: 0, v: 1 }];
    gradient.setModulation('Breath');
    gradient.phase = 0;
    const breath0 = render(gradient, 'huegradient.js', 9, 1, {}, 'gradient breath phase0');
    gradient.phase = 1 / 6;
    const breath1 = render(gradient, 'huegradient.js', 9, 1, {}, 'gradient breath phase1');
    gradient.phase = 2 / 3;
    const breath2 = render(gradient, 'huegradient.js', 9, 1, {}, 'gradient breath phase2');
    assert.deepStrictEqual(litIndices(breath0), [0], 'huegradient.js: breath phase0 suffix clear mismatch');
    assert.deepStrictEqual(litIndices(breath1), [0, 1, 2, 3, 4, 5, 6, 7, 8], 'huegradient.js: breath phase1 suffix clear mismatch');
    assert.deepStrictEqual(litIndices(breath2), [0, 1], 'huegradient.js: breath phase2 suffix clear mismatch');
    gradient.setModulation('Off');
    gradient.colors = [{ h: 0, s: 1, v: 1 }, { h: 1 / 3, s: 1, v: 1 }, { h: 2 / 3, s: 1, v: 1 }];
    gradient.setPositions('0,0.2,1');
    const gPos = render(gradient, 'huegradient.js', 5, 1, {}, 'gradient positioned');
    gradient.setPositions('');
    const gEven = render(gradient, 'huegradient.js', 5, 1, {}, 'gradient even');
    assert(Math.abs(gPos[6] - gEven[6]) > 0.05, 'huegradient.js: positions did not alter gradient placement');

    const metroClock = makeClock(0);
    const metro = loadScript('huemetro.js', 0x7777, SCRIPTS_DIR, {
        expectedUsesAudio: false,
        clock: metroClock
    });
    metro.colors = [{ h: 0, s: 0, v: 0 }, { h: 0.33, s: 1, v: 1 }];
    metro.setPeriod(4);
    metro.setDuty(0.6);
    metro.setSteps(4);
    const m0 = render(metro, 'huemetro.js', 16, 1, {}, 'metro stage0');
    metroClock.nowMs = 4100;
    const m1 = render(metro, 'huemetro.js', 16, 1, {}, 'metro stage1');
    metroClock.nowMs = 8100;
    const m2 = render(metro, 'huemetro.js', 16, 1, {}, 'metro stage2');
    metroClock.nowMs = 12100;
    const m3 = render(metro, 'huemetro.js', 16, 1, {}, 'metro stage3');
    metroClock.nowMs = 15000;
    const mOff = render(metro, 'huemetro.js', 16, 1, {}, 'metro off');
    assert.deepStrictEqual(litIndices(m0), [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
        'huemetro.js: stage0 set mismatch');
    assert.deepStrictEqual(litIndices(m1), [0, 1, 2, 3, 4, 5, 6],
        'huemetro.js: stage1 set mismatch');
    assert.deepStrictEqual(litIndices(m2), [0, 1, 2, 8, 9, 10],
        'huemetro.js: stage2 set mismatch');
    assert.deepStrictEqual(litIndices(m3), [0, 4, 8, 12],
        'huemetro.js: stage3 set mismatch');
    assert.strictEqual(countLit(mOff), 0, 'huemetro.js: off duty should show background only');

    assertIgnoresAudioPayload('huefade.js', 0x1111, algo => {
        algo.colors = [{ h: 0, s: 1, v: 1 }, { h: 0.4, s: 1, v: 1 }];
        algo.setSpeed(1);
    }, 5, 1);
    assertIgnoresAudioPayload('huegradient.js', 0x6666, algo => {
        algo.colors = [{ h: 0, s: 1, v: 1 }, { h: 0.4, s: 1, v: 1 }];
        algo.setModulation('Sine');
    }, 7, 1);
    assertIgnoresAudioPayload('huemetro.js', 0x7777, algo => {
        algo.setPeriod(4);
        algo.setDuty(0.6);
        algo.setSteps(4);
    }, 16, 1);
    assertIgnoresAudioPayload('huepixels.js', 0x5555, algo => {
        algo.setPeriod(0.2);
        algo.setChunk(2);
        algo.setBuildUp('On');
    }, 8, 1);
    assertIgnoresAudioPayload('huerainbow.js', 0x2222, algo => {
        algo.setFrequency(3);
        algo.setSpeed(2);
    }, 8, 1);
    assertIgnoresAudioPayload('huerandomflash.js', 0x4444, algo => {
        algo.setProbability(0.4);
        algo.setDuration(0.8);
        algo.setSize(20);
    }, 20, 1);
    assertIgnoresAudioPayload('huesinglecolor.js', 0x3333, algo => {
        algo.setModulation('Breath');
        algo.setModulationSpeed(0.4);
    }, 6, 1);
    markCase('nonaudio-clock-boundary');
}

function assertWorkerASpectralRows() {
    const EPS = 1e-6;

    function approx(actual, expected, label, eps = EPS) {
        assert(Math.abs(actual - expected) <= eps, `${label}: expected ${expected}, got ${actual}`);
    }

    function hsvAt(map, pixelIndex) {
        const o = pixelIndex * 3;
        return { h: map[o], s: map[o + 1], v: map[o + 2] };
    }

    function litIndices(map, start, end, threshold = 1e-6) {
        const out = [];
        for (let i = start; i < end; i++) {
            if (map[i * 3 + 2] > threshold)
                out.push(i);
        }
        return out;
    }

    function splitRanges(length, count) {
        const active = Math.min(Math.max(1, count), Math.max(1, length));
        const base = Math.floor(length / active);
        const extra = length - base * active;
        const ranges = [];
        let start = 0;
        for (let i = 0; i < active; i++) {
            const size = base + (i < extra ? 1 : 0);
            ranges.push({ start, end: start + size, size });
            start += size;
        }
        return ranges;
    }

    function makeBankFrame(novelty, processed) {
        return audio({
            banks: {
                full: {
                    count: novelty.length,
                    novelty: novelty.slice(),
                    processed: (processed || new Array(novelty.length).fill(0)).slice()
                }
            }
        });
    }

    function clamp01(x) {
        return x < 0 ? 0 : (x > 1 ? 1 : x);
    }

    function mod1(x) {
        const m = x - Math.floor(x);
        return m < 0 ? m + 1 : m;
    }

    function hsvToRgb(h, s, v) {
        h = mod1(h);
        s = clamp01(s);
        v = Math.max(0, v);
        const i = Math.floor(h * 6);
        const f = h * 6 - i;
        const p = v * (1 - s);
        const q = v * (1 - f * s);
        const t = v * (1 - (1 - f) * s);
        switch (i % 6) {
        case 0: return [v, t, p];
        case 1: return [q, v, p];
        case 2: return [p, v, t];
        case 3: return [p, q, v];
        case 4: return [t, p, v];
        default: return [v, p, q];
        }
    }

    function rgbToHsvClipped(r, g, b) {
        r = clamp01(r);
        g = clamp01(g);
        b = clamp01(b);
        const max = Math.max(r, g, b);
        const min = Math.min(r, g, b);
        const delta = max - min;
        let h = 0;
        if (delta > 0) {
            if (max === r) h = ((g - b) / delta) / 6;
            else if (max === g) h = (2 + (b - r) / delta) / 6;
            else h = (4 + (r - g) / delta) / 6;
        }
        return { h: mod1(h), s: max > 0 ? delta / max : 0, v: max };
    }

    function gradientLedfxAt(stops, t) {
        if (!stops || stops.length === 0) return { h: 0, s: 0, v: 0 };
        if (stops.length === 1) return { h: stops[0].h, s: stops[0].s, v: stops[0].v };
        const clamped = clamp01(t);
        const pos = clamped * (stops.length - 1);
        const idx = Math.min(stops.length - 2, Math.floor(pos));
        const frac = pos - idx;
        const slope = 1.5;
        const powT = Math.pow(frac, slope);
        const invPowT = Math.pow(1 - frac, slope);
        const eased = powT + invPowT > 0 ? powT / (powT + invPowT) : frac;
        const a = hsvToRgb(stops[idx].h, stops[idx].s, stops[idx].v);
        const b = hsvToRgb(stops[idx + 1].h, stops[idx + 1].s, stops[idx + 1].v);
        return rgbToHsvClipped(
            a[0] + (b[0] - a[0]) * eased,
            a[1] + (b[1] - a[1]) * eased,
            a[2] + (b[2] - a[2]) * eased
        );
    }

    function assertHsvClose(actual, expected, label) {
        approx(actual.h, expected.h, `${label}.h`);
        approx(actual.s, expected.s, `${label}.s`);
        approx(actual.v, expected.v, `${label}.v`);
    }

    {
        const script = 'audiobands.js';
        const novelty = [1, 0, 0, 0, 0.5, 0.25, 0, 0, 0.75, 0.75, 0.75, 0.75];
        const ranges = splitRanges(12, 3);
        const expectedSecondLit = {
            left: [4, 5],
            right: [6, 7],
            center: [5, 6],
            invert: [4, 7]
        };
        const expectedCounts = [4, 2, 3];
        for (const align of Object.keys(expectedSecondLit)) {
            const algo = loadScript(script);
            algo.setBandCount(3);
            algo.setAlign(align);
            const map = render(algo, script, 12, 1, makeBankFrame(novelty), `workerA row01 ${align}`);
            ranges.forEach((range, idx) => {
                assert.strictEqual(
                    litIndices(map, range.start, range.end).length,
                    expectedCounts[idx],
                    `row01/${align}: segment ${idx} lit count`
                );
                const expectedColor = gradientLedfxAt(PALETTE, idx / ranges.length);
                const lit = litIndices(map, range.start, range.end);
                if (lit.length)
                    assertHsvClose(hsvAt(map, lit[0]), expectedColor, `row01/${align}: segment ${idx} color`);
            });
            assert.deepStrictEqual(
                litIndices(map, ranges[1].start, ranges[1].end),
                expectedSecondLit[align],
                `row01/${align}: second segment indices`
            );
        }
        markCase('row01.align-count-gradient');
        console.log('PASS row01.align-count-gradient');
    }

    {
        const script = 'audiobands.js';
        const cases = [
            { width: 7, count: 3, expectedSizes: [3, 2, 2] },
            { width: 5, count: 16, expectedSizes: [1, 1, 1, 1, 1] }
        ];
        for (const testCase of cases) {
            const algo = loadScript(script);
            algo.setBandCount(testCase.count);
            algo.setAlign('left');
            const map = render(
                algo,
                script,
                testCase.width,
                1,
                makeBankFrame(new Array(testCase.width).fill(1)),
                `workerA row01 split ${testCase.width}/${testCase.count}`
            );
            const ranges = splitRanges(testCase.width, testCase.count);
            assert.deepStrictEqual(ranges.map(r => r.size), testCase.expectedSizes);
            ranges.forEach(range => {
                assert.strictEqual(litIndices(map, range.start, range.end).length, range.size);
            });
        }
        markCase('row01.split-cap');
        console.log('PASS row01.split-cap');
    }

    {
        const script = 'audiobandsmatrix.js';
        const novelty = [1, 0, 0, 0, 0.5, 0.25, 0, 0, 0.75, 0.75, 0.75, 0.75];
        const frame = makeBankFrame(novelty);
        const algo = loadScript(script);
        algo.setBandCount(3);
        algo.setMirror('No');
        const base = render(algo, script, 12, 1, frame, 'workerA row02 base');
        const ranges = splitRanges(12, 3);
        assert.deepStrictEqual(litIndices(base, ranges[1].start, ranges[1].end), [6, 7]);
        assertHsvClose(hsvAt(base, 6), gradientLedfxAt(PALETTE, 0.25), 'row02/base first');
        assertHsvClose(hsvAt(base, 7), gradientLedfxAt(PALETTE, 0.0), 'row02/base second');

        algo.setFlipGradient('Yes');
        const flipGradient = render(algo, script, 12, 1, frame, 'workerA row02 flip gradient');
        assertHsvClose(hsvAt(flipGradient, 6), gradientLedfxAt(PALETTE, 0.75), 'row02/flipGradient first');
        assertHsvClose(hsvAt(flipGradient, 7), gradientLedfxAt(PALETTE, 1.0), 'row02/flipGradient second');

        algo.setFlipGradient('No');
        algo.setFlipBandOrder('Yes');
        const flipBandOrder = render(algo, script, 12, 1, frame, 'workerA row02 flip order');
        for (let i = 0; i < 12; i++)
            approx(flipBandOrder[i], base[24 + i], `row02/flipBandOrder channel ${i}`);

        const uneven = loadScript(script);
        uneven.setBandCount(3);
        uneven.setMirror('No');
        const unevenMap = render(
            uneven,
            script,
            8,
            1,
            makeBankFrame([1, 1, 1, 0.8, 0.2, 0, 0.6, 0.6]),
            'workerA row02 uneven'
        );
        const unevenRanges = splitRanges(8, 3);
        assert.deepStrictEqual(litIndices(unevenMap, unevenRanges[1].start, unevenRanges[1].end), [4, 5]);

        const single = loadScript(script);
        single.setBandCount(16);
        single.setMirror('No');
        const singleMap = render(single, script, 1, 1, makeBankFrame([1]), 'workerA row02 N1');
        assert.deepStrictEqual(litIndices(singleMap, 0, 1), [0]);
        markCase('row02.matrix-branches');
        console.log('PASS row02.matrix-branches');
    }

    {
        const script = 'audiospectralblocks.js';
        const algo = loadScript(script);
        algo.setBlockCount(2);
        const novelty = [1, 0.25, 0, 0.5, 0.2, 0.1];
        const map = render(algo, script, 6, 1, makeBankFrame(novelty), 'workerA row08 products');
        const ranges = splitRanges(6, 2);
        const expected = [
            [1, 0.25, 0],
            [0.25, 0.1, 0.05]
        ];
        for (let band = 0; band < ranges.length; band++) {
            const color = gradientLedfxAt(PALETTE, band / ranges.length);
            const rgb = hsvToRgb(color.h, color.s, color.v);
            for (let i = 0; i < ranges[band].size; i++) {
                const f = expected[band][i];
                const pixel = hsvAt(map, ranges[band].start + i);
                const exp = rgbToHsvClipped(rgb[0] * f, rgb[1] * f, rgb[2] * f);
                assertHsvClose(pixel, exp, `row08 products band${band} px${i}`);
            }
        }

        const overdrive = loadScript(script);
        overdrive.colors = [{ h: 1 / 12, s: 1, v: 1 }, { h: 1 / 12, s: 1, v: 1 }];
        overdrive.color = overdrive.colors[0];
        overdrive.setBlockCount(1);
        overdrive.setBrightness(0.5);
        const overdriveMap = render(
            overdrive,
            script,
            2,
            1,
            makeBankFrame([1.4, 0.7]),
            'workerA row08 overdrive hue'
        );
        approx(hsvAt(overdriveMap, 0).h, 1 / 12, 'row08 overdrive hue preserved');
        approx(hsvAt(overdriveMap, 0).v, 0.98, 'row08 first pixel preserves through final brightness');
        approx(hsvAt(overdriveMap, 1).v, 0.49, 'row08 second pixel preserves through final brightness');

        const capped = loadScript(script);
        capped.colors = [{ h: 0, s: 0, v: 1 }];
        capped.color = capped.colors[0];
        capped.setBlockCount(10);
        const cappedMap = render(capped, script, 4, 1, makeBankFrame([1, 1, 1, 1]), 'workerA row08 count cap');
        assert.deepStrictEqual(litIndices(cappedMap, 0, 4), [0, 1, 2, 3]);
        markCase('row08.blocks-products');
        console.log('PASS row08.blocks-products');
    }

    {
        const script = 'audiobladepower.js';
        const persistence = loadScript(script);
        persistence.colors = [{ h: 0.00, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];
        persistence.color = persistence.colors[0];
        persistence.setMirror('No');
        persistence.setBlur(0);
        persistence.setMultiplier(0.5);
        persistence.setDecay(0.7);
        persistence.setFrequencyRange('Lows (beat+bass)');
        const mapA = render(
            persistence,
            script,
            10,
            1,
            audio({ beat: 0.3, bass: 0.3, low: 0.3, timing: { deltaSeconds: 1 / 60 } }),
            'workerA row04 start'
        );
        assert.strictEqual(litIndices(mapA, 0, 10).length, 3);
        const mapB = render(
            persistence,
            script,
            10,
            1,
            audio({ beat: 0.1, bass: 0.1, low: 0.1, timing: { deltaSeconds: 1 / 60 } }),
            'workerA row04 decay'
        );
        approx(hsvAt(mapB, 0).v, 1.0, 'row04 first refresh');
        approx(hsvAt(mapB, 1).v, 0.8, 'row04 second decay');
        approx(hsvAt(mapB, 2).v, 0.8, 'row04 third decay');
        const mapRepeat = render(
            persistence,
            script,
            10,
            1,
            audio({ beat: 0.1, bass: 0.1, low: 0.1, timing: { deltaSeconds: 0 }, dt: 0 }),
            'workerA row04 zero repeat'
        );
        assert.deepStrictEqual(mapRepeat, mapB, 'row04 zero-time repeat');

        const sourceCases = [
            { range: 'Beat', frame: { beat: 0.4, bass: 0.9, low: 0.9, mid: 0.9, high: 0.9 }, expected: 4 },
            { range: 'Bass', frame: { beat: 0.1, bass: 0.7, low: 0.1, mid: 0.1, high: 0.1 }, expected: 7 },
            { range: 'Mids', frame: { beat: 0.1, bass: 0.1, low: 0.1, mid: 0.6, high: 0.1 }, expected: 6 },
            { range: 'High', frame: { beat: 0.1, bass: 0.1, low: 0.1, mid: 0.1, high: 0.2 }, expected: 2 },
            { range: 'Lows (beat+bass)', frame: { beat: 0.2, bass: 0.4, low: 0.9, mid: 0.1, high: 0.1 }, expected: 3 }
        ];
        for (const t of sourceCases) {
            const sourceAlgo = loadScript(script);
            sourceAlgo.colors = [{ h: 0.00, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];
            sourceAlgo.color = sourceAlgo.colors[0];
            sourceAlgo.setMirror('No');
            sourceAlgo.setBlur(0);
            sourceAlgo.setMultiplier(0.5);
            sourceAlgo.setDecay(0.7);
            sourceAlgo.setFrequencyRange(t.range);
            const sourceMap = render(
                sourceAlgo,
                script,
                10,
                1,
                audio(Object.assign({}, t.frame, { timing: { deltaSeconds: 0 } })),
                `workerA row04 source ${t.range}`
            );
            assert.strictEqual(litIndices(sourceMap, 0, 10).length, t.expected);
        }

        const hueAlgo = loadScript(script);
        hueAlgo.colors = [{ h: 0.00, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];
        hueAlgo.color = hueAlgo.colors[0];
        hueAlgo.setMirror('No');
        hueAlgo.setBlur(0);
        hueAlgo.setMultiplier(0.5);
        hueAlgo.setDecay(0.7);
        hueAlgo.setFrequencyRange('Lows (beat+bass)');
        hueAlgo.setFixHues('No');
        const hueNo = render(hueAlgo, script, 10, 1, audio({ beat: 1, bass: 1, low: 1, timing: { deltaSeconds: 0 } }), 'workerA row04 fix no');
        assert.strictEqual(litIndices(hueNo, 0, 10).length, 10);
        approx(hsvAt(hueNo, 5).h, gradientLedfxAt(hueAlgo.colors, 5 / 9).h, 'row04 fix off coord');
        hueAlgo.setFixHues('Yes');
        const hueYes = render(hueAlgo, script, 10, 1, audio({ beat: 1, bass: 1, low: 1, timing: { deltaSeconds: 0 } }), 'workerA row04 fix yes');
        approx(hsvAt(hueYes, 5).h, gradientLedfxAt(hueAlgo.colors, Math.floor(5 * 12 / 9) / 12).h, 'row04 fix on coord');

        const brightnessAlgo = loadScript(script);
        brightnessAlgo.colors = [{ h: 0.20, s: 1, v: 0.5 }, { h: 0.20, s: 1, v: 0.5 }];
        brightnessAlgo.color = brightnessAlgo.colors[0];
        brightnessAlgo.setMirror('No');
        brightnessAlgo.setBlur(0);
        brightnessAlgo.setMultiplier(0.5);
        brightnessAlgo.setDecay(0.7);
        brightnessAlgo.setBrightness(0.5);
        const brightnessMap = render(
            brightnessAlgo,
            script,
            10,
            1,
            audio({ beat: 1, bass: 1, low: 1, timing: { deltaSeconds: 0 } }),
            'workerA row04 brightness'
        );
        approx(hsvAt(brightnessMap, 0).v, 0.25, 'row04 source double-brightness');

        const resetAlgo = loadScript(script);
        resetAlgo.colors = [{ h: 0.00, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];
        resetAlgo.color = resetAlgo.colors[0];
        resetAlgo.setMirror('No');
        resetAlgo.setBlur(0);
        resetAlgo.setMultiplier(0.5);
        resetAlgo.setDecay(0.7);
        resetAlgo.setFrequencyRange('Lows (beat+bass)');
        render(
            resetAlgo,
            script,
            10,
            1,
            audio({ beat: 0.3, bass: 0.3, low: 0.3, sourceId: 'A', sourceEpoch: 1, timing: { deltaSeconds: 1 / 60 } }),
            'workerA row04 reset A0'
        );
        render(
            resetAlgo,
            script,
            10,
            1,
            audio({ beat: 0.1, bass: 0.1, low: 0.1, sourceId: 'A', sourceEpoch: 1, timing: { deltaSeconds: 1 / 60 } }),
            'workerA row04 reset A1'
        );
        const resetMap = render(
            resetAlgo,
            script,
            10,
            1,
            audio({ beat: 0.1, bass: 0.1, low: 0.1, sourceId: 'B', sourceEpoch: 1, timing: { deltaSeconds: 1 / 60 } }),
            'workerA row04 reset B'
        );
        approx(hsvAt(resetMap, 1).v, 0, 'row04 reset on source change');
        markCase('row04.blade-branches');
        console.log('PASS row04.blade-branches');
    }
}

function assertWorkerBHistoryRows() {
    function workerB_audio(overrides) {
        return audio(Object.assign({
            version: 6,
            sourceId: 'worker-b-source',
            profileId: 42,
            sourceEpoch: 1,
            configRevision: 1,
            timing: { deltaSeconds: 0 },
            dt: 0,
            low: 0,
            mid: 0,
            high: 0,
            beat: 0,
            bass: 0,
            powers: { raw: { low: 0, mid: 0, high: 0, bass: 0, beat: 0 } },
            banks: { full: { count: 8, processed: [0,0,0,0,0,0,0,0], novelty: [0,0,0,0,0,0,0,0] } },
            tempo: { beatPhase: 0 }
        }, overrides || {}));
    }

    function workerB_at(map, width, x, y) {
        var i = (y * width + x) * 3;
        return { h: map[i], s: map[i + 1], v: map[i + 2] };
    }

    function workerB_lit(map) {
        var lit = 0;
        for (var i = 2; i < map.length; i += 3) if (map[i] > 0.001) lit++;
        return lit;
    }

    function workerB_whiteHeads(map) {
        var c = 0;
        for (var i = 0; i < map.length; i += 3) {
            if (map[i + 2] > 0.05 && map[i + 1] < 0.02) c++;
        }
        return c;
    }

    function workerB_clamp01(v) {
        return v < 0 ? 0 : (v > 1 ? 1 : v);
    }

    function test_row05_bleep() {
        var algo = loadScript('audiobleep.js', 0x501);
        algo.colors = [{ h: 0.0, s: 1, v: 1 }, { h: 0.6, s: 1, v: 1 }];
        algo.setSource('High');
        algo.setPoints(3);
        algo.setScrollSeconds(3);
        algo.setLineWidth(1);
        algo.setMirror('No');

        function frame(value, dt, id) {
            return workerB_audio({
                sourceId: id || 'bleep-a',
                high: value,
                timing: { deltaSeconds: dt },
                dt: dt * 120 / 60
            });
        }

        algo.setMode('Points');
        render(algo, 'audiobleep.js', 9, 9, frame(0.25, 1), 'row05 old');
        render(algo, 'audiobleep.js', 9, 9, frame(0.75, 1), 'row05 mid');
        var pointsMap = render(algo, 'audiobleep.js', 9, 9, frame(0.5, 1), 'row05 new');
        assert(workerB_at(pointsMap, 9, 0, 4).v > 0.01, 'row05 points: newest should be x0,y4');
        assert(workerB_at(pointsMap, 9, 4, 2).v > 0.01, 'row05 points: mid should be x4,y2');
        assert(workerB_at(pointsMap, 9, 8, 6).v > 0.01, 'row05 points: oldest should be x8,y6');

        algo.setMode('Lines');
        algo.setLineWidth(3);
        var lineMap = render(algo, 'audiobleep.js', 9, 9, frame(0.5, 0), 'row05 lines width');
        assert(workerB_at(lineMap, 9, 2, 3).v > 0.01, 'row05 lines: connecting segment missing');
        assert(workerB_at(lineMap, 9, 2, 2).v > 0.01, 'row05 lineWidth: thickness not applied');

        algo.setMode('Fill');
        algo.setLineWidth(1);
        var fillMap = render(algo, 'audiobleep.js', 9, 9, frame(0.5, 0), 'row05 fill');
        assert(workerB_at(fillMap, 9, 0, 0).v > 0.01, 'row05 fill: must fill toward row 0');

        algo.setMirror('Yes');
        var mirrorFill = render(algo, 'audiobleep.js', 9, 9, frame(0.5, 0), 'row05 mirror fill');
        assert(workerB_at(mirrorFill, 9, 0, 2).v > 0.01 && workerB_at(mirrorFill, 9, 0, 6).v > 0.01,
            'row05 mirror: top/bottom traces missing');
        assert(workerB_at(mirrorFill, 9, 0, 4).v > 0.01, 'row05 mirror fill: center span missing');

        var gradientProbe = loadScript('audiobleep.js', 0x503);
        gradientProbe.colors = algo.colors;
        gradientProbe.setSource('High');
        gradientProbe.setMode('Points');
        gradientProbe.setMirror('No');
        gradientProbe.setPoints(3);
        gradientProbe.setScrollSeconds(3);
        render(gradientProbe, 'audiobleep.js', 9, 9, frame(0.5, 1, 'bleep-gradient'), 'row05 gradient fill1');
        render(gradientProbe, 'audiobleep.js', 9, 9, frame(0.5, 1, 'bleep-gradient'), 'row05 gradient fill2');
        render(gradientProbe, 'audiobleep.js', 9, 9, frame(0.5, 1, 'bleep-gradient'), 'row05 gradient fill3');
        var timeColors = (function() {
            gradientProbe.setColorBy('Time');
            return render(gradientProbe, 'audiobleep.js', 9, 9, frame(0.5, 0, 'bleep-gradient'), 'row05 time gradient');
        })();
        var powerColors = (function() {
            gradientProbe.setColorBy('Power');
            return render(gradientProbe, 'audiobleep.js', 9, 9, frame(0.5, 0, 'bleep-gradient'), 'row05 power gradient');
        })();
        assert(Math.abs(workerB_at(timeColors, 9, 0, 4).h - workerB_at(timeColors, 9, 8, 4).h) > 0.1,
            'row05 time gradient: colors should vary by time axis');
        assert(Math.abs(workerB_at(powerColors, 9, 0, 4).h - workerB_at(powerColors, 9, 8, 4).h) < 1e-6,
            'row05 power gradient: equal amplitudes should keep equal color');

        algo.setPoints(9);
        algo.setScrollSeconds(1);
        render(algo, 'audiobleep.js', 3, 7, frame(0.8, 1), 'row05 points > width');
        render(algo, 'audiobleep.js', 3, 7, frame(0.8, 1), 'row05 points > width2');
        var oversampled = render(algo, 'audiobleep.js', 3, 7, frame(0.8, 0), 'row05 points > width3');
        assert(workerB_lit(oversampled) > 0, 'row05 points>width: should still render visible pixels');

        var singleCol = render(algo, 'audiobleep.js', 1, 9, frame(0.4, 0.2, 'bleep-single'), 'row05 1xN');
        assert(workerB_lit(singleCol) > 0, 'row05 1xN: must render finite output');

        var stall = loadScript('audiobleep.js', 0x502);
        stall.colors = algo.colors;
        stall.setSource('High');
        stall.setMode('Points');
        stall.setPoints(5);
        stall.setScrollSeconds(5);
        render(stall, 'audiobleep.js', 5, 9, frame(0.2, 1, 'bleep-stall'), 'row05 stall base');
        var afterStall = render(stall, 'audiobleep.js', 5, 9, frame(0.8, 3, 'bleep-stall'), 'row05 stall catchup');
        for (var x = 0; x < 3; x++) {
            assert(workerB_at(afterStall, 5, x, 2).v > 0.01,
                'row05 stall: missed slots should repeat latest sample');
        }
    }

    function test_row62_waterfall() {
        var brightAtZero = loadScript('audiowaterfall.js', 0x621);
        brightAtZero.colors = [{ h: 0, s: 0, v: 1 }, { h: 0, s: 0, v: 0 }];
        brightAtZero.setBands(4);
        brightAtZero.setAggregation('Mean');
        brightAtZero.setCenterMode('Off');
        brightAtZero.setDropSeconds(2);
        brightAtZero.setFadeOut(0);
        var zeroBright = render(brightAtZero, 'audiowaterfall.js', 5, 3, workerB_audio({
            sourceId: 'wf-g0',
            banks: { full: { count: 4, novelty: [0, 0, 0, 0], processed: [1, 1, 1, 1] } }
        }), 'row62 g0 bright');
        for (var x = 0; x < 5; x++) assert(workerB_at(zeroBright, 5, x, 0).v > 0.95, 'row62 G(0) must remain bright');

        function wfFrame(values, dt, processed, id) {
            return workerB_audio({
                sourceId: id || 'wf-main',
                timing: { deltaSeconds: dt },
                dt: dt,
                banks: { full: { count: values.length, novelty: values.slice(), processed: (processed || values).slice() } }
            });
        }

        var wf = loadScript('audiowaterfall.js', 0x622);
        wf.colors = [{ h: 0, s: 0, v: 0 }, { h: 0, s: 0, v: 1 }];
        wf.setBands(4);
        wf.setAggregation('Max');
        wf.setCenterMode('Off');
        wf.setDropSeconds(3);
        wf.setFadeOut(0);
        var first = render(wf, 'audiowaterfall.js', 12, 5, wfFrame([1,0,0,0,0,0,0,0], 0), 'row62 first');
        var dup = render(wf, 'audiowaterfall.js', 12, 5, wfFrame([1,0,0,0,0,0,0,0], 0), 'row62 dup');
        var second = render(wf, 'audiowaterfall.js', 12, 5, wfFrame([0,0,0,0,0,0,0,1], 0.7), 'row62 second');
        assert.deepStrictEqual(dup, first, 'row62: zero dt duplicate must be idempotent');
        assert.notDeepStrictEqual(first.slice(0, 36), second.slice(0, 36), 'row62: distinct peaks must change top row');
        assert(second.slice(36, 72).some((v, i) => i % 3 === 2 && v > 0.9), 'row62: history row should persist');

        var meanA = loadScript('audiowaterfall.js', 0x623);
        var maxA = loadScript('audiowaterfall.js', 0x623);
        meanA.colors = maxA.colors = wf.colors;
        meanA.setBands(1); maxA.setBands(1);
        meanA.setAggregation('Mean'); maxA.setAggregation('Max');
        var vector = [0.9, 0, 0, 0];
        var meanMap = render(meanA, 'audiowaterfall.js', 4, 3, wfFrame(vector, 0, vector, 'wf-mean'), 'row62 mean');
        var maxMap = render(maxA, 'audiowaterfall.js', 4, 3, wfFrame(vector, 0, vector, 'wf-max'), 'row62 max');
        assert(workerB_at(maxMap, 4, 0, 0).v > workerB_at(meanMap, 4, 0, 0).v + 0.4, 'row62: max_vs_mean branch missing');

        var keepAgg = loadScript('audiowaterfall.js', 0x627);
        keepAgg.colors = wf.colors;
        keepAgg.setBands(1);
        keepAgg.setAggregation('Mean');
        keepAgg.setCenterMode('Off');
        keepAgg.setDropSeconds(3);
        keepAgg.setFadeOut(0);
        render(keepAgg, 'audiowaterfall.js', 8, 5, wfFrame([1,0,0,0,0,0,0,0], 0, [1,0,0,0,0,0,0,0], 'wf-keep-agg'), 'row62 keep agg seed 1');
        render(keepAgg, 'audiowaterfall.js', 8, 5, wfFrame([0,0,0,1,0,0,0,0], 0.7, [0,0,0,1,0,0,0,0], 'wf-keep-agg'), 'row62 keep agg seed 2');
        var meanTop = render(keepAgg, 'audiowaterfall.js', 8, 5, wfFrame([0.9,0,0,0,0,0,0,0], 0, [0.9,0,0,0,0,0,0,0], 'wf-keep-agg'), 'row62 keep agg mean');
        keepAgg.setAggregation('Max');
        var maxTop = render(keepAgg, 'audiowaterfall.js', 8, 5, wfFrame([0.9,0,0,0,0,0,0,0], 0, [0.9,0,0,0,0,0,0,0], 'wf-keep-agg'), 'row62 keep agg max zero');
        assert.deepStrictEqual(maxTop.slice(8 * 3), meanTop.slice(8 * 3),
            'row62: switching mean/max must preserve old rows on zero-time render');
        assert(workerB_at(maxTop, 8, 0, 0).v > workerB_at(meanTop, 8, 0, 0).v + 0.4,
            'row62: new top row must reflect changed aggregation setting');
        var maxShift = render(keepAgg, 'audiowaterfall.js', 8, 5, wfFrame([0.9,0,0,0,0,0,0,0], 0.7, [0.9,0,0,0,0,0,0,0], 'wf-keep-agg'), 'row62 keep agg max shift');
        assert(workerB_at(maxShift, 8, 0, 1).v > 0.8,
            'row62: shifted history should carry post-switch max row');

        var keepBands = loadScript('audiowaterfall.js', 0x628);
        keepBands.colors = wf.colors;
        keepBands.setBands(4);
        keepBands.setAggregation('Mean');
        keepBands.setCenterMode('Off');
        keepBands.setDropSeconds(3);
        keepBands.setFadeOut(0);
        render(keepBands, 'audiowaterfall.js', 8, 5, wfFrame([1,0,0,0,0,0,0,0], 0, [1,0,0,0,0,0,0,0], 'wf-keep-bands'), 'row62 keep bands seed 1');
        var beforeBandSwitch = render(keepBands, 'audiowaterfall.js', 8, 5, wfFrame([0,0,0,0,0,0,0,1], 0.7, [0,0,0,0,0,0,0,1], 'wf-keep-bands'), 'row62 keep bands seed 2');
        keepBands.setBands(1);
        var zeroBandSwitch = render(keepBands, 'audiowaterfall.js', 8, 5, wfFrame([0.9,0,0,0,0,0,0,0], 0, [0.9,0,0,0,0,0,0,0], 'wf-keep-bands'), 'row62 keep bands zero');
        assert.deepStrictEqual(zeroBandSwitch.slice(8 * 3), beforeBandSwitch.slice(8 * 3),
            'row62: switching bands must preserve old rows on zero-time render');
        var postBandShift = render(keepBands, 'audiowaterfall.js', 8, 5, wfFrame([0.9,0,0,0,0,0,0,0], 0.7, [0.9,0,0,0,0,0,0,0], 'wf-keep-bands'), 'row62 keep bands shift');
        assert(Math.abs(workerB_at(postBandShift, 8, 0, 0).v - workerB_at(postBandShift, 8, 7, 0).v) < 1e-6,
            'row62: post-switch bands=1 row should use new grouping');

        var centerOdd = loadScript('audiowaterfall.js', 0x624);
        centerOdd.colors = wf.colors;
        centerOdd.setBands(1);
        centerOdd.setCenterMode('On');
        centerOdd.setDropSeconds(1);
        centerOdd.setFadeOut(0);
        render(centerOdd, 'audiowaterfall.js', 5, 5, wfFrame([1], 0, [1], 'wf-odd'), 'row62 odd center seed');
        var oddShift = render(centerOdd, 'audiowaterfall.js', 5, 5, wfFrame([0], 0.6, [0], 'wf-odd'), 'row62 odd center shift');
        assert(workerB_at(oddShift, 5, 0, 1).v > 0.9 && workerB_at(oddShift, 5, 0, 3).v > 0.9,
            'row62: odd center should move history outward');

        var centerEven = loadScript('audiowaterfall.js', 0x625);
        centerEven.colors = wf.colors;
        centerEven.setBands(1);
        centerEven.setCenterMode('On');
        centerEven.setDropSeconds(1);
        centerEven.setFadeOut(0);
        render(centerEven, 'audiowaterfall.js', 5, 6, wfFrame([1], 0, [1], 'wf-even'), 'row62 even center seed');
        var evenShift = render(centerEven, 'audiowaterfall.js', 5, 6, wfFrame([0], 0.34, [0], 'wf-even'), 'row62 even center shift');
        assert(workerB_at(evenShift, 5, 0, 1).v > 0.9 && workerB_at(evenShift, 5, 0, 4).v > 0.9,
            'row62: even center should move history outward');

        var fadeCheck = loadScript('audiowaterfall.js', 0x626);
        fadeCheck.colors = wf.colors;
        fadeCheck.setBands(1);
        fadeCheck.setCenterMode('Off');
        fadeCheck.setDropSeconds(3);
        fadeCheck.setFadeOut(1);
        render(fadeCheck, 'audiowaterfall.js', 5, 5, wfFrame([1], 0, [1], 'wf-fade'), 'row62 fade seed');
        var faded = render(fadeCheck, 'audiowaterfall.js', 5, 5, wfFrame([0], 0.7, [0], 'wf-fade'), 'row62 faded');
        fadeCheck.setFadeOut(0);
        var restored = render(fadeCheck, 'audiowaterfall.js', 5, 5, wfFrame([0], 0, [0], 'wf-fade'), 'row62 fade restored');
        assert(workerB_at(restored, 5, 0, 1).v > workerB_at(faded, 5, 0, 1).v + 0.2,
            'row62: fade should affect display only');

        var clipped = render(wf, 'audiowaterfall.js', 3, 5, wfFrame([2, 2, 2, 2], 0, [2, 2, 2, 2], 'wf-clip'), 'row62 >1 clip');
        assert(clipped.every(function(v) { return v >= 0 && v <= 1; }), 'row62: clipping failed');
        var tiny = render(wf, 'audiowaterfall.js', 1, 9, wfFrame([0.5], 0.2, [0.5], 'wf-1xn'), 'row62 1xN');
        assert(workerB_lit(tiny) > 0, 'row62 1xN: no output');

        var keepIdentity = loadScript('audiowaterfall.js', 0x629);
        keepIdentity.colors = wf.colors;
        keepIdentity.setBands(8);
        keepIdentity.setAggregation('Max');
        keepIdentity.setCenterMode('Off');
        keepIdentity.setDropSeconds(3);
        keepIdentity.setFadeOut(0);
        render(keepIdentity, 'audiowaterfall.js', 8, 5, workerB_audio({
            sourceId: 'wf-ident',
            profileId: 800,
            sourceEpoch: 1,
            configRevision: 2,
            timing: { deltaSeconds: 0 },
            dt: 0,
            banks: { full: { count: 8, novelty: [1,0,0,0,0,0,0,0], processed: [1,0,0,0,0,0,0,0] } }
        }), 'row62 identity seed1');
        render(keepIdentity, 'audiowaterfall.js', 8, 5, workerB_audio({
            sourceId: 'wf-ident',
            profileId: 800,
            sourceEpoch: 1,
            configRevision: 2,
            timing: { deltaSeconds: 0.7 },
            dt: 0.7,
            banks: { full: { count: 8, novelty: [0,0,0,0,0,0,0,1], processed: [0,0,0,0,0,0,0,1] } }
        }), 'row62 identity seed2');
        var keepAfterUnavailable = render(keepIdentity, 'audiowaterfall.js', 8, 5, workerB_audio({
            sourceId: '',
            profileId: 800,
            sourceEpoch: 1,
            configRevision: 2,
            available: false,
            status: 'reset',
            timing: { deltaSeconds: 0 },
            dt: 0,
            banks: { full: { count: 8, novelty: [0,0,0,0,0,0,0,0], processed: [0,0,0,0,0,0,0,0] } }
        }), 'row62 identity hold unavailable');
        assert(workerB_at(keepAfterUnavailable, 8, 0, 1).v > 0.9,
            'row62: unavailable empty/reset frame on same profile must preserve history rows');
        var resetOnEpoch = render(keepIdentity, 'audiowaterfall.js', 8, 5, workerB_audio({
            sourceId: 'wf-ident',
            profileId: 800,
            sourceEpoch: 2,
            configRevision: 2,
            timing: { deltaSeconds: 0 },
            dt: 0,
            banks: { full: { count: 8, novelty: [0,0,0,0,0,0,0,0], processed: [0,0,0,0,0,0,0,0] } }
        }), 'row62 identity epoch reset');
        assert(workerB_at(resetOnEpoch, 8, 0, 1).v < 0.05,
            'row62: sourceEpoch change must reset history');
    }

    function test_row12_digital_rain() {
        function rainFrame(dt, beatPhase, raw, extras) {
            var payload = {
                sourceId: 'rain-main',
                timing: { deltaSeconds: dt },
                dt: dt,
                tempo: beatPhase == null ? undefined : { beatPhase: beatPhase },
                powers: { raw: { low: raw[0], mid: raw[1], high: raw[2], bass: 0, beat: 0 } },
                low: raw[0], mid: raw[1], high: raw[2]
            };
            if (extras) for (var k in extras) payload[k] = extras[k];
            return workerB_audio(payload);
        }

        var base = loadScript('audiodigitalrain.js', 0x1201);
        base.setCount(1.2);
        base.setMaxStreams(8);
        base.setAddSpeed(20);
        base.setTail(0.5);
        base.setTailSegments(10);
        base.setRunSeconds(2);
        base.setLineWidth(5);
        base.setSpeed(1);
        base.setMultiplier(10);
        base.setImpulseDecay(0.1);

        var first = render(base, 'audiodigitalrain.js', 12, 7, rainFrame(0.1, 0.1, [0.1, 0.2, 0.9]), 'row12 first');
        var dup = render(base, 'audiodigitalrain.js', 12, 7, rainFrame(0, 0.1, [0.1, 0.2, 0.9]), 'row12 dup');
        var moved = render(base, 'audiodigitalrain.js', 12, 7, rainFrame(0.2, 0.1, [0.1, 0.2, 0.9]), 'row12 moved');
        assert.deepStrictEqual(dup, first, 'row12: zero dt advanced state');
        assert.notDeepStrictEqual(moved, first, 'row12: positive dt failed to move streams');

        var phaseA = loadScript('audiodigitalrain.js', 0x1202);
        phaseA.setCount(0.5); phaseA.setMaxStreams(1); phaseA.setAddSpeed(30); phaseA.setRunSeconds(5); phaseA.setTail(0.2);
        render(phaseA, 'audiodigitalrain.js', 9, 9, rainFrame(0.1, 0.1, [0,0,0], { sourceId: 'rain-phase' }), 'row12 phase seed');
        var headA = render(phaseA, 'audiodigitalrain.js', 9, 9, rainFrame(0, 0.1, [0,0,0], { sourceId: 'rain-phase' }), 'row12 phase a');
        var headB = render(phaseA, 'audiodigitalrain.js', 9, 9, rainFrame(0, 0.6, [0,0,0], { sourceId: 'rain-phase' }), 'row12 phase b');
        function peakV(map) { var p = 0; for (var i = 2; i < map.length; i += 3) p = Math.max(p, map[i]); return p; }
        assert(Math.abs((peakV(headB) - peakV(headA)) - 0.25) < 0.06,
            'row12: beat phase offset must shift head brightness by ~0.25 for Δphase=0.5');

        var slowBirth = loadScript('audiodigitalrain.js', 0x1203);
        var fastBirth = loadScript('audiodigitalrain.js', 0x1203);
        [slowBirth, fastBirth].forEach(function(a) {
            a.setCount(2); a.setMaxStreams(20); a.setTail(0.4); a.setTailSegments(10); a.setRunSeconds(5); a.setMultiplier(0);
        });
        slowBirth.setAddSpeed(1);
        fastBirth.setAddSpeed(20);
        var slowMap = render(slowBirth, 'audiodigitalrain.js', 16, 10, rainFrame(0.5, null, [0,0,0], { sourceId: 'rain-birth' }), 'row12 slow birth');
        var fastMap = render(fastBirth, 'audiodigitalrain.js', 16, 10, rainFrame(0.5, null, [0,0,0], { sourceId: 'rain-birth' }), 'row12 fast birth');
        assert(workerB_whiteHeads(fastMap) > workerB_whiteHeads(slowMap), 'row12: addSpeed should be independent birth control');

        var seg2 = loadScript('audiodigitalrain.js', 0x1204);
        var seg10 = loadScript('audiodigitalrain.js', 0x1204);
        [seg2, seg10].forEach(function(a) {
            a.setCount(0.8); a.setMaxStreams(4); a.setAddSpeed(20); a.setRunSeconds(3); a.setTail(0.8); a.setMultiplier(0);
        });
        seg2.setTailSegments(2);
        seg10.setTailSegments(10);
        render(seg2, 'audiodigitalrain.js', 10, 12, rainFrame(0.2, 0.2, [0,0,0], { sourceId: 'rain-tail' }), 'row12 seg2 seed');
        render(seg10, 'audiodigitalrain.js', 10, 12, rainFrame(0.2, 0.2, [0,0,0], { sourceId: 'rain-tail' }), 'row12 seg10 seed');
        var seg2Map = render(seg2, 'audiodigitalrain.js', 10, 12, rainFrame(0.9, 0.2, [0,0,0], { sourceId: 'rain-tail' }), 'row12 seg2');
        var seg10Map = render(seg10, 'audiodigitalrain.js', 10, 12, rainFrame(0.9, 0.2, [0,0,0], { sourceId: 'rain-tail' }), 'row12 seg10');
        assert.notDeepStrictEqual(seg2Map, seg10Map, 'row12: tail segment count branch missing');

        var headOnly = loadScript('audiodigitalrain.js', 0x1205);
        headOnly.setCount(0.6); headOnly.setAddSpeed(20); headOnly.setMaxStreams(3); headOnly.setTail(1); headOnly.setTailSegments(10); headOnly.setLineWidth(30);
        var headOnlyMap = render(headOnly, 'audiodigitalrain.js', 8, 8, rainFrame(0.2, 0.25, [0,0,0], { sourceId: 'rain-headonly' }), 'row12 short tail');
        assert(workerB_whiteHeads(headOnlyMap) > 0, 'row12: short tail should still render head');

        var accelLow = loadScript('audiodigitalrain.js', 0x1206);
        var accelMid = loadScript('audiodigitalrain.js', 0x1206);
        [accelLow, accelMid].forEach(function(a) {
            a.setCount(1.5); a.setAddSpeed(30); a.setMaxStreams(8); a.setTail(0.5); a.setTailSegments(6); a.setRunSeconds(3); a.setMultiplier(10);
        });
        render(accelLow, 'audiodigitalrain.js', 12, 12, rainFrame(0.1, 0.2, [0,0,0], { sourceId: 'rain-accel' }), 'row12 accel seed low');
        render(accelMid, 'audiodigitalrain.js', 12, 12, rainFrame(0.1, 0.2, [0,0,0], { sourceId: 'rain-accel' }), 'row12 accel seed mid');
        var lowPulse = render(accelLow, 'audiodigitalrain.js', 12, 12, rainFrame(0.3, 0.2, [1,0,0], { sourceId: 'rain-accel' }), 'row12 low pulse');
        var midPulse = render(accelMid, 'audiodigitalrain.js', 12, 12, rainFrame(0.3, 0.2, [0,1,0], { sourceId: 'rain-accel' }), 'row12 mid pulse');
        assert.notDeepStrictEqual(lowPulse, midPulse, 'row12: per-stream raw-band acceleration branch missing');

        var unavailable = loadScript('audiodigitalrain.js', 0x1207);
        unavailable.setCount(0.8); unavailable.setAddSpeed(10); unavailable.setMaxStreams(4); unavailable.setMultiplier(10);
        render(unavailable, 'audiodigitalrain.js', 10, 8, rainFrame(0.2, null, [0,0,0], { sourceId: 'rain-unavail' }), 'row12 unavailable seed');
        var unavailMove = render(unavailable, 'audiodigitalrain.js', 10, 8, rainFrame(0.3, null, [0,0,0], { sourceId: 'rain-unavail' }), 'row12 unavailable motion');
        assert(workerB_lit(unavailMove) > 0, 'row12: no-beat unavailable input should still move/live');

        var identity = loadScript('audiodigitalrain.js', 0x1208);
        identity.setCount(1.2); identity.setAddSpeed(30); identity.setMaxStreams(8); identity.setRunSeconds(5); identity.setTail(0.5);
        var idSeed = render(identity, 'audiodigitalrain.js', 10, 8,
            rainFrame(0.2, 0.25, [0.1,0.1,0.1], { sourceId: 'rain-ident', profileId: 700, sourceEpoch: 1, configRevision: 4, available: true }),
            'row12 identity seed');
        var idHold = render(identity, 'audiodigitalrain.js', 10, 8,
            rainFrame(0, 0.25, [0,0,0], { sourceId: '', profileId: 700, sourceEpoch: 1, configRevision: 4, available: false, status: 'reset' }),
            'row12 identity hold unavailable');
        assert.deepStrictEqual(idHold, idSeed,
            'row12: unavailable empty/reset frame on same profile must keep identity and state');
        var idEpochReset = render(identity, 'audiodigitalrain.js', 10, 8,
            rainFrame(0, 0.25, [0,0,0], { sourceId: 'rain-ident', profileId: 700, sourceEpoch: 2, configRevision: 4, available: true }),
            'row12 identity epoch reset');
        assert.notDeepStrictEqual(idEpochReset, idHold, 'row12: sourceEpoch change must reset state');
        var idProfileReset = render(identity, 'audiodigitalrain.js', 10, 8,
            rainFrame(0, 0.25, [0,0,0], { sourceId: 'rain-ident', profileId: 701, sourceEpoch: 1, configRevision: 4, available: true }),
            'row12 identity profile reset');
        assert.notDeepStrictEqual(idProfileReset, idHold, 'row12: profileId change must reset state');
    }

    function test_row16_equalizer2d() {
        function eqFrame(levels, dt, opts) {
            return workerB_audio(Object.assign({
                sourceId: 'eq-main',
                timing: { deltaSeconds: dt },
                dt: dt,
                low: 0.25,
                mid: 0.4,
                high: 0.7,
                banks: { full: { count: levels.length, processed: levels.slice(), novelty: levels.slice().map(function(v) { return workerB_clamp01(v * 0.5); }) } }
            }, opts || {}));
        }

        var levels = [0.25, 0.8, 0.5, 0.1];
        var off = loadScript('audioequalizer2d.js', 0x1601);
        off.colors = [{ h: 0.0, s: 1, v: 1 }, { h: 0.5, s: 1, v: 1 }];
        off.setMode('Bars'); off.setCenter('Off'); off.setBandCount(4); off.setAggregation('Mean'); off.setColorMode('Off');
        var offMap = render(off, 'audioequalizer2d.js', 8, 9, eqFrame(levels, 0.05), 'row16 bars off');
        assert(Math.abs(workerB_at(offMap, 8, 0, 8).h - workerB_at(offMap, 8, 6, 8).h) > 0.1,
            'row16 off: colors must vary by band index');

        var solid = loadScript('audioequalizer2d.js', 0x1602);
        solid.colors = off.colors;
        solid.setMode('Bars'); solid.setCenter('Off'); solid.setBandCount(4); solid.setColorMode('Solid');
        var solidMap = render(solid, 'audioequalizer2d.js', 8, 9, eqFrame(levels, 0.05), 'row16 bars solid');
        assert(Math.abs(workerB_at(solidMap, 8, 2, 8).h - workerB_at(solidMap, 8, 2, 6).h) < 1e-6,
            'row16 solid: within-bar hue should stay constant');

        var progressive = loadScript('audioequalizer2d.js', 0x1603);
        progressive.colors = off.colors;
        progressive.setMode('Bars'); progressive.setCenter('Off'); progressive.setBandCount(4); progressive.setColorMode('Progressive');
        var progMap = render(progressive, 'audioequalizer2d.js', 8, 9, eqFrame(levels, 0.05), 'row16 bars progressive');
        assert(Math.abs(workerB_at(progMap, 8, 0, 8).h - workerB_at(progMap, 8, 6, 8).h) < 0.06,
            'row16 progressive: same row should share gradient coord');

        var stretch = loadScript('audioequalizer2d.js', 0x1604);
        stretch.colors = off.colors;
        stretch.setMode('Bars'); stretch.setCenter('Off'); stretch.setBandCount(4); stretch.setColorMode('Stretch');
        var stretchMap = render(stretch, 'audioequalizer2d.js', 8, 9, eqFrame(levels, 0.05), 'row16 bars stretch');
        assert(Math.abs(workerB_at(stretchMap, 8, 2, 8).h - workerB_at(stretchMap, 8, 2, 4).h) > 0.06,
            'row16 stretch: within-bar hue should vary with bar depth');

        var peak = loadScript('audioequalizer2d.js', 0x1605);
        peak.colors = off.colors;
        peak.setMode('Bars'); peak.setCenter('Off'); peak.setBandCount(4); peak.setColorMode('Solid'); peak.setPeakColor('#ffffff'); peak.setPeakThickness(1); peak.setPeakDecay(0.2);
        render(peak, 'audioequalizer2d.js', 8, 9, eqFrame([1, 1, 1, 1], 0.05), 'row16 peak seed');
        var peakDrop = render(peak, 'audioequalizer2d.js', 8, 9, eqFrame([0.1, 0.1, 0.1, 0.1], 0.05), 'row16 peak hold');
        assert(peakDrop.some(function(v, i) { return i % 3 === 1 && v < 0.05; }), 'row16 peak: white markers missing');

        function singleBandFrame(center) {
            return eqFrame([1, 0, 0, 0, 0, 0, 0, 0], 0.05, {
                sourceId: center ? 'eq-ring-center' : 'eq-ring-edge',
                low: 1,
                mid: 0,
                high: 0
            });
        }

        var ringCenter = loadScript('audioequalizer2d.js', 0x1606);
        ringCenter.colors = off.colors;
        ringCenter.setMode('Ring'); ringCenter.setCenter('On'); ringCenter.setBandCount(8); ringCenter.setColorMode('Off'); ringCenter.setSpin(0);
        var centerMap = render(ringCenter, 'audioequalizer2d.js', 17, 13, singleBandFrame(true), 'row16 ring center');
        assert(workerB_at(centerMap, 17, 8, 6).v > 0.2, 'row16 ring center: center-origin triangle missing');

        var ringEdge = loadScript('audioequalizer2d.js', 0x1607);
        ringEdge.colors = off.colors;
        ringEdge.setMode('Ring'); ringEdge.setCenter('Off'); ringEdge.setBandCount(8); ringEdge.setColorMode('Off'); ringEdge.setSpin(0);
        var edgeMap = render(ringEdge, 'audioequalizer2d.js', 17, 13, singleBandFrame(false), 'row16 ring edge');
        assert(workerB_at(edgeMap, 17, 8, 6).v < 0.05, 'row16 ring edge: perimeter-origin triangle should avoid center');

        var spinning = loadScript('audioequalizer2d.js', 0x1608);
        spinning.colors = off.colors;
        spinning.setMode('Ring'); spinning.setCenter('Off'); spinning.setBandCount(8); spinning.setColorMode('Off');
        spinning.setSpin(2); spinning.setSpinSource('Lows'); spinning.setMultiplier(4); spinning.setDecay(0.1);
        var spinA = render(spinning, 'audioequalizer2d.js', 17, 13, eqFrame([1,0,0,0,0,0,0,0], 0.1, { sourceId: 'eq-spin', low: 1 }), 'row16 spin a');
        var spinB = render(spinning, 'audioequalizer2d.js', 17, 13, eqFrame([1,0,0,0,0,0,0,0], 0.3, { sourceId: 'eq-spin', low: 1 }), 'row16 spin b');
        assert.notDeepStrictEqual(spinA, spinB, 'row16 spin: triangle should rotate with spin impulse');

        var deg1 = loadScript('audioequalizer2d.js', 0x1609);
        deg1.setMode('Ring'); deg1.setCenter('Off'); deg1.setBandCount(1); deg1.setColorMode('Off');
        var degMap1 = render(deg1, 'audioequalizer2d.js', 7, 7, eqFrame([1], 0.05, { sourceId: 'eq-deg1' }), 'row16 ring bands1');
        assert(degMap1.every(function(v) { return Number.isFinite(v) && v >= 0 && v <= 1; }), 'row16 ring bands1: non-finite output');

        var deg2 = loadScript('audioequalizer2d.js', 0x1610);
        deg2.setMode('Ring'); deg2.setCenter('On'); deg2.setBandCount(2); deg2.setColorMode('Off');
        var degMap2 = render(deg2, 'audioequalizer2d.js', 7, 7, eqFrame([1, 0], 0.05, { sourceId: 'eq-deg2' }), 'row16 ring bands2');
        assert(degMap2.every(function(v) { return Number.isFinite(v) && v >= 0 && v <= 1; }), 'row16 ring bands2: non-finite output');

        var overBands = loadScript('audioequalizer2d.js', 0x1611);
        overBands.setMode('Bars'); overBands.setCenter('Off'); overBands.setBandCount(32); overBands.setColorMode('Off');
        var overMap = render(overBands, 'audioequalizer2d.js', 5, 9, eqFrame([0,0,1,0], 0.05, { sourceId: 'eq-over' }), 'row16 bands>width');
        assert(workerB_lit(overMap) > 0, 'row16 bands>width: should remain visible');
    }

    test_row05_bleep();
    markCase('row05-bleep');
    console.log('PASS row05 bleep');

    test_row12_digital_rain();
    markCase('row12-digitalrain2d');
    console.log('PASS row12 digitalrain2d');

    test_row16_equalizer2d();
    markCase('row16-equalizer2d');
    console.log('PASS row16 equalizer2d');

    test_row62_waterfall();
    markCase('row62-waterfall2d');
    console.log('PASS row62 waterfall2d');
}

function assertWorkerCSimulationRows() {
    const ORACLE_TOLERANCE = 1e-6;
    const LIFE_PATTERNS = [
        [[0, 0], [1, 0], [2, 0], [2, -1], [1, -2]],
        [[-1, 0], [0, 0], [1, 0]],
        [[-1, 0], [0, 0], [1, 0], [0, -1], [1, -1], [2, -1]],
        [[0, 0], [1, 0], [0, 1], [3, 2], [2, 3], [3, 3]]
    ];

    function lcg(seed) {
        let value = seed >>> 0;
        return function() {
            value = (value * 1664525 + 1013904223) >>> 0;
            return value / 0x100000000;
        };
    }

    function clamp01(x) {
        return x < 0 ? 0 : (x > 1 ? 1 : x);
    }

    function approx(actual, expected, eps, label) {
        assert(Math.abs(actual - expected) <= eps,
            `${label}: expected ${expected}, got ${actual}`);
    }

    function hasDifference(a, b, eps = 1e-9) {
        if (a.length !== b.length) return true;
        for (let i = 0; i < a.length; i++)
            if (Math.abs(a[i] - b[i]) > eps) return true;
        return false;
    }

    function meanAbsDiff(a, b) {
        assert.strictEqual(a.length, b.length, 'meanAbsDiff length mismatch');
        let sum = 0;
        for (let i = 0; i < a.length; i++) sum += Math.abs(a[i] - b[i]);
        return sum / a.length;
    }

    function maxValue(map) {
        let peak = 0;
        for (let i = 2; i < map.length; i += 3)
            if (map[i] > peak) peak = map[i];
        return peak;
    }

    function litCount(map, threshold) {
        let count = 0;
        for (let i = 2; i < map.length; i += 3)
            if (map[i] > threshold) count++;
        return count;
    }

    function mkFrame(overrides) {
        return audio(Object.assign({
            version: 6,
            sourceId: 'worker-c-source',
            profileId: 3,
            sourceEpoch: 1,
            configRevision: 1,
            frameSequence: 1,
            timing: { deltaSeconds: 0 },
            powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } },
            events: { delta: { kick: 0, beat: 0 } }
        }, overrides || {}));
    }

    function loadUtilOracle() {
        const utilSandbox = { Math, Float32Array };
        vm.createContext(utilSandbox);
        vm.runInContext(
            fs.readFileSync(path.join(__dirname, '..', 'resources', 'huescripts', 'hsvutil.js'), 'utf8'),
            utilSandbox,
            { filename: 'hsvutil.js' }
        );
        return utilSandbox.HSVUtil;
    }

    function assertArrayClose(actual, expected, eps, id) {
        assert.strictEqual(actual.length, expected.length, `${id}: length mismatch`);
        for (let i = 0; i < actual.length; i++)
            approx(actual[i], expected[i], eps, `${id} @${i}`);
    }

    function randomFromSequence(sequence, fallback) {
        let index = 0;
        const tail = fallback === undefined ? 0.91 : fallback;
        return function() {
            if (index < sequence.length) return sequence[index++];
            return tail;
        };
    }

    function randomForBoard(width, height, liveCoords, tail) {
        const live = new Set(liveCoords.map(([x, y]) => y * width + x));
        const sequence = [];
        for (let i = 0; i < width * height; i++)
            sequence.push(live.has(i) ? 0.1 : 0.9);
        return randomFromSequence(sequence, tail);
    }

    function randomForBoardAndAnchors(width, height, liveCoords, anchors, extraBoardCoords) {
        const live = new Set(liveCoords.map(([x, y]) => y * width + x));
        const sequence = [];
        for (let i = 0; i < width * height; i++)
            sequence.push(live.has(i) ? 0.1 : 0.9);
        for (const [x, y] of anchors) {
            sequence.push((x + 0.01) / width);
            sequence.push((y + 0.01) / height);
        }
        if (extraBoardCoords) {
            const extra = new Set(extraBoardCoords.map(([x, y]) => y * width + x));
            for (let i = 0; i < width * height; i++)
                sequence.push(extra.has(i) ? 0.1 : 0.9);
        }
        return randomFromSequence(sequence, 0.9);
    }

    function loadLife(seed, random) {
        return loadScript('audiogameoflife.js', seed, SCRIPTS_DIR, { random });
    }

    function lifeCellIndex(width, height, x, y) {
        const xx = (x + width) % width;
        const yy = (y + height) % height;
        return yy * width + xx;
    }

    function lifeBoardFromMap(map) {
        const board = new Array(map.length / 3);
        for (let i = 0; i < board.length; i++)
            board[i] = map[i * 3 + 2] > 0.6 ? 1 : 0;
        return board;
    }

    function boardEquals(a, b) {
        if (a.length !== b.length) return false;
        for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
        return true;
    }

    function lifeStepToroidal(board, width, height) {
        const next = new Array(board.length).fill(0);
        for (let y = 0; y < height; y++) {
            for (let x = 0; x < width; x++) {
                let n = 0;
                for (let dy = -1; dy <= 1; dy++) {
                    for (let dx = -1; dx <= 1; dx++) {
                        if (dx === 0 && dy === 0) continue;
                        n += board[lifeCellIndex(width, height, x + dx, y + dy)];
                    }
                }
                const id = lifeCellIndex(width, height, x, y);
                const alive = board[id] === 1;
                next[id] = (alive && (n === 2 || n === 3)) || (!alive && n === 3) ? 1 : 0;
            }
        }
        return next;
    }

    function lifeStamp(board, width, height, pattern, anchorX, anchorY) {
        const out = board.slice();
        if (width <= 4 || height <= 4) return out;
        for (const [dx, dy] of pattern)
            out[lifeCellIndex(width, height, anchorX + dx, anchorY + dy)] = 1;
        return out;
    }

    function hueDelta(a, b) {
        let d = Math.abs(a - b) % 1;
        if (d > 0.5) d = 1 - d;
        return d;
    }

    const util = loadUtilOracle();

    function rawLowOf(frame) {
        return clamp01(frame && frame.powers && frame.powers.raw && Number.isFinite(frame.powers.raw.low)
            ? frame.powers.raw.low
            : 0);
    }

    function expectedNoiseLikeMap({
        seed, width, height, frame, colors, speed, stretch, zoom, impulseDecay, multiplier, useFbm
    }) {
        const rng = lcg(seed);
        const state = { x: rng(), y: rng(), z: rng(), lowsImpulse: 0 };
        const dt = Math.max(0, frame && frame.timing && Number.isFinite(frame.timing.deltaSeconds)
            ? frame.timing.deltaSeconds : 0);
        if (speed > 0 && dt > 0) {
            const target = rawLowOf(frame) * multiplier;
            const alpha = target > state.lowsImpulse ? 0.99 : impulseDecay;
            state.lowsImpulse += (target - state.lowsImpulse) * alpha;
        }
        const mov = 0.5 * speed * dt;
        state.x += mov;
        state.z += mov;
        if (height > 1) state.y += mov;

        const baseScaleX = zoom / Math.max(1, width);
        const baseScaleY = zoom / Math.max(1, height);
        const bassX = baseScaleX * state.lowsImpulse;
        const bassY = height > 1
            ? baseScaleY * state.lowsImpulse
            : (baseScaleY * state.lowsImpulse * (1 / Math.max(1, width)));
        const scaleX = baseScaleX + bassX;
        const scaleY = baseScaleY + bassY;
        const noiseX = state.x - (scaleX * height * 0.5);
        const noiseY = state.y - (scaleY * width * 0.5);

        function fbm(nx, ny, nz) {
            let total = 0;
            let amp = 0.5;
            let freq = 1;
            for (let octave = 0; octave < 4; octave++) {
                total += amp * util.simplex3d(nx * freq, ny * freq, nz * freq);
                freq *= 2;
                amp *= 0.5;
            }
            return total;
        }

        const map = new Array(width * height * 3);
        const palette = colors && colors.length ? colors : [{ h: 0.6, s: 1, v: 1 }];
        for (let y = 0; y < height; y++) {
            for (let x = 0; x < width; x++) {
                const nx = noiseX + scaleX * y;
                const ny = noiseY + scaleY * x;
                const n = useFbm ? fbm(nx, ny, state.z) : util.simplex3d(nx, ny, state.z);
                const t = util.clamp01((n * stretch + 1) * 0.5);
                const hsv = util.gradientLedfxAt(palette, t);
                const i = (y * width + x) * 3;
                map[i] = hsv.h;
                map[i + 1] = hsv.s;
                map[i + 2] = util.clamp01(hsv.v);
            }
        }
        return map;
    }

    function caseNoiseRow36() {
        const seed = 0xc36031;
        const algo = loadScript('audionoise.js', seed);
        algo.setSpeed(1.3);
        algo.setStretch(1.1);
        algo.setZoom(3.2);
        algo.setImpulseDecay(0.08);
        algo.setMultiplier(2.4);
        const input = mkFrame({
            timing: { deltaSeconds: 0.21 },
            powers: { raw: { beat: 0.35, bass: 0.52, low: 0.61, mid: 0.07, high: 0.03 } }
        });
        const actual = render(algo, 'audionoise.js', 8, 6, input, 'C36 oracle');
        const expected = expectedNoiseLikeMap({
            seed,
            width: 8,
            height: 6,
            frame: input,
            colors: algo.colors,
            speed: 1.3,
            stretch: 1.1,
            zoom: 3.2,
            impulseDecay: 0.08,
            multiplier: 2.4,
            useFbm: false
        });
        for (const [x, y] of [[2, 2], [4, 3], [6, 1]]) {
            const i = (y * 8 + x) * 3;
            assertArrayClose(actual.slice(i, i + 3), expected.slice(i, i + 3), ORACLE_TOLERANCE,
                `C36/noise/3d-pixel-oracle ${x},${y}`);
        }

        const slow = loadScript('audionoise.js', 0xc36032);
        const fast = loadScript('audionoise.js', 0xc36032);
        slow.setSpeed(1); fast.setSpeed(1);
        slow.setMultiplier(2); fast.setMultiplier(2);
        slow.setImpulseDecay(0.01); fast.setImpulseDecay(0.3);
        const excite = mkFrame({ timing: { deltaSeconds: 0.2 }, powers: { raw: { beat: 0, bass: 1, low: 1, mid: 0, high: 0 } } });
        render(slow, 'audionoise.js', 9, 1, excite, 'C36 excite slow');
        render(fast, 'audionoise.js', 9, 1, excite, 'C36 excite fast');
        const drop = mkFrame({ frameSequence: 2, timing: { deltaSeconds: 0.2 }, powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } } });
        const slowDrop = render(slow, 'audionoise.js', 9, 1, drop, 'C36 drop slow');
        const fastDrop = render(fast, 'audionoise.js', 9, 1, drop, 'C36 drop fast');
        assert(meanAbsDiff(slowDrop, fastDrop) > 1e-4,
            'C36/noise/impulse-decay-control: decay setting had no visible effect after raw-low drop');

        const unavailable = loadScript('audionoise.js', 0xc36033);
        unavailable.setSpeed(1.1);
        unavailable.setMultiplier(2);
        const t1 = render(unavailable, 'audionoise.js', 9, 1,
            mkFrame({ frameSequence: 1, timing: { deltaSeconds: 0.2 }, powers: null }), 'C36 unavailable 1');
        const t2 = render(unavailable, 'audionoise.js', 9, 1,
            mkFrame({ frameSequence: 2, timing: { deltaSeconds: 0.2 }, powers: null }), 'C36 unavailable 2');
        assert(hasDifference(t1, t2),
            'C36/noise/unavailable-time-continues: motion stopped when audio powers unavailable');
    }

    function caseSmokeRow54() {
        const seed = 0xc54031;
        const algo = loadScript('audiosmoke.js', seed);
        algo.setSpeed(0.9);
        algo.setStretch(1.8);
        algo.setZoom(2.6);
        algo.setImpulseDecay(0.07);
        algo.setMultiplier(1.7);
        const input = mkFrame({
            timing: { deltaSeconds: 0.19 },
            powers: { raw: { beat: 0.25, bass: 0.47, low: 0.66, mid: 0.09, high: 0.02 } }
        });
        const actual = render(algo, 'audiosmoke.js', 9, 7, input, 'C54 fbm oracle');
        const expected = expectedNoiseLikeMap({
            seed,
            width: 9,
            height: 7,
            frame: input,
            colors: algo.colors,
            speed: 0.9,
            stretch: 1.8,
            zoom: 2.6,
            impulseDecay: 0.07,
            multiplier: 1.7,
            useFbm: true
        });
        for (const [x, y] of [[2, 3], [4, 4], [6, 2]]) {
            const i = (y * 9 + x) * 3;
            assertArrayClose(actual.slice(i, i + 3), expected.slice(i, i + 3), ORACLE_TOLERANCE,
                `C54/smoke/fbm4-pixel-oracle ${x},${y}`);
        }

        const zoomNear = loadScript('audiosmoke.js', 0xc54032);
        const zoomFar = loadScript('audiosmoke.js', 0xc54032);
        zoomNear.setSpeed(1); zoomFar.setSpeed(1);
        zoomNear.setMultiplier(0); zoomFar.setMultiplier(0);
        zoomNear.setZoom(0.5); zoomFar.setZoom(9);
        const nearMap = render(zoomNear, 'audiosmoke.js', 9, 7,
            mkFrame({ timing: { deltaSeconds: 0.3 } }), 'C54 zoom near');
        const farMap = render(zoomFar, 'audiosmoke.js', 9, 7,
            mkFrame({ timing: { deltaSeconds: 0.3 } }), 'C54 zoom far');
        assert(hasDifference(nearMap, farMap, 1e-6),
            'C54/smoke/zoom-shape-change: zoom did not reshape same-area field');

        const slow = loadScript('audiosmoke.js', 0xc54033);
        const fast = loadScript('audiosmoke.js', 0xc54033);
        slow.setSpeed(1); fast.setSpeed(1);
        slow.setMultiplier(2); fast.setMultiplier(2);
        slow.setImpulseDecay(0.01); fast.setImpulseDecay(0.3);
        const excite = mkFrame({ timing: { deltaSeconds: 0.2 }, powers: { raw: { beat: 0, bass: 1, low: 1, mid: 0, high: 0 } } });
        render(slow, 'audiosmoke.js', 9, 1, excite, 'C54 excite slow');
        render(fast, 'audiosmoke.js', 9, 1, excite, 'C54 excite fast');
        const drop = mkFrame({ frameSequence: 2, timing: { deltaSeconds: 0.2 }, powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } } });
        const slowDrop = render(slow, 'audiosmoke.js', 9, 1, drop, 'C54 drop slow');
        const fastDrop = render(fast, 'audiosmoke.js', 9, 1, drop, 'C54 drop fast');
        assert(meanAbsDiff(slowDrop, fastDrop) > 1e-4,
            'C54/smoke/impulse-decay-control: decay setting had no visible effect after raw-low drop');

        const unavailable = loadScript('audiosmoke.js', 0xc54034);
        unavailable.setSpeed(1.1);
        unavailable.setMultiplier(2);
        const t1 = render(unavailable, 'audiosmoke.js', 9, 1,
            mkFrame({ frameSequence: 1, timing: { deltaSeconds: 0.2 }, powers: null }), 'C54 unavailable 1');
        const t2 = render(unavailable, 'audiosmoke.js', 9, 1,
            mkFrame({ frameSequence: 2, timing: { deltaSeconds: 0.2 }, powers: null }), 'C54 unavailable 2');
        assert(hasDifference(t1, t2),
            'C54/smoke/unavailable-time-continues: motion stopped when audio powers unavailable');
    }

    function dominantHueCounts(map, minV) {
        const hit = { red: 0, green: 0, blue: 0 };
        for (let i = 0; i < map.length; i += 3) {
            const h = map[i], v = map[i + 2];
            if (v < minV) continue;
            if (h <= 0.08 || h >= 0.92) hit.red++;
            else if (h >= 0.22 && h <= 0.46) hit.green++;
            else if (h >= 0.52 && h <= 0.78) hit.blue++;
        }
        return hit;
    }

    function topPixelSV(map) {
        let index = 0;
        for (let i = 2; i < map.length; i += 3)
            if (map[i] > map[index + 2]) index = i - 2;
        return { s: map[index + 1], v: map[index + 2] };
    }

    function centroidRow(map, width, height, threshold) {
        let weighted = 0;
        let total = 0;
        for (let y = 0; y < height; y++) {
            for (let x = 0; x < width; x++) {
                const i = (y * width + x) * 3;
                const v = map[i + 2];
                if (v > threshold) {
                    weighted += y * v;
                    total += v;
                }
            }
        }
        return total > 0 ? weighted / total : height;
    }

    function caseFlameRow20() {
        const full = loadScript('audioflame.js', 0xc20031);
        full.setSpawnRate(50);
        full.setIntensity(1);
        full.setBlur(0);
        full.setTopTrips(0);
        full.setColorLow('#ff0000');
        full.setColorMid('#00ff00');
        full.setColorHigh('#0000ff');
        const fullMap = render(full, 'audioflame.js', 9, 6,
            mkFrame({ low: 0.9, mid: 0.8, high: 0.7, timing: { deltaSeconds: 0.5 } }), 'C20 all populations');

        const noMid = loadScript('audioflame.js', 0xc20031);
        noMid.setSpawnRate(50);
        noMid.setIntensity(1);
        noMid.setBlur(0);
        noMid.setTopTrips(0);
        noMid.setColorLow('#ff0000');
        noMid.setColorMid('#000000');
        noMid.setColorHigh('#0000ff');
        const noMidMap = render(noMid, 'audioflame.js', 9, 6,
            mkFrame({ low: 0.9, mid: 0.8, high: 0.7, timing: { deltaSeconds: 0.5 } }), 'C20 mid disabled');

        const hitsA = dominantHueCounts(fullMap, 0.07);
        const hitsB = dominantHueCounts(noMidMap, 0.07);
        assert(hitsA.red > 0 && hitsA.green > 0 && hitsA.blue > 0,
            'C20/flame/three-populations: expected red/green/blue populations together');
        assert(hitsB.green === 0 && hitsB.red > 0 && hitsB.blue > 0,
            'C20/flame/disable-one-population: black should disable only one population');

        const lowOnlyA = loadScript('audioflame.js', 0xc20032);
        const lowOnlyB = loadScript('audioflame.js', 0xc20032);
        for (const algo of [lowOnlyA, lowOnlyB]) {
            algo.setSpawnRate(40);
            algo.setIntensity(1.0);
            algo.setBlur(0);
            algo.setColorLow('#ff0000');
            algo.setColorMid('#000000');
            algo.setColorHigh('#000000');
        }
        const baseline = render(lowOnlyA, 'audioflame.js', 9, 6,
            mkFrame({ low: 0.7, mid: 0, high: 0, timing: { deltaSeconds: 0.4 } }), 'C20 low only');
        const decoy = render(lowOnlyB, 'audioflame.js', 9, 6,
            mkFrame({ low: 0.7, mid: 1, high: 1, timing: { deltaSeconds: 0.4 } }), 'C20 low only decoy');
        assert.deepStrictEqual(decoy, baseline,
            'C20/flame/assigned-band-only-height-wobble: disabled populations changed low-band dynamics');

        const dynamic = loadScript('audioflame.js', 0xc20033);
        dynamic.setSpawnRate(45);
        dynamic.setIntensity(1.2);
        dynamic.setBlur(0);
        dynamic.setTopTrips(0);
        dynamic.setColorLow('#ff3300');
        dynamic.setColorMid('#000000');
        dynamic.setColorHigh('#000000');
        const born = render(dynamic, 'audioflame.js', 9, 7,
            mkFrame({ low: 0.9, timing: { deltaSeconds: 0.45 } }), 'C20 born');
        dynamic.setSpawnRate(0);
        const aged = render(dynamic, 'audioflame.js', 9, 7,
            mkFrame({ frameSequence: 2, low: 0.9, timing: { deltaSeconds: 0.45 } }), 'C20 aged');
        assert(centroidRow(aged, 9, 7, 0.04) < centroidRow(born, 9, 7, 0.04),
            'C20/flame/rise: particles did not rise with age');
        assert(maxValue(aged) < maxValue(born),
            'C20/flame/fade: particles did not fade with age');
        const svBorn = topPixelSV(born);
        const svAged = topPixelSV(aged);
        assert(svAged.s < svBorn.s,
            'C20/flame/age-saturation: saturation should decrease with particle age');

        const blur0 = loadScript('audioflame.js', 0xc20034);
        const blur1 = loadScript('audioflame.js', 0xc20034);
        for (const algo of [blur0, blur1]) {
            algo.setSpawnRate(40);
            algo.setIntensity(1.0);
            algo.setColorLow('#ff0000');
            algo.setColorMid('#000000');
            algo.setColorHigh('#000000');
        }
        blur0.setBlur(0);
        blur1.setBlur(1);
        const sharp = render(blur0, 'audioflame.js', 9, 5,
            mkFrame({ low: 0.8, timing: { deltaSeconds: 0.5 } }), 'C20 blur 0');
        const spread = render(blur1, 'audioflame.js', 9, 5,
            mkFrame({ low: 0.8, timing: { deltaSeconds: 0.5 } }), 'C20 blur 1');
        assert(litCount(spread, 0.02) > litCount(sharp, 0.02),
            'C20/flame/blur-spreading: blur should spread energy across more pixels');

        const extinction = loadScript('audioflame.js', 0xc20035);
        extinction.setSpawnRate(60);
        extinction.setIntensity(1.0);
        extinction.setBlur(0);
        extinction.setColorLow('#ff6600');
        extinction.setColorMid('#000000');
        extinction.setColorHigh('#000000');
        render(extinction, 'audioflame.js', 9, 5,
            mkFrame({ low: 1.0, timing: { deltaSeconds: 0.5 } }), 'C20 extinction seed');
        extinction.setSpawnRate(0);
        const cleared = render(extinction, 'audioflame.js', 9, 5,
            mkFrame({ frameSequence: 2, low: 1.0, timing: { deltaSeconds: 4.0 } }), 'C20 extinction clear');
        assert(maxValue(cleared) < 0.01,
            'C20/flame/zero-spawn-extinction: particles should die out when spawn=0');

        const oneByN = loadScript('audioflame.js', 0xc20036);
        oneByN.setSpawnRate(45);
        oneByN.setIntensity(1);
        oneByN.setBlur(0.2);
        oneByN.setColorLow('#ff0000');
        oneByN.setColorMid('#00ff00');
        oneByN.setColorHigh('#0000ff');
        const strip = render(oneByN, 'audioflame.js', 1, 9,
            mkFrame({ low: 0.7, mid: 0.6, high: 0.5, timing: { deltaSeconds: 0.5 } }), 'C20 1xN');
        assert(maxValue(strip) > 0.01,
            'C20/flame/1xN: strip should remain active on 1xN geometry');

        const stall = loadScript('audioflame.js', 0xc20037);
        stall.setSpawnRate(60);
        stall.setIntensity(1.5);
        stall.setBlur(0.3);
        stall.setColorLow('#ff0000');
        stall.setColorMid('#00ff00');
        stall.setColorHigh('#0000ff');
        const stalled = render(stall, 'audioflame.js', 9, 5,
            mkFrame({ low: 1, mid: 1, high: 1, timing: { deltaSeconds: 90 } }), 'C20 longstall');
        assert(stalled.every(Number.isFinite) && maxValue(stalled) <= 1,
            'C20/flame/bounded-longstall: long stall must stay finite and clamped');
    }

    function caseLifeBlinkerTimingWrap() {
        const width = 7;
        const height = 7;
        const start = [[6, 3], [0, 3], [1, 3]];
        const random = randomForBoard(width, height, start, 0.9);
        const algo = loadLife(0xc22031, random);
        algo.colors = [{ h: 0, s: 1, v: 1 }];
        algo.setFreeRun('Yes');
        algo.setRate(2);
        algo.setSource('Lows');
        algo.setHealthCheck('None');
        algo.setHealthInterval(9);

        const map0 = render(algo, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 1, timing: { deltaSeconds: 0 } }), 'C22 blinker init');
        const b0 = lifeBoardFromMap(map0);
        const expected0 = new Array(width * height).fill(0);
        for (const [x, y] of start) expected0[lifeCellIndex(width, height, x, y)] = 1;
        assert(boardEquals(b0, expected0),
            'C22/life/blinker-seed: deterministic blinker board did not initialize as requested');

        const warm = lifeBoardFromMap(render(algo, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 2, low: 1, timing: { deltaSeconds: 0.5 } }), 'C22 blinker warmup'));
        assert(boardEquals(warm, expected0),
            'C22/life/blinker-warmup: first half-second should precharge carry without stepping');

        const bHalf = lifeBoardFromMap(render(algo, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 3, low: 1, timing: { deltaSeconds: 0.5 } }), 'C22 blinker half'));
        const expectedHalf = lifeStepToroidal(expected0, width, height);
        assert(boardEquals(bHalf, expectedHalf),
            'C22/life/blinker-flip-at-0.5s: expected one toroidal B3/S23 step');

        const bOne = lifeBoardFromMap(render(algo, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 4, low: 1, timing: { deltaSeconds: 0.5 } }), 'C22 blinker one'));
        const expectedOne = lifeStepToroidal(expectedHalf, width, height);
        assert(boardEquals(bOne, expectedOne),
            'C22/life/blinker-return-at-1s: expected second toroidal B3/S23 step');
        assert(boardEquals(bOne, expected0),
            'C22/life/blinker-return-at-1s: blinker must return to original phase after two steps');
    }

    function caseLifeKickPatternsAndDuplicateGuard() {
        const width = 7;
        const height = 7;
        const empty = [];
        const anchors = [[1, 1], [4, 1], [1, 4], [4, 4], [2, 2], [5, 3]];
        const random = randomForBoardAndAnchors(width, height, empty, anchors);
        const algo = loadLife(0xc22032, random);
        algo.colors = [{ h: 0, s: 1, v: 1 }];
        algo.setFreeRun('No');
        algo.setSource('Lows');
        algo.setHealthCheck('None');

        let board = lifeBoardFromMap(render(algo, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 0, timing: { deltaSeconds: 0 } }), 'C22 kick init'));
        const blank = new Array(width * height).fill(0);
        assert(boardEquals(board, blank),
            'C22/life/kick-seed: deterministic empty board failed to initialize');

        for (let i = 0; i < 4; i++) {
            const after = lifeBoardFromMap(render(algo, 'audiogameoflife.js', width, height,
                mkFrame({ frameSequence: i + 2, low: 0, timing: { deltaSeconds: 0 }, events: { delta: { kick: 1, beat: 0 } } }),
                `C22 kick ${i + 1}`));
            board = lifeStamp(board, width, height, LIFE_PATTERNS[i], anchors[i][0], anchors[i][1]);
            assert(boardEquals(after, board),
                `C22/life/four-pattern-kicks-${i + 1}: kick did not stamp expected pattern`);
        }

        const duplicateFirst = lifeBoardFromMap(render(algo, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 6, low: 0, timing: { deltaSeconds: 0 }, events: { delta: { kick: 1, beat: 0 } } }),
            'C22 duplicate first'));
        const expectedFirst = lifeStamp(board, width, height, LIFE_PATTERNS[4 % LIFE_PATTERNS.length], anchors[4][0], anchors[4][1]);
        assert(boardEquals(duplicateFirst, expectedFirst),
            'C22/life/duplicate-zero-time-repeat-guard: first frameSequence event did not inject');

        const duplicateRepeat = lifeBoardFromMap(render(algo, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 6, low: 0, timing: { deltaSeconds: 0 }, events: { delta: { kick: 1, beat: 0 } } }),
            'C22 duplicate repeat'));
        assert(boardEquals(duplicateRepeat, duplicateFirst),
            'C22/life/duplicate-zero-time-repeat-guard: duplicate frameSequence re-injected event');

        const next = lifeBoardFromMap(render(algo, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 7, low: 0, timing: { deltaSeconds: 0 }, events: { delta: { kick: 1, beat: 0 } } }),
            'C22 duplicate next'));
        const expectedNext = lifeStamp(expectedFirst, width, height, LIFE_PATTERNS[5 % LIFE_PATTERNS.length], anchors[5][0], anchors[5][1]);
        assert(boardEquals(next, expectedNext),
            'C22/life/duplicate-zero-time-repeat-guard: new frameSequence did not consume new event');
    }

    function caseLifeHealthSourceDecayOccupancy() {
        const width = 7;
        const height = 7;

        const blockBoard = [[3, 3], [4, 3], [3, 4], [4, 4]];
        const stable = loadLife(0xc22033, randomForBoard(width, height, blockBoard, 0.9));
        stable.colors = [{ h: 0, s: 1, v: 1 }];
        stable.setFreeRun('Yes');
        stable.setRate(2);
        stable.setSource('Lows');
        stable.setHealthCheck('None');
        stable.setHealthInterval(1);
        const stable0 = lifeBoardFromMap(render(stable, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 1, timing: { deltaSeconds: 0 } }), 'C22 block init'));
        const stableHalf = lifeBoardFromMap(render(stable, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 2, low: 1, timing: { deltaSeconds: 0.5 } }), 'C22 block half'));
        const stableOne = lifeBoardFromMap(render(stable, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 3, low: 1, timing: { deltaSeconds: 0.5 } }), 'C22 block one'));
        assert(boardEquals(stable0, stableHalf) && boardEquals(stable0, stableOne),
            'C22/life/block-stable-health-none: stable block changed under Health None');

        const dead = loadLife(0xc22034, randomFromSequence([0.9, 0.1], 0.9));
        dead.colors = [{ h: 0, s: 1, v: 1 }];
        dead.setFreeRun('No');
        dead.setRate(2);
        dead.setSource('Lows');
        dead.setHealthCheck('Dead');
        dead.setHealthInterval(0.5);
        const dead0 = lifeBoardFromMap(render(dead, 'audiogameoflife.js', 1, 1,
            mkFrame({ frameSequence: 1, low: 0, timing: { deltaSeconds: 0 } }), 'C22 dead init'));
        const dead1 = lifeBoardFromMap(render(dead, 'audiogameoflife.js', 1, 1,
            mkFrame({ frameSequence: 2, low: 0, timing: { deltaSeconds: 0.5 } }), 'C22 dead interval'));
        assert(dead0[0] === 0 && dead1[0] === 1,
            'C22/life/health-dead: dead board should reseed at interval');

        const all = loadLife(0xc22035, randomFromSequence([0.1, 0.9], 0.9));
        all.colors = [{ h: 0, s: 1, v: 1 }];
        all.setFreeRun('No');
        all.setRate(2);
        all.setSource('Lows');
        all.setHealthCheck('All');
        all.setHealthInterval(0.5);
        const all0 = lifeBoardFromMap(render(all, 'audiogameoflife.js', 1, 1,
            mkFrame({ frameSequence: 1, low: 0, timing: { deltaSeconds: 0 } }), 'C22 all init'));
        const all1 = lifeBoardFromMap(render(all, 'audiogameoflife.js', 1, 1,
            mkFrame({ frameSequence: 2, low: 0, timing: { deltaSeconds: 0.5 } }), 'C22 all interval'));
        assert(all0[0] === 1 && all1[0] === 0,
            'C22/life/health-all: all-live board should reseed at interval');

        const initialBlinker = [[2, 3], [3, 3], [4, 3]];
        const reseedSingle = [[0, 0]];
        const osc = loadLife(0xc22036, randomForBoardAndAnchors(width, height, initialBlinker, [], reseedSingle));
        osc.colors = [{ h: 0, s: 1, v: 1 }];
        osc.setFreeRun('Yes');
        osc.setRate(2);
        osc.setSource('Lows');
        osc.setHealthCheck('Oscillating');
        osc.setHealthInterval(1.5);
        render(osc, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 1, timing: { deltaSeconds: 0 } }), 'C22 osc init');
        render(osc, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 2, low: 1, timing: { deltaSeconds: 0.5 } }), 'C22 osc warmup');
        render(osc, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 3, low: 1, timing: { deltaSeconds: 0.5 } }), 'C22 osc half');
        const oscOne = lifeBoardFromMap(render(osc, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 4, low: 1, timing: { deltaSeconds: 0.5 } }), 'C22 osc one'));
        const expectedReseed = new Array(width * height).fill(0);
        expectedReseed[lifeCellIndex(width, height, 0, 0)] = 1;
        assert(boardEquals(oscOne, expectedReseed),
            'C22/life/health-oscillating: oscillating board should reseed at health interval');

        const none = loadLife(0xc22036, randomForBoardAndAnchors(width, height, initialBlinker, [], reseedSingle));
        none.colors = [{ h: 0, s: 1, v: 1 }];
        none.setFreeRun('Yes');
        none.setRate(2);
        none.setSource('Lows');
        none.setHealthCheck('None');
        none.setHealthInterval(1.5);
        render(none, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 1, timing: { deltaSeconds: 0 } }), 'C22 none init');
        render(none, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 2, low: 1, timing: { deltaSeconds: 0.5 } }), 'C22 none warmup');
        render(none, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 3, low: 1, timing: { deltaSeconds: 0.5 } }), 'C22 none half');
        const noneOne = lifeBoardFromMap(render(none, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 4, low: 1, timing: { deltaSeconds: 0.5 } }), 'C22 none one'));
        const expectedBlinker = new Array(width * height).fill(0);
        for (const [x, y] of initialBlinker)
            expectedBlinker[lifeCellIndex(width, height, x, y)] = 1;
        assert(boardEquals(noneOne, expectedBlinker),
            'C22/life/health-none: oscillator should persist without reset');

        const sourceStart = [[1, 1], [2, 1], [3, 1], [3, 2], [2, 3]];
        const midDriven = loadLife(0xc22037, randomForBoard(width, height, sourceStart, 0.9));
        const lowDecoy = loadLife(0xc22037, randomForBoard(width, height, sourceStart, 0.9));
        for (const algo of [midDriven, lowDecoy]) {
            algo.colors = [{ h: 0, s: 1, v: 1 }];
            algo.setFreeRun('No');
            algo.setRate(4);
            algo.setSource('Mids');
            algo.setHealthCheck('None');
        }
        const mid0 = lifeBoardFromMap(render(midDriven, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 0, mid: 0, timing: { deltaSeconds: 0 } }), 'C22 source init mid'));
        const mid1 = lifeBoardFromMap(render(midDriven, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 2, low: 0, mid: 1, timing: { deltaSeconds: 0.5 } }), 'C22 source active mid'));
        const low0 = lifeBoardFromMap(render(lowDecoy, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 0, mid: 0, timing: { deltaSeconds: 0 } }), 'C22 source init low'));
        const low1 = lifeBoardFromMap(render(lowDecoy, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 2, low: 1, mid: 0, timing: { deltaSeconds: 0.5 } }), 'C22 source decoy low'));
        assert(!boardEquals(mid0, mid1),
            'C22/life/source-selection: selected mid source should advance simulation');
        assert(boardEquals(low0, low1),
            'C22/life/source-selection: unselected low source should not advance when source=Mids');

        const decaySeedBoard = [[2, 3], [3, 3], [4, 3]];
        const slowDecay = loadLife(0xc22038, randomForBoard(width, height, decaySeedBoard, 0.9));
        const fastDecay = loadLife(0xc22038, randomForBoard(width, height, decaySeedBoard, 0.9));
        for (const algo of [slowDecay, fastDecay]) {
            algo.colors = [{ h: 0, s: 1, v: 1 }];
            algo.setFreeRun('No');
            algo.setRate(4);
            algo.setSource('Lows');
            algo.setHealthCheck('None');
        }
        slowDecay.setImpulseDecay(0.01);
        fastDecay.setImpulseDecay(0.5);
        render(slowDecay, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 0, timing: { deltaSeconds: 0 } }), 'C22 decay init slow');
        render(fastDecay, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 0, timing: { deltaSeconds: 0 } }), 'C22 decay init fast');
        render(slowDecay, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 2, low: 1, timing: { deltaSeconds: 0.05 } }), 'C22 decay prime slow');
        render(fastDecay, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 2, low: 1, timing: { deltaSeconds: 0.05 } }), 'C22 decay prime fast');
        const slowOut = lifeBoardFromMap(render(slowDecay, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 3, low: 0, timing: { deltaSeconds: 0.21 } }), 'C22 decay drop slow'));
        const fastOut = lifeBoardFromMap(render(fastDecay, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 3, low: 0, timing: { deltaSeconds: 0.21 } }), 'C22 decay drop fast'));
        assert(!boardEquals(slowOut, fastOut),
            'C22/life/impulse-decay: decay values should affect simulation advance after source drop');

        const freeRunYes = loadLife(0xc22039, randomForBoard(width, height, sourceStart, 0.9));
        const freeRunNo = loadLife(0xc22039, randomForBoard(width, height, sourceStart, 0.9));
        for (const algo of [freeRunYes, freeRunNo]) {
            algo.colors = [{ h: 0, s: 1, v: 1 }];
            algo.setRate(20);
            algo.setSource('Lows');
            algo.setHealthCheck('None');
        }
        freeRunYes.setFreeRun('Yes');
        freeRunNo.setFreeRun('No');
        const yes0 = lifeBoardFromMap(render(freeRunYes, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 0, timing: { deltaSeconds: 0 } }), 'C22 freerun yes init'));
        const yes1 = lifeBoardFromMap(render(freeRunYes, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 2, low: 0, timing: { deltaSeconds: 5 } }), 'C22 freerun yes'));
        const no0 = lifeBoardFromMap(render(freeRunNo, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 0, timing: { deltaSeconds: 0 } }), 'C22 freerun no init'));
        const no1 = lifeBoardFromMap(render(freeRunNo, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 2, low: 0, timing: { deltaSeconds: 5 } }), 'C22 freerun no'));
        assert(!boardEquals(yes0, yes1) && boardEquals(no0, no1),
            'C22/life/free-run-0.01-branch: free-run yes/no behavior mismatch at zero source');

        const occColors = [
            { h: 0.00, s: 1, v: 1 },
            { h: 0.20, s: 1, v: 1 },
            { h: 0.40, s: 1, v: 1 },
            { h: 0.60, s: 1, v: 1 },
            { h: 0.80, s: 1, v: 1 }
        ];
        const occStart = [[3, 3], [4, 3], [5, 3], [5, 2], [4, 1]];
        const occAlgo = loadLife(0xc2203a, randomForBoard(width, height, occStart, 0.9));
        occAlgo.colors = occColors.slice();
        occAlgo.setFreeRun('Yes');
        occAlgo.setRate(2);
        occAlgo.setSource('Lows');
        occAlgo.setHealthCheck('None');
        occAlgo.setHealthInterval(9);
        const initialBoard = new Array(width * height).fill(0);
        for (const [x, y] of occStart)
            initialBoard[lifeCellIndex(width, height, x, y)] = 1;
        const simulateOccupancy = function() {
            let board = initialBoard.slice();
            let carry = 0;
            let impulse = 0;
            const recentBoards = [board.slice()];
            const frames = [
                { dt: 0, low: 1 },
                { dt: 0.5, low: 1 },
                { dt: 0.5, low: 1 },
                { dt: 0.5, low: 1 },
                { dt: 0.5, low: 1 },
                { dt: 0.5, low: 1 }
            ];
            for (let f = 0; f < frames.length; f++) {
                const target = frames[f].low;
                const alpha = target > impulse ? 0.99 : occAlgo.getImpulseDecay();
                impulse += (target - impulse) * alpha;
                carry += frames[f].dt * occAlgo.getRate() * Math.max(0.01, impulse);
                let steps = 0;
                while (carry >= 1 && steps < 256) {
                    board = lifeStepToroidal(board, width, height);
                    carry -= 1;
                    steps++;
                    recentBoards.push(board.slice());
                    if (recentBoards.length > 5) recentBoards.shift();
                }
                if (steps === 256 && carry >= 1) carry = carry % 1;
            }
            return { currentBoard: board, recentBoards };
        };
        render(occAlgo, 'audiogameoflife.js', width, height,
            mkFrame({ frameSequence: 1, low: 1, timing: { deltaSeconds: 0 } }), 'C22 occ init');
        let finalMap = null;
        for (let i = 0; i < 5; i++) {
            finalMap = render(occAlgo, 'audiogameoflife.js', width, height,
                mkFrame({ frameSequence: 2 + i, low: 1, timing: { deltaSeconds: 0.5 } }), `C22 occ step ${i + 1}`);
        }
        const occupancyOracle = simulateOccupancy();
        const recentBoards = occupancyOracle.recentBoards;
        const currentBoard = occupancyOracle.currentBoard;
        const occupancySet = new Set();
        for (let i = 0; i < width * height; i++) {
            let aliveCount = 0;
            for (const board of recentBoards) aliveCount += board[i];
            const occ = aliveCount / recentBoards.length;
            occupancySet.add(occ.toFixed(2));
            const expectedColor = util.gradientLedfxAt(occColors, occ);
            const expectedV = currentBoard[i] ? expectedColor.v : expectedColor.v * occ * 0.35;
            const h = finalMap[i * 3];
            const s = finalMap[i * 3 + 1];
            const v = finalMap[i * 3 + 2];
            assert(hueDelta(h, expectedColor.h) < 1e-5,
                `C22/life/five-board-occupancy-coloring hue @${i}: expected ${expectedColor.h}, got ${h}`);
            approx(s, expectedColor.s, 1e-5,
                `C22/life/five-board-occupancy-coloring sat @${i}`);
            approx(v, expectedV, 1e-5,
                `C22/life/five-board-occupancy-coloring val @${i}`);
        }
        assert(occupancySet.size >= 3,
            'C22/life/five-board-occupancy-coloring: expected multiple occupancy levels');
    }

    const cases = [
        ['C36/noise/3d-pixel-oracle+decay+unavailable', caseNoiseRow36],
        ['C54/smoke/fbm4-oracle+zoom+decay+unavailable', caseSmokeRow54],
        ['C20/flame/populations+bands+dynamics+bounds', caseFlameRow20],
        ['C22/life/blinker-flip-return-wrap', caseLifeBlinkerTimingWrap],
        ['C22/life/four-pattern-kicks+duplicate-guard', caseLifeKickPatternsAndDuplicateGuard],
        ['C22/life/health-source-decay-occupancy', caseLifeHealthSourceDecayOccupancy]
    ];

    for (const [id, fn] of cases) {
        fn();
        markCase(id);
        console.log('PASS ' + id);
    }
}

function assertWorkerDInputRows() {
    const approx = (a, b, eps) => Math.abs(a - b) <= eps;

    function hsvToRgb(h, s, v) {
        h = ((h % 1) + 1) % 1;
        s = Math.max(0, Math.min(1, s));
        v = Math.max(0, Math.min(1, v));
        const i = Math.floor(h * 6);
        const f = h * 6 - i;
        const p = v * (1 - s);
        const q = v * (1 - f * s);
        const t = v * (1 - (1 - f) * s);
        switch (i % 6) {
        case 0: return [v, t, p];
        case 1: return [q, v, p];
        case 2: return [p, v, t];
        case 3: return [p, q, v];
        case 4: return [t, p, v];
        default: return [v, p, q];
        }
    }

    function makeFrame(sourceId, profileId, overrides) {
        return audio(Object.assign({
            version: 6,
            dt: 0,
            timing: { deltaSeconds: 0 },
            sourceId: sourceId,
            profileId: profileId,
            sourceEpoch: 1,
            configRevision: 1,
            pitch: { valid: false, hz: 0, midi: 0, confidence: 0 },
            banks: { full: { count: 6, processed: [0, 0, 0, 0, 0, 0], novelty: [0, 0, 0, 0, 0, 0] } },
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
            powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } }
        }, overrides || {}));
    }

    function rgbAt(map, x, y, width) {
        const i = (y * width + x) * 3;
        return hsvToRgb(map[i], map[i + 1], map[i + 2]);
    }

    function assertUniformRgb(map, width, height, msg) {
        const base = rgbAt(map, 0, 0, width);
        for (let y = 0; y < height; y++) {
            for (let x = 0; x < width; x++) {
                const c = rgbAt(map, x, y, width);
                assert(
                    approx(c[0], base[0], 1e-6) &&
                    approx(c[1], base[1], 1e-6) &&
                    approx(c[2], base[2], 1e-6),
                    `${msg}: non-uniform at (${x},${y})`
                );
            }
        }
        return base;
    }

    function runCase(caseId, fn) {
        try {
            fn();
        markCase(caseId);
            console.log(`PASS ${caseId}`);
            return { caseId: caseId, ok: true };
        } catch (err) {
            console.log(`FAIL ${caseId} :: ${err.message}`);
            return { caseId: caseId, ok: false, error: err.message };
        }
    }

    function caseRow18Filter() {
        const algo = loadScript('audiofilter.js', 0x44d1);
        algo.setUseGradient('No');
        algo.setColor('#ff0000');
        algo.setFrequencyRange('Beat');
        algo.setRollSpeed(0);
        algo.setBoost(0);

        const width = 7;
        const low = render(
            algo,
            'audiofilter.js',
            width,
            1,
            makeFrame('filter-src', 18, { beat: 0.5, low: 1 }),
            'row18 static boost0'
        );
        const lowRgb = assertUniformRgb(low, width, 1, 'row18 boost0');
        assert(approx(lowRgb[0], 0.5, 1e-6) && approx(lowRgb[1], 0, 1e-6) && approx(lowRgb[2], 0, 1e-6),
            `row18 expected red 0.5, got ${JSON.stringify(lowRgb)}`);

        algo.setBoost(1);
        const high = render(
            algo,
            'audiofilter.js',
            width,
            1,
            makeFrame('filter-src', 18, { beat: 0.5, low: 1 }),
            'row18 static boost1'
        );
        const highRgb = assertUniformRgb(high, width, 1, 'row18 boost1');
        assert(approx(highRgb[0], 0.9375, 1e-6) && approx(highRgb[1], 0, 1e-6) && approx(highRgb[2], 0, 1e-6),
            `row18 expected red 0.9375, got ${JSON.stringify(highRgb)}`);

        algo.setBoost(0);
        const silentSelected = render(
            algo,
            'audiofilter.js',
            width,
            1,
            makeFrame('filter-src', 18, { beat: 0, low: 1, mid: 1, high: 1 }),
            'row18 selected silent'
        );
        const selectedRgb = rgbAt(silentSelected, 0, 0, width);
        assert(approx(selectedRgb[0], 0, 1e-6) && approx(selectedRgb[1], 0, 1e-6) && approx(selectedRgb[2], 0, 1e-6),
            `row18 selected silent expected black, got ${JSON.stringify(selectedRgb)}`);

        algo.setUseGradient('Yes');
        algo.colors = [
            { h: 0.00, s: 1, v: 1 },
            { h: 0.33, s: 1, v: 1 },
            { h: 0.66, s: 1, v: 1 }
        ];
        algo.setRollSpeed(0.5);
        const period = 1 + (1 - 0.5) * 59;

        const g0 = render(
            algo,
            'audiofilter.js',
            width,
            1,
            makeFrame('filter-src', 18, { beat: 1, dt: 0, timing: { deltaSeconds: 0 } }),
            'row18 grad t0'
        );
        const gHalf = render(
            algo,
            'audiofilter.js',
            width,
            1,
            makeFrame('filter-src', 18, { beat: 1, dt: period / 2, timing: { deltaSeconds: period / 2 } }),
            'row18 grad thalf'
        );
        const gFull = render(
            algo,
            'audiofilter.js',
            width,
            1,
            makeFrame('filter-src', 18, { beat: 1, dt: period / 2, timing: { deltaSeconds: period / 2 } }),
            'row18 grad tperiod'
        );
        assertUniformRgb(g0, width, 1, 'row18 grad t0 uniform');
        assertUniformRgb(gHalf, width, 1, 'row18 grad half uniform');
        assertUniformRgb(gFull, width, 1, 'row18 grad full uniform');
        assert.deepStrictEqual(g0, gFull, 'row18 gradient period mismatch under deltaSeconds accumulation');
        assert.notDeepStrictEqual(g0, gHalf, 'row18 gradient roll did not move with deltaSeconds');
    }

    function caseRow26Hierarchy() {
        const algo = loadScript('audiohierarchy.js', 0x44d2);
        assert.strictEqual(algo.acceptColors, 0, 'row26 hierarchy should not expose palette picker');
        algo.setColorLows('#ff0000');
        algo.setColorMids('#00ff00');
        algo.setColorHigh('#0000ff');
        algo.setThresholdLows(0.05);
        algo.setThresholdMids(0.05);
        algo.setSwitchTime(0.1);
        algo.setBrightnessBoost(0);
        const width = 4;
        const height = 2;

        const f0 = render(algo, 'audiohierarchy.js', width, height,
            makeFrame('hier-src', 26, { beat: 0.8, bass: 0.8, mid: 0, high: 0, timing: { deltaSeconds: 0 } }),
            'row26 t0');
        const c0 = assertUniformRgb(f0, width, height, 'row26 t0');
        assert(approx(c0[0], 0.8, 1e-6) && approx(c0[1], 0, 1e-6) && approx(c0[2], 0, 1e-6),
            `row26 t0 expected red .8, got ${JSON.stringify(c0)}`);

        const f1 = render(algo, 'audiohierarchy.js', width, height,
            makeFrame('hier-src', 26, { beat: 0.02, bass: 0.02, mid: 0.7, high: 0, timing: { deltaSeconds: 0.05 } }),
            'row26 t0.05');
        const c1 = assertUniformRgb(f1, width, height, 'row26 t0.05');
        assert(approx(c1[0], 0.02, 1e-6) && approx(c1[1], 0, 1e-6) && approx(c1[2], 0, 1e-6),
            `row26 t0.05 expected retained red .02, got ${JSON.stringify(c1)}`);

        const f2 = render(algo, 'audiohierarchy.js', width, height,
            makeFrame('hier-src', 26, { beat: 0.02, bass: 0.02, mid: 0.7, high: 0, timing: { deltaSeconds: 0.06 } }),
            'row26 t0.11');
        const c2 = assertUniformRgb(f2, width, height, 'row26 t0.11');
        assert(approx(c2[0], 0, 1e-6) && approx(c2[1], 0.7, 1e-6) && approx(c2[2], 0, 1e-6),
            `row26 t0.11 expected green .7, got ${JSON.stringify(c2)}`);

        const f3 = render(algo, 'audiohierarchy.js', width, height,
            makeFrame('hier-src', 26, { beat: 0.01, bass: 0.01, mid: 0.01, high: 0.9, timing: { deltaSeconds: 0.05 } }),
            'row26 t0.16');
        const c3 = assertUniformRgb(f3, width, height, 'row26 t0.16');
        assert(approx(c3[0], 0, 1e-6) && approx(c3[1], 0.01, 1e-6) && approx(c3[2], 0, 1e-6),
            `row26 t0.16 expected retained green .01, got ${JSON.stringify(c3)}`);

        const f4 = render(algo, 'audiohierarchy.js', width, height,
            makeFrame('hier-src', 26, { beat: 0.01, bass: 0.01, mid: 0.01, high: 0.9, timing: { deltaSeconds: 0.06 } }),
            'row26 t0.22');
        const c4 = assertUniformRgb(f4, width, height, 'row26 t0.22');
        assert(approx(c4[0], 0, 1e-6) && approx(c4[1], 0, 1e-6) && approx(c4[2], 0.9, 1e-6),
            `row26 t0.22 expected blue .9, got ${JSON.stringify(c4)}`);

        function configuredHierarchy(seed) {
            const a = loadScript('audiohierarchy.js', seed);
            a.setColorLows('#ff0000');
            a.setColorMids('#00ff00');
            a.setColorHigh('#0000ff');
            a.setThresholdLows(0.05);
            a.setThresholdMids(0.05);
            a.setSwitchTime(0.1);
            a.setBrightnessBoost(0);
            return a;
        }

        const switched = configuredHierarchy(0x44e1);
        render(switched, 'audiohierarchy.js', width, height,
            makeFrame('hier-old', 26, { beat: 0.8, bass: 0.8, mid: 0, high: 0, timing: { deltaSeconds: 0.02 } }),
            'row26 reset prime');
        const switchedMap = render(switched, 'audiohierarchy.js', width, height,
            makeFrame('hier-new', 26, { beat: 0.02, bass: 0.02, mid: 0.7, high: 0, timing: { deltaSeconds: 0.12 } }),
            'row26 reset source switch');
        const freshSwitched = configuredHierarchy(0x44e2);
        const freshMap = render(freshSwitched, 'audiohierarchy.js', width, height,
            makeFrame('hier-new', 26, { beat: 0.02, bass: 0.02, mid: 0.7, high: 0, timing: { deltaSeconds: 0.12 } }),
            'row26 reset fresh');
        assert.deepStrictEqual(switchedMap, freshMap, 'row26 source/profile/epoch change must reset hierarchy state');

        const unavailableA = configuredHierarchy(0x44e3);
        render(unavailableA, 'audiohierarchy.js', width, height,
            makeFrame('hier-keep', 26, { beat: 0.01, bass: 0.01, mid: 0.7, high: 0, timing: { deltaSeconds: 0.12 } }),
            'row26 unavailable prime');
        const unavailableMap = render(unavailableA, 'audiohierarchy.js', width, height,
            makeFrame('', 26, {
                available: false,
                status: 'reset',
                sourceEpoch: 1,
                beat: 0.01,
                bass: 0.01,
                mid: 0.01,
                high: 0.9,
                timing: { deltaSeconds: 0.05 }
            }),
            'row26 unavailable frame');
        const unavailableB = configuredHierarchy(0x44e4);
        render(unavailableB, 'audiohierarchy.js', width, height,
            makeFrame('hier-keep', 26, { beat: 0.01, bass: 0.01, mid: 0.7, high: 0, timing: { deltaSeconds: 0.12 } }),
            'row26 unavailable prime ref');
        const unavailableRef = render(unavailableB, 'audiohierarchy.js', width, height,
            makeFrame('hier-keep', 26, { beat: 0.01, bass: 0.01, mid: 0.01, high: 0.9, timing: { deltaSeconds: 0.05 } }),
            'row26 unavailable ref');
        assert.deepStrictEqual(unavailableMap, unavailableRef, 'row26 empty source frame should not wipe timers/selection');

        const geometryA = configuredHierarchy(0x44ef);
        render(geometryA, 'audiohierarchy.js', width, height,
            makeFrame('hier-geo', 26, { beat: 0.8, bass: 0.8, mid: 0, high: 0, timing: { deltaSeconds: 0.02 } }),
            'row26 geometry prime');
        const geometryMap = render(geometryA, 'audiohierarchy.js', 6, 1,
            makeFrame('hier-geo', 26, { beat: 0.02, bass: 0.02, mid: 0.7, high: 0, timing: { deltaSeconds: 0.12 } }),
            'row26 geometry switch');
        const geometryFresh = configuredHierarchy(0x44f0);
        const geometryRef = render(geometryFresh, 'audiohierarchy.js', 6, 1,
            makeFrame('hier-geo', 26, { beat: 0.02, bass: 0.02, mid: 0.7, high: 0, timing: { deltaSeconds: 0.12 } }),
            'row26 geometry fresh');
        assert.deepStrictEqual(geometryMap, geometryRef, 'row26 geometry change must reset hierarchy state');
    }

    function caseRow60Vumeter() {
        const rawForMeter = meter => Math.pow(10, ((meter - 1) * 100) / 20);
        const algo = loadScript('audiovumeter.js', 0x44d3);
        assert.strictEqual(algo.acceptColors, 0, 'row60 vumeter should not expose palette picker');
        algo.setColorMin('#0000ff');
        algo.setColorMid('#00ff00');
        algo.setColorMax('#ff0000');
        algo.setColorPeak('#ffffff');
        algo.setMinVolume(0.2);
        algo.setMaxVolume(0.8);
        algo.setPeakPercent(2);
        algo.setPeakDecay(0.1);
        const frame = meter => makeFrame('vu-src', 60, {
            dt: 0.04,
            timing: { deltaSeconds: 0.04 },
            volume: { rawRms: rawForMeter(meter), normalized: 0.01 }
        });

        const m70 = render(algo, 'audiovumeter.js', 100, 1, frame(0.7), 'row60 m70');
        const m90 = render(algo, 'audiovumeter.js', 100, 1, frame(0.9), 'row60 m90');
        const m40 = render(algo, 'audiovumeter.js', 100, 1, frame(0.4), 'row60 m40');
        const c10 = rgbAt(m70, 10, 0, 100);
        const c30 = rgbAt(m70, 30, 0, 100);
        const c85 = rgbAt(m90, 85, 0, 100);
        assert(c10[2] > 0.95 && c10[0] < 1e-6 && c10[1] < 1e-6, `row60 min zone expected blue, got ${JSON.stringify(c10)}`);
        assert(c30[1] > 0.95 && c30[0] < 1e-6 && c30[2] < 1e-6, `row60 mid zone expected green, got ${JSON.stringify(c30)}`);
        assert(c85[0] > 0.95 && c85[1] < 1e-6 && c85[2] < 1e-6, `row60 max zone expected red, got ${JSON.stringify(c85)}`);

        const markerXs = [];
        for (let x = 0; x < 100; x++) {
            const c = rgbAt(m40, x, 0, 100);
            if (c[0] > 0.95 && c[1] > 0.95 && c[2] > 0.95) markerXs.push(x);
        }
        assert.deepStrictEqual(markerXs, [38, 39, 84, 85], `row60 exact markers mismatch: ${markerXs}`);

        const fullScale = render(
            algo,
            'audiovumeter.js',
            20,
            1,
            makeFrame('vu-src', 60, { dt: 0.04, timing: { deltaSeconds: 0.04 }, volume: { rawRms: 1 } }),
            'row60 rms1 full scale'
        );
        assert(rgbAt(fullScale, 19, 0, 20)[0] > 0.95, 'row60 rms=1 should hit max-volume red zone');

        const zero = render(
            algo,
            'audiovumeter.js',
            20,
            1,
            makeFrame('vu-src', 60, { dt: 0.04, timing: { deltaSeconds: 0.04 }, volume: { rawRms: 0 } }),
            'row60 rms0 zero'
        );
        for (let x = 0; x < 20; x++) {
            const c = rgbAt(zero, x, 0, 20);
            assert(approx(c[0], 0, 1e-6) && approx(c[1], 0, 1e-6) && approx(c[2], 0, 1e-6),
                `row60 rms=0 expected black at ${x}, got ${JSON.stringify(c)}`);
        }

        function configuredVu(seed) {
            const a = loadScript('audiovumeter.js', seed);
            a.setColorMin('#0000ff');
            a.setColorMid('#00ff00');
            a.setColorMax('#ff0000');
            a.setColorPeak('#ffffff');
            a.setMinVolume(0.2);
            a.setMaxVolume(0.8);
            a.setPeakPercent(2);
            a.setPeakDecay(0.1);
            return a;
        }

        const switched = configuredVu(0x44e5);
        render(switched, 'audiovumeter.js', 100, 1, frame(0.7), 'row60 reset prime');
        render(switched, 'audiovumeter.js', 100, 1, frame(0.9), 'row60 reset prime2');
        const switchedMap = render(switched, 'audiovumeter.js', 100, 1,
            makeFrame('vu-new', 60, { dt: 0.04, timing: { deltaSeconds: 0.04 }, volume: { rawRms: rawForMeter(0.4), normalized: 0.01 } }),
            'row60 reset source switch');
        const freshVu = configuredVu(0x44e6);
        const freshMap = render(freshVu, 'audiovumeter.js', 100, 1,
            makeFrame('vu-new', 60, { dt: 0.04, timing: { deltaSeconds: 0.04 }, volume: { rawRms: rawForMeter(0.4), normalized: 0.01 } }),
            'row60 reset fresh');
        assert.deepStrictEqual(switchedMap, freshMap, 'row60 source/profile/epoch change must reset peak/trough state');

        const unavailableA = configuredVu(0x44e7);
        render(unavailableA, 'audiovumeter.js', 100, 1, frame(0.7), 'row60 unavailable prime');
        render(unavailableA, 'audiovumeter.js', 100, 1, frame(0.9), 'row60 unavailable prime2');
        const unavailableMap = render(unavailableA, 'audiovumeter.js', 100, 1,
            makeFrame('', 60, {
                available: false,
                status: 'reset',
                sourceEpoch: 1,
                dt: 0.04,
                timing: { deltaSeconds: 0.04 },
                volume: { rawRms: rawForMeter(0.4), normalized: 0.01 }
            }),
            'row60 unavailable frame');
        const unavailableB = configuredVu(0x44e8);
        render(unavailableB, 'audiovumeter.js', 100, 1, frame(0.7), 'row60 unavailable ref prime');
        render(unavailableB, 'audiovumeter.js', 100, 1, frame(0.9), 'row60 unavailable ref prime2');
        const unavailableRef = render(unavailableB, 'audiovumeter.js', 100, 1, frame(0.4), 'row60 unavailable ref');
        assert.deepStrictEqual(unavailableMap, unavailableRef, 'row60 empty source frame should not wipe peak/trough state');

        const geometryA = configuredVu(0x44f1);
        render(geometryA, 'audiovumeter.js', 100, 1, frame(0.7), 'row60 geometry prime');
        render(geometryA, 'audiovumeter.js', 100, 1, frame(0.9), 'row60 geometry prime2');
        const geometryMap = render(geometryA, 'audiovumeter.js', 40, 1,
            makeFrame('vu-src', 60, { dt: 0.04, timing: { deltaSeconds: 0.04 }, volume: { rawRms: rawForMeter(0.4), normalized: 0.01 } }),
            'row60 geometry switch');
        const geometryFresh = configuredVu(0x44f2);
        const geometryRef = render(geometryFresh, 'audiovumeter.js', 40, 1,
            makeFrame('vu-src', 60, { dt: 0.04, timing: { deltaSeconds: 0.04 }, volume: { rawRms: rawForMeter(0.4), normalized: 0.01 } }),
            'row60 geometry fresh');
        assert.deepStrictEqual(geometryMap, geometryRef, 'row60 geometry change must reset peak/trough state');
    }

    function caseRow38PitchSpectrum() {
        const palette = [
            { h: 0.00, s: 1, v: 1 },
            { h: 0.33, s: 1, v: 1 },
            { h: 0.66, s: 1, v: 1 }
        ];

        function makePitchAlgo(seed) {
            const a = loadScript('audiopitchspectrum.js', seed);
            a.setFadeRate(0);
            a.setResponsiveness(1);
            a.setMirror('No');
            a.setBlur(0);
            a.colors = palette;
            return a;
        }

        const frame = (processed, novelty, pitchFields, decoy) => makeFrame('pitch-src', 38, Object.assign({
            dt: 0.04,
            timing: { deltaSeconds: 0.04 },
            pitch: pitchFields,
            banks: { full: { count: processed.length, processed: processed, novelty: novelty } }
        }, decoy || {}));

        const stripeAlgo = makePitchAlgo(0x44d4);
        const flatAlgo = makePitchAlgo(0x44d4);
        const striped = render(
            stripeAlgo,
            'audiopitchspectrum.js',
            6,
            1,
            frame(
                [1, 0, 1, 0, 1, 0],
                [0, 1, 0, 1, 0, 1],
                { valid: true, hz: 440, midi: 69, confidence: 0.8 }
            ),
            'row38 striped'
        );
        const flatSameMean = render(
            flatAlgo,
            'audiopitchspectrum.js',
            6,
            1,
            frame(
                [0.5, 0.5, 0.5, 0.5, 0.5, 0.5],
                [1, 0, 1, 0, 1, 0],
                { valid: true, hz: 440, midi: 69, confidence: 0.8 }
            ),
            'row38 flat same mean'
        );
        assert.notDeepStrictEqual(striped, flatSameMean, 'row38 each-bin processed blend collapsed to mean-like output');
        const c0 = rgbAt(striped, 0, 0, 6);
        const c1 = rgbAt(striped, 1, 0, 6);
        assert((c0[0] + c0[1] + c0[2]) > 0.2 && (c1[0] + c1[1] + c1[2]) < 1e-6,
            `row38 processed-bin gating mismatch: ${JSON.stringify(c0)} vs ${JSON.stringify(c1)}`);

        const lowAlgo = makePitchAlgo(0x44d5);
        const highAlgo = makePitchAlgo(0x44d5);
        const lowPitch = render(
            lowAlgo,
            'audiopitchspectrum.js',
            6,
            1,
            frame(
                [0.7, 0.7, 0.7, 0.7, 0.7, 0.7],
                [0, 0, 0, 0, 0, 0],
                { valid: true, hz: 220, midi: 57, confidence: 0.8 }
            ),
            'row38 pitch220'
        );
        const highPitch = render(
            highAlgo,
            'audiopitchspectrum.js',
            6,
            1,
            frame(
                [0.7, 0.7, 0.7, 0.7, 0.7, 0.7],
                [0, 0, 0, 0, 0, 0],
                { valid: true, hz: 880, midi: 81, confidence: 0.8 }
            ),
            'row38 pitch880'
        );
        assert.notDeepStrictEqual(lowPitch, highPitch, 'row38 220Hz and 880Hz produced identical reference colors');

        const decoyA = makePitchAlgo(0x44d6);
        const decoyB = makePitchAlgo(0x44d6);
        const baseline = render(
            decoyA,
            'audiopitchspectrum.js',
            6,
            1,
            frame(
                [0.4, 0.5, 0.6, 0.7, 0.5, 0.4],
                [0.1, 0.2, 0.3, 0.4, 0.2, 0.1],
                { valid: true, hz: 440, midi: 69, confidence: 0.8 },
                { low: 1, mid: 0, high: 1, beat: 1, bass: 1, onset: true, events: { delta: { onset: 1, beat: 1, kick: 1, bar: 1 } } }
            ),
            'row38 decoy baseline'
        );
        const decoyChanged = render(
            decoyB,
            'audiopitchspectrum.js',
            6,
            1,
            frame(
                [0.4, 0.5, 0.6, 0.7, 0.5, 0.4],
                [0.1, 0.2, 0.3, 0.4, 0.2, 0.1],
                { valid: true, hz: 440, midi: 69, confidence: 0.8 },
                { low: 0, mid: 1, high: 0, beat: 0, bass: 0, onset: false, events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } } }
            ),
            'row38 decoy changed'
        );
        assert.deepStrictEqual(baseline, decoyChanged, 'row38 scalar decoy fields changed pitch-spectrum output');

        const holdA = makePitchAlgo(0x44d7);
        holdA.setFadeRate(0.25);
        const prime = render(
            holdA,
            'audiopitchspectrum.js',
            4,
            1,
            frame(
                [1, 1, 1, 1],
                [0, 0, 0, 0],
                { valid: true, hz: 440, midi: 69, confidence: 0.8 }
            ),
            'row38 prime'
        );
        const invalid1 = render(
            holdA,
            'audiopitchspectrum.js',
            4,
            1,
            frame(
                [0, 0, 0, 0],
                [0, 0, 0, 0],
                { valid: false, hz: 880, midi: 108, confidence: 0 }
            ),
            'row38 invalid hold 1'
        );
        const holdB = makePitchAlgo(0x44d7);
        holdB.setFadeRate(0.25);
        render(
            holdB,
            'audiopitchspectrum.js',
            4,
            1,
            frame(
                [1, 1, 1, 1],
                [0, 0, 0, 0],
                { valid: true, hz: 440, midi: 69, confidence: 0.8 }
            ),
            'row38 prime b'
        );
        const invalid2 = render(
            holdB,
            'audiopitchspectrum.js',
            4,
            1,
            frame(
                [0, 0, 0, 0],
                [0, 0, 0, 0],
                { valid: false, hz: 55, midi: 21, confidence: 0 }
            ),
            'row38 invalid hold 2'
        );
        assert.deepStrictEqual(invalid1, invalid2, 'row38 invalid pitch fields replaced color instead of holding and fading history');
        let faded = false;
        for (let x = 0; x < 4; x++) {
            const before = rgbAt(prime, x, 0, 4);
            const after = rgbAt(invalid1, x, 0, 4);
            assert(after[0] <= before[0] + 1e-6, `row38 invalid frame unexpectedly increased red at ${x}`);
            assert(after[1] <= before[1] + 1e-6, `row38 invalid frame unexpectedly increased green at ${x}`);
            assert(after[2] <= before[2] + 1e-6, `row38 invalid frame unexpectedly increased blue at ${x}`);
            if (after[0] + after[1] + after[2] < before[0] + before[1] + before[2] - 1e-6) faded = true;
        }
        assert(faded, 'row38 invalid pitch should keep prior note color and fade it');
    }

    function caseRow10Concentric() {
        const width = 9;
        const height = 9;
        const palette = [
            { h: 0.00, s: 1, v: 0.2 },
            { h: 0.33, s: 1, v: 1.0 },
            { h: 0.66, s: 1, v: 0.2 }
        ];

        const still = loadScript('audioconcentric.js', 0x44d8);
        still.setPowerMultiplier(0);
        still.setIdleSpeed(0);
        still.setCenterSmoothing(0);
        still.setStretchHeight(1);
        still.colors = palette;
        const base = render(still, 'audioconcentric.js', width, height,
            makeFrame('con-src', 10, { timing: { deltaSeconds: 0 }, low: 0, mid: 0, high: 0 }),
            'row10 base');
        const eqA = rgbAt(base, 0, 4, width);
        const eqB = rgbAt(base, 4, 0, width);
        assert(
            approx(eqA[0], eqB[0], 1e-6) &&
            approx(eqA[1], eqB[1], 1e-6) &&
            approx(eqA[2], eqB[2], 1e-6),
            `row10 equal-distance mismatch without stretch: ${JSON.stringify(eqA)} vs ${JSON.stringify(eqB)}`
        );

        const stretched = loadScript('audioconcentric.js', 0x44d8);
        stretched.setPowerMultiplier(0);
        stretched.setIdleSpeed(0);
        stretched.setCenterSmoothing(0);
        stretched.setStretchHeight(2.5);
        stretched.colors = palette;
        const stretchedMap = render(stretched, 'audioconcentric.js', width, height,
            makeFrame('con-src', 10, { timing: { deltaSeconds: 0 }, low: 0, mid: 0, high: 0 }),
            'row10 stretched');
        const stA = rgbAt(stretchedMap, 0, 4, width);
        const stB = rgbAt(stretchedMap, 4, 0, width);
        assert(
            !(approx(stA[0], stB[0], 1e-6) && approx(stA[1], stB[1], 1e-6) && approx(stA[2], stB[2], 1e-6)),
            `row10 stretch should break equal-distance symmetry: ${JSON.stringify(stA)} vs ${JSON.stringify(stB)}`
        );

        const idle = loadScript('audioconcentric.js', 0x44d9);
        idle.setPowerMultiplier(0);
        idle.setIdleSpeed(1);
        const i0 = render(idle, 'audioconcentric.js', width, height,
            makeFrame('con-src', 10, { timing: { deltaSeconds: 0 }, low: 0, mid: 0, high: 0 }),
            'row10 idle start');
        const i1 = render(idle, 'audioconcentric.js', width, height,
            makeFrame('con-src', 10, { timing: { deltaSeconds: 0.5 }, dt: 0.5, low: 0, mid: 0, high: 0 }),
            'row10 idle move');
        assert.notDeepStrictEqual(i0, i1, 'row10 silence should still move with idle speed');

        const low = loadScript('audioconcentric.js', 0x44da);
        const high = loadScript('audioconcentric.js', 0x44da);
        low.setFrequencyRange('Lows (beat+bass)');
        high.setFrequencyRange('Lows (beat+bass)');
        low.setPowerMultiplier(1);
        high.setPowerMultiplier(1);
        low.setIdleSpeed(0);
        high.setIdleSpeed(0);
        low.colors = palette;
        high.colors = palette;
        const lowShift = render(low, 'audioconcentric.js', width, height,
            makeFrame('con-src', 10, { timing: { deltaSeconds: 0.2 }, dt: 0.2, beat: 0, bass: 0.1, low: 0.1 }),
            'row10 low power');
        const highShift = render(high, 'audioconcentric.js', width, height,
            makeFrame('con-src', 10, { timing: { deltaSeconds: 0.2 }, dt: 0.2, beat: 0, bass: 0.9, low: 0.9 }),
            'row10 high power');
        assert.notDeepStrictEqual(lowShift, highShift, 'row10 power should alter field travel');
        const lowV = [];
        const highV = [];
        for (let i = 2; i < lowShift.length; i += 3) {
            lowV.push(lowShift[i]);
            highV.push(highShift[i]);
        }
        let ratioMin = Infinity;
        let ratioMax = -Infinity;
        for (let i = 0; i < lowV.length; i++) {
            if (lowV[i] > 1e-6 && highV[i] > 1e-6) {
                const ratio = highV[i] / lowV[i];
                ratioMin = Math.min(ratioMin, ratio);
                ratioMax = Math.max(ratioMax, ratio);
            }
        }
        assert(ratioMax - ratioMin > 0.1, `row10 output changed like uniform V scaling instead of field travel (ratio spread ${ratioMax - ratioMin})`);

        const invNo = loadScript('audioconcentric.js', 0x44db);
        const invYes = loadScript('audioconcentric.js', 0x44db);
        invNo.setInvert('No');
        invYes.setInvert('Yes');
        invNo.setIdleSpeed(0.4);
        invYes.setIdleSpeed(0.4);
        const noMap = render(invNo, 'audioconcentric.js', width, height,
            makeFrame('con-src', 10, { timing: { deltaSeconds: 0.25 }, dt: 0.25, beat: 0, bass: 0.3, low: 0.3 }),
            'row10 invert no');
        const yesMap = render(invYes, 'audioconcentric.js', width, height,
            makeFrame('con-src', 10, { timing: { deltaSeconds: 0.25 }, dt: 0.25, beat: 0, bass: 0.3, low: 0.3 }),
            'row10 invert yes');
        assert.notDeepStrictEqual(noMap, yesMap, 'row10 invert should reverse propagation direction');

        function configuredConcentric(seed) {
            const a = loadScript('audioconcentric.js', seed);
            a.setFrequencyRange('Lows (beat+bass)');
            a.setPowerMultiplier(1);
            a.setIdleSpeed(0.2);
            a.setCenterSmoothing(0);
            a.setStretchHeight(1);
            a.colors = palette;
            return a;
        }

        const switched = configuredConcentric(0x44e9);
        render(switched, 'audioconcentric.js', width, height,
            makeFrame('con-old', 10, { timing: { deltaSeconds: 0.3 }, dt: 0.3, beat: 0, bass: 0.8, low: 0.8 }),
            'row10 reset prime');
        const switchedMap = render(switched, 'audioconcentric.js', width, height,
            makeFrame('con-new', 10, { timing: { deltaSeconds: 0 }, dt: 0, beat: 0, bass: 0.8, low: 0.8 }),
            'row10 reset switch');
        const freshCon = configuredConcentric(0x44ea);
        const freshConMap = render(freshCon, 'audioconcentric.js', width, height,
            makeFrame('con-new', 10, { timing: { deltaSeconds: 0 }, dt: 0, beat: 0, bass: 0.8, low: 0.8 }),
            'row10 reset fresh');
        assert.deepStrictEqual(switchedMap, freshConMap, 'row10 source/profile/epoch change must reset offset');

        const unavailableA = configuredConcentric(0x44eb);
        render(unavailableA, 'audioconcentric.js', width, height,
            makeFrame('con-keep', 10, { timing: { deltaSeconds: 0.3 }, dt: 0.3, beat: 0, bass: 0.8, low: 0.8 }),
            'row10 unavailable prime');
        const unavailableMap = render(unavailableA, 'audioconcentric.js', width, height,
            makeFrame('', 10, {
                available: false,
                status: 'reset',
                sourceEpoch: 1,
                timing: { deltaSeconds: 0.2 },
                dt: 0.2,
                beat: 0,
                bass: 0.6,
                low: 0.6
            }),
            'row10 unavailable frame');
        const unavailableB = configuredConcentric(0x44ec);
        render(unavailableB, 'audioconcentric.js', width, height,
            makeFrame('con-keep', 10, { timing: { deltaSeconds: 0.3 }, dt: 0.3, beat: 0, bass: 0.8, low: 0.8 }),
            'row10 unavailable ref prime');
        const unavailableRef = render(unavailableB, 'audioconcentric.js', width, height,
            makeFrame('con-keep', 10, { timing: { deltaSeconds: 0.2 }, dt: 0.2, beat: 0, bass: 0.6, low: 0.6 }),
            'row10 unavailable ref');
        assert.deepStrictEqual(unavailableMap, unavailableRef, 'row10 empty source frame should not wipe offset state');

        const geometryA = configuredConcentric(0x44f3);
        render(geometryA, 'audioconcentric.js', width, height,
            makeFrame('con-geo', 10, { timing: { deltaSeconds: 0.3 }, dt: 0.3, beat: 0, bass: 0.8, low: 0.8 }),
            'row10 geometry prime');
        const geometryMap = render(geometryA, 'audioconcentric.js', 7, 5,
            makeFrame('con-geo', 10, { timing: { deltaSeconds: 0 }, dt: 0, beat: 0, bass: 0.8, low: 0.8 }),
            'row10 geometry switch');
        const geometryB = configuredConcentric(0x44f4);
        const geometryRef = render(geometryB, 'audioconcentric.js', 7, 5,
            makeFrame('con-geo', 10, { timing: { deltaSeconds: 0 }, dt: 0, beat: 0, bass: 0.8, low: 0.8 }),
            'row10 geometry fresh');
        assert.deepStrictEqual(geometryMap, geometryRef, 'row10 geometry change must reset offset');
    }

    function caseRow30Magnitude() {
        const gradient = [
            { h: 0.00, s: 1, v: 1 },
            { h: 0.17, s: 1, v: 1 },
            { h: 0.33, s: 1, v: 1 }
        ];

        const baseAlgo = loadScript('audiomagnitude.js', 0x44dc);
        baseAlgo.colors = gradient;
        baseAlgo.setFrequencyRange('Mids');
        baseAlgo.setFlip('Off');
        baseAlgo.setMirror('Off');
        baseAlgo.setBackgroundMode('Off');
        baseAlgo.setBackgroundBrightness(1);
        baseAlgo.setBrightness(1);
        baseAlgo.setBlur(0);
        const quiet = render(baseAlgo, 'audiomagnitude.js', 5, 1,
            makeFrame('mag-src', 30, { beat: 1, bass: 1, low: 1, mid: 0, high: 1 }),
            'row30 quiet');
        const full = render(baseAlgo, 'audiomagnitude.js', 5, 1,
            makeFrame('mag-src', 30, { beat: 0, bass: 0, low: 0, mid: 1, high: 0 }),
            'row30 full');
        const scaled = render(baseAlgo, 'audiomagnitude.js', 5, 1,
            makeFrame('mag-src', 30, { beat: 0, bass: 0, low: 0, mid: 0.4, high: 0 }),
            'row30 scaled');
        const doubled = render(baseAlgo, 'audiomagnitude.js', 5, 1,
            makeFrame('mag-src', 30, { beat: 0, bass: 0, low: 0, mid: 0.8, high: 0 }),
            'row30 doubled');
        for (let x = 0; x < 5; x++) {
            const q = rgbAt(quiet, x, 0, 5);
            const f = rgbAt(full, x, 0, 5);
            const s = rgbAt(scaled, x, 0, 5);
            const d = rgbAt(doubled, x, 0, 5);
            assert(approx(q[0], 0, 1e-6) && approx(q[1], 0, 1e-6) && approx(q[2], 0, 1e-6),
                `row30 quiet expected black at ${x}, got ${JSON.stringify(q)}`);
            assert(approx(s[0], f[0] * 0.4, 1e-6) && approx(s[1], f[1] * 0.4, 1e-6) && approx(s[2], f[2] * 0.4, 1e-6),
                `row30 .4 scalar mismatch at ${x}`);
            assert(approx(d[0], s[0] * 2, 1e-6) && approx(d[1], s[1] * 2, 1e-6) && approx(d[2], s[2] * 2, 1e-6),
                `row30 .8 scalar mismatch at ${x}`);
        }

        function expectedXFromBase(baseMap, width, opts) {
            let pixels = [];
            const n = width;
            for (let x = 0; x < n; x++) pixels.push(rgbAt(baseMap, x, 0, width));
            if (opts.flip === 'On') pixels = pixels.slice().reverse();
            if (opts.mirror === 'On') {
                const mirrored = new Array(n * 2);
                for (let i = 0; i < n; i++) {
                    mirrored[i] = pixels[n - 1 - i];
                    mirrored[n + i] = pixels[i];
                }
                const folded = new Array(n);
                for (let i = 0; i < n; i++) {
                    const a = mirrored[i * 2];
                    const b = mirrored[i * 2 + 1];
                    folded[i] = [
                        Math.max(a[0], b[0]),
                        Math.max(a[1], b[1]),
                        Math.max(a[2], b[2])
                    ];
                }
                pixels = folded;
            }
            if (opts.backgroundMode === 'Additive') {
                const bg = [0, 0, 1];
                for (let i = 0; i < n; i++) {
                    pixels[i] = [
                        pixels[i][0] + bg[0] * opts.backgroundBrightness,
                        pixels[i][1] + bg[1] * opts.backgroundBrightness,
                        pixels[i][2] + bg[2] * opts.backgroundBrightness
                    ];
                }
            }
            for (let i = 0; i < n; i++) {
                pixels[i] = [
                    pixels[i][0] * opts.brightness,
                    pixels[i][1] * opts.brightness,
                    pixels[i][2] * opts.brightness
                ];
            }
            return pixels;
        }

        const xAlgo = loadScript('audiomagnitude.js', 0x44dd);
        xAlgo.colors = gradient;
        xAlgo.setFrequencyRange('Mids');
        xAlgo.setFlip('On');
        xAlgo.setMirror('On');
        xAlgo.setBackgroundMode('Additive');
        xAlgo.setBackgroundColor('#0000ff');
        xAlgo.setBackgroundBrightness(1);
        xAlgo.setBrightness(0.5);
        xAlgo.setBlur(0);
        const xOut = render(xAlgo, 'audiomagnitude.js', 5, 1,
            makeFrame('mag-src', 30, { beat: 0, bass: 0, low: 0, mid: 0.8, high: 0 }),
            'row30 X exact');
        const expected = expectedXFromBase(doubled, 5, {
            flip: 'On',
            mirror: 'On',
            backgroundMode: 'Additive',
            backgroundBrightness: 1,
            brightness: 0.5
        });
        for (let x = 0; x < 5; x++) {
            const actual = rgbAt(xOut, x, 0, 5);
            const exp = expected[x];
            assert(approx(actual[0], exp[0], 1e-6), `row30 X red mismatch at ${x}: ${actual[0]} vs ${exp[0]}`);
            assert(approx(actual[1], exp[1], 1e-6), `row30 X green mismatch at ${x}: ${actual[1]} vs ${exp[1]}`);
            assert(approx(actual[2], exp[2], 1e-6), `row30 X blue mismatch at ${x}: ${actual[2]} vs ${exp[2]}`);
        }

        const geometryA = loadScript('audiomagnitude.js', 0x44ed);
        const geometryB = loadScript('audiomagnitude.js', 0x44ee);
        geometryA.colors = gradient;
        geometryB.colors = gradient;
        geometryA.setFrequencyRange('Mids');
        geometryB.setFrequencyRange('Mids');
        geometryA.setFlip('Off');
        geometryB.setFlip('Off');
        geometryA.setMirror('Off');
        geometryB.setMirror('Off');
        geometryA.setBackgroundMode('Off');
        geometryB.setBackgroundMode('Off');
        geometryA.setBrightness(1);
        geometryB.setBrightness(1);
        geometryA.setBlur(0);
        geometryB.setBlur(0);
        const horizontal = render(geometryA, 'audiomagnitude.js', 6, 1,
            makeFrame('mag-shape', 30, { beat: 0, bass: 0, low: 0, mid: 1, high: 0 }),
            'row30 geometry horizontal');
        const vertical = render(geometryB, 'audiomagnitude.js', 2, 3,
            makeFrame('mag-shape', 30, { beat: 0, bass: 0, low: 0, mid: 1, high: 0 }),
            'row30 geometry vertical');
        assert.deepStrictEqual(vertical, horizontal, 'row30 must sample gradient by flattened N (row-major)');
    }

    const results = [
        runCase('row18-filter-core', caseRow18Filter),
        runCase('row26-hierarchy-retention', caseRow26Hierarchy),
        runCase('row60-vumeter-raw-zones-markers', caseRow60Vumeter),
        runCase('row38-pitch-spectrum-bin-blend', caseRow38PitchSpectrum),
        runCase('row10-concentric-field-travel', caseRow10Concentric),
        runCase('row30-magnitude-s-g-x', caseRow30Magnitude)
    ];
    const failed = results.filter(r => !r.ok);
    console.log(`SUMMARY pass=${results.length - failed.length} fail=${failed.length}`);
    if (failed.length) {
        throw new Error(`Failed cases: ${failed.map(f => f.caseId).join(', ')}`);
    }
}

function assertWorkerEBeatRows() {
  const EPS = 1e-6;

  const workerEApprox = (a, b, eps = EPS) => Math.abs(a - b) <= eps;
  const workerELitIndices = map => {
    const out = [];
    for (let i = 0; i < map.length / 3; i++) if (map[i * 3 + 2] > 1e-6) out.push(i);
    return out;
  };
  const workerEPeakV = map => {
    let peak = 0;
    for (let i = 2; i < map.length; i += 3) peak = Math.max(peak, map[i]);
    return peak;
  };
  const workerEHsvAt = (map, width, x, y = 0) => {
    const i = (y * width + x) * 3;
    return { h: map[i], s: map[i + 1], v: map[i + 2] };
  };
  const workerEFrame = ({
    phase = 0,
    valid = true,
    beatDelta = 0,
    onsetDelta = 0,
    kickDelta = 0,
    beatFired = false,
    onset = false,
    kickFired = false,
    dt = 0,
    low = 0,
    mid = 0,
    high = 0,
    sourceId = 'worker-e',
    profileId = 1,
    sourceEpoch = 1,
    configRevision = 1
  } = {}) => audio({
    version: 6,
    phase,
    dt,
    timing: { deltaSeconds: dt },
    low,
    mid,
    high,
    beatFired,
    onset,
    kickFired,
    sourceId,
    profileId,
    sourceEpoch,
    configRevision,
    tempo: { valid, bpm: 120, beatPhase: phase, barPhase: phase },
    events: { delta: { beat: beatDelta, onset: onsetDelta, kick: kickDelta, bar: 0 } }
  });

  function assertWorkerERow03Bar() {
    const algo = loadScript('audiobpmbar.js', 0xE031);
    algo.colors = [{ h: 0.02, s: 1, v: 1 }, { h: 0.62, s: 1, v: 1 }];
    algo.setMode('wipe');
    algo.setEaseMethod('linear');
    algo.setBeatOffset(0);
    algo.setColorStep(0.125);
    algo.setBeatSkip('none');
    algo.setSkipEvery(1);

    const wipeA = render(algo, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.25 }), 'worker-e row03 wipeA');
    assert.deepStrictEqual(workerELitIndices(wipeA), [0, 1, 2, 3, 4], 'row03/wipe phase0 mismatch');

    render(algo, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.9 }), 'worker-e row03 wrap prep');
    render(algo, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.05 }), 'worker-e row03 wrap trigger');
    const wipeB = render(algo, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.25 }), 'worker-e row03 wipeB');
    assert.deepStrictEqual(workerELitIndices(wipeB), [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19], 'row03/wipe phase1 mismatch');

    const duplicateBase = render(algo, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.3, beatDelta: 0 }), 'worker-e row03 dup base');
    const duplicateBeat = render(algo, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.3, beatDelta: 2, beatFired: true }), 'worker-e row03 dup beat');
    assert.deepStrictEqual(duplicateBeat, duplicateBase, 'row03/duplicate phase mutated on beat deltas');

    const validBefore = render(algo, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.95, valid: true }), 'worker-e row03 invalid prep');
    const invalidNow = render(algo, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.05, valid: false }), 'worker-e row03 invalid');
    assert.deepStrictEqual(invalidNow, validBefore, 'row03/invalid tempo should not phase-wrap');

    const bounce = loadScript('audiobpmbar.js', 0xE032);
    bounce.colors = algo.colors;
    bounce.setMode('bounce');
    bounce.setEaseMethod('linear');
    const bounceMap = render(bounce, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.25 }), 'worker-e row03 bounce');
    assert.deepStrictEqual(workerELitIndices(bounceMap), [3, 4, 5, 6, 7, 8], 'row03/bounce mismatch');

    const inout = loadScript('audiobpmbar.js', 0xE033);
    inout.colors = algo.colors;
    inout.setMode('in-out');
    inout.setEaseMethod('linear');
    const ioA = render(inout, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.25 }), 'worker-e row03 inoutA');
    assert.deepStrictEqual(workerELitIndices(ioA), [0, 1, 2, 3, 4], 'row03/in-out phase0 mismatch');
    render(inout, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.9 }), 'worker-e row03 inout prep');
    render(inout, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.05 }), 'worker-e row03 inout trigger');
    const ioB = render(inout, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.25 }), 'worker-e row03 inoutB');
    assert.deepStrictEqual(workerELitIndices(ioB), [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14], 'row03/in-out phase1 mismatch');

    const easingExpected = { linear: 5, ease_in: 1, ease_out: 8, ease_in_out: 2 };
    for (const [method, expected] of Object.entries(easingExpected)) {
      const e = loadScript('audiobpmbar.js', 0xE034);
      e.colors = algo.colors;
      e.setMode('wipe');
      e.setEaseMethod(method);
      const m = render(e, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.25 }), `worker-e row03 easing ${method}`);
      assert.strictEqual(workerELitIndices(m).length, expected, `row03/${method} mismatch`);
    }

    const offset = loadScript('audiobpmbar.js', 0xE035);
    offset.colors = algo.colors;
    offset.setMode('wipe');
    offset.setEaseMethod('linear');
    offset.setBeatOffset(0.9);
    const offsetMap = render(offset, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.25 }), 'worker-e row03 offset');
    const expectedLit = Math.floor(20 * ((0.25 + 0.9) - Math.floor(0.25 + 0.9)));
    assert.strictEqual(workerELitIndices(offsetMap).length, expectedLit, 'row03/offset wrap mismatch');

    const skip = loadScript('audiobpmbar.js', 0xE036);
    skip.colors = algo.colors;
    skip.setMode('wipe');
    skip.setEaseMethod('linear');
    skip.setBeatSkip('even');
    skip.setSkipEvery(1);
    const skipped = render(skip, 'audiobpmbar.js', 20, 1, workerEFrame({ phase: 0.25 }), 'worker-e row03 skip even');
    assert(workerEPeakV(skipped) < 1e-8, 'row03/even skip should blackout at beatCount=0');

    markCase('row03-bar');
    console.log('PASS row03-bar');
  }

  function assertWorkerERow35MulticolorBar() {
    const colors = [{ h: 0.0, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];

    const wipe = loadScript('audiomulticolorbar.js', 0xE351);
    wipe.colors = colors;
    wipe.setMode('wipe');
    wipe.setEaseMethod('linear');
    wipe.setColorStep(0.5);

    const first = render(wipe, 'audiomulticolorbar.js', 8, 1, workerEFrame({ phase: 0.25 }), 'worker-e row35 wipe first');
    const firstLead = workerEHsvAt(first, 8, 0);
    const firstTrail = workerEHsvAt(first, 8, 7);
    for (let x = 0; x < 2; x++) {
      const p = workerEHsvAt(first, 8, x);
      assert(workerEApprox(p.h, firstLead.h) && workerEApprox(p.v, firstLead.v), `row35/phase0 lead mismatch at ${x}`);
    }
    for (let x = 2; x < 8; x++) {
      const p = workerEHsvAt(first, 8, x);
      assert(workerEApprox(p.h, firstTrail.h) && workerEApprox(p.v, firstTrail.v), `row35/phase0 trail mismatch at ${x}`);
    }

    render(wipe, 'audiomulticolorbar.js', 8, 1, workerEFrame({ phase: 0.25, beatDelta: 1, beatFired: true }), 'worker-e row35 beat+1');
    const second = render(wipe, 'audiomulticolorbar.js', 8, 1, workerEFrame({ phase: 0.25 }), 'worker-e row35 wipe second');
    const secondLead = workerEHsvAt(second, 8, 0);
    const secondTrail = workerEHsvAt(second, 8, 7);
    for (let x = 0; x < 6; x++) {
      const p = workerEHsvAt(second, 8, x);
      assert(workerEApprox(p.h, secondLead.h) && workerEApprox(p.v, secondLead.v), `row35/phase1 lead mismatch at ${x}`);
    }
    for (let x = 6; x < 8; x++) {
      const p = workerEHsvAt(second, 8, x);
      assert(workerEApprox(p.h, secondTrail.h) && workerEApprox(p.v, secondTrail.v), `row35/phase1 trail mismatch at ${x}`);
    }

    const cascade = loadScript('audiomulticolorbar.js', 0xE352);
    cascade.colors = colors;
    cascade.setMode('cascade');
    cascade.setEaseMethod('linear');
    cascade.setColorStep(0.5);
    render(cascade, 'audiomulticolorbar.js', 8, 1, workerEFrame({ phase: 0.25, beatDelta: 1, beatFired: true }), 'worker-e row35 cascade beat');
    const cascadeMap = render(cascade, 'audiomulticolorbar.js', 8, 1, workerEFrame({ phase: 0.25 }), 'worker-e row35 cascade');
    const cascadeLead = workerEHsvAt(cascadeMap, 8, 0);
    const cascadeTrail = workerEHsvAt(cascadeMap, 8, 7);
    for (let x = 0; x < 2; x++) {
      const p = workerEHsvAt(cascadeMap, 8, x);
      assert(workerEApprox(p.h, cascadeLead.h) && workerEApprox(p.v, cascadeLead.v), `row35/cascade lead mismatch at ${x}`);
    }
    for (let x = 2; x < 8; x++) {
      const p = workerEHsvAt(cascadeMap, 8, x);
      assert(workerEApprox(p.h, cascadeTrail.h) && workerEApprox(p.v, cascadeTrail.v), `row35/cascade trail mismatch at ${x}`);
    }

    const delta2 = loadScript('audiomulticolorbar.js', 0xE353);
    delta2.colors = [{ h: 0.05, s: 1, v: 1 }, { h: 0.35, s: 1, v: 1 }, { h: 0.65, s: 1, v: 1 }];
    delta2.setMode('wipe');
    delta2.setEaseMethod('linear');
    delta2.setColorStep(0.125);
    const before = render(delta2, 'audiomulticolorbar.js', 8, 1, workerEFrame({ phase: 0.25, beatDelta: 0 }), 'worker-e row35 delta2 before');
    render(delta2, 'audiomulticolorbar.js', 8, 1, workerEFrame({ phase: 0.25, beatDelta: 2, beatFired: true }), 'worker-e row35 delta2 trigger');
    const after = render(delta2, 'audiomulticolorbar.js', 8, 1, workerEFrame({ phase: 0.25, beatDelta: 0 }), 'worker-e row35 delta2 after');
    assert.deepStrictEqual(workerELitIndices(after), workerELitIndices(before), 'row35/delta2 should restore parity');
    assert.notDeepStrictEqual(after, before, 'row35/delta2 should advance color twice');

    const zeroDelta = render(delta2, 'audiomulticolorbar.js', 8, 1, workerEFrame({ phase: 0.25, beatDelta: 0 }), 'worker-e row35 zero delta');
    assert.deepStrictEqual(zeroDelta, after, 'row35/zero delta should not mutate state');

    const oneStop = loadScript('audiomulticolorbar.js', 0xE354);
    oneStop.colors = [{ h: 0.8, s: 0.7, v: 0.4 }];
    oneStop.setMode('wipe');
    oneStop.setEaseMethod('linear');
    const oneStopMap = render(oneStop, 'audiomulticolorbar.js', 8, 1, workerEFrame({ phase: 0.25 }), 'worker-e row35 one-stop');
    for (let x = 1; x < 8; x++) {
      assert(workerEApprox(workerEHsvAt(oneStopMap, 8, x).h, workerEHsvAt(oneStopMap, 8, 0).h), 'row35/one-stop hue mismatch');
      assert(workerEApprox(workerEHsvAt(oneStopMap, 8, x).v, workerEHsvAt(oneStopMap, 8, 0).v), 'row35/one-stop value mismatch');
    }

    markCase('row35-multibar');
    console.log('PASS row35-multibar');
  }

  function assertWorkerERow31Marching() {
    const mk = (seed, reactivity) => {
      const s = loadScript('audiomarching.js', seed);
      s.colors = [{ h: 0.0, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];
      s.setSpeed(0.1);
      s.setReactivity(reactivity);
      return s;
    };
    const run = (script, lowA, lowB) => {
      render(script, 'audiomarching.js', 17, 1, workerEFrame({ dt: 0.2, low: lowA }), 'worker-e row31 warmupA');
      return render(script, 'audiomarching.js', 17, 1, workerEFrame({ dt: 0.2, low: lowB }), 'worker-e row31 warmupB');
    };

    const reactiveHi = run(mk(0xE311, 1.0), 0, 0.7);
    const reactiveLo = run(mk(0xE312, 1.0), 0, 0);
    assert.notDeepStrictEqual(reactiveHi, reactiveLo, 'row31/raw low should offset marching wave at reactivity=1');

    let minV = 1;
    let maxV = 0;
    for (let i = 2; i < reactiveHi.length; i += 3) {
      minV = Math.min(minV, reactiveHi[i]);
      maxV = Math.max(maxV, reactiveHi[i]);
    }
    assert(minV <= 1e-6, `row31/expected dark valleys, min=${minV}`);
    assert(maxV > 0.2, `row31/expected bright ridges, max=${maxV}`);

    const inertA = run(mk(0xE313, 0.00001), 0, 0.1);
    const inertB = run(mk(0xE314, 0.00001), 0, 0.7);
    let delta = 0;
    for (let i = 0; i < inertA.length; i++) delta += Math.abs(inertA[i] - inertB[i]);
    assert(delta < 1e-3, `row31/zero-reactivity must be invariant to low changes, delta=${delta}`);

    const singleton = loadScript('audiomarching.js', 0xE315);
    singleton.setSpeed(0.1);
    singleton.setReactivity(0.2);
    const one = render(singleton, 'audiomarching.js', 1, 1, workerEFrame({ dt: 0.2, low: 0.5 }), 'worker-e row31 1x1');
    assert(one.length === 3 && one.every(v => Number.isFinite(v) && v >= 0 && v <= 1), 'row31/1x1 contract failure');

    markCase('row31-marching');
    console.log('PASS row31-marching');
  }

  function assertWorkerERow57Spotlight() {
    const halfLifeExpected = Math.pow(0.5, 1.4);

    const decay = loadScript('audiospotlight.js', 0xE571);
    decay.colors = [{ h: 0, s: 0, v: 1 }, { h: 0.66, s: 1, v: 1 }];
    decay.setGradient('No');
    decay.setMaxSpots(1);
    decay.setFadeSeconds(0.8);
    decay.setBirthGain(0);
    const born = render(decay, 'audiospotlight.js', 11, 1, workerEFrame({ dt: 0, onset: true, onsetDelta: 1 }), 'worker-e row57 born');
    const half = render(decay, 'audiospotlight.js', 11, 1, workerEFrame({ dt: 0.4 }), 'worker-e row57 half');
    const ratio = workerEPeakV(half) / Math.max(1e-9, workerEPeakV(born));
    assert(Math.abs(ratio - halfLifeExpected) < 0.03,
      `row57/life^1.4 half-life mismatch expected ${halfLifeExpected}, got ${ratio}`);

    const quiet = loadScript('audiospotlight.js', 0xE572);
    quiet.colors = [{ h: 0.1, s: 1, v: 1 }];
    quiet.setGradient('No');
    quiet.setMaxSpots(1);
    quiet.setBirthGain(0);
    const quietMap = render(quiet, 'audiospotlight.js', 13, 1, workerEFrame({ dt: 0.4, low: 0, mid: 0, high: 0 }), 'worker-e row57 quiet');
    assert(workerEPeakV(quietMap) > 0.05, 'row57/quiet should still birth from base rate');

    const activity = loadScript('audiospotlight.js', 0xE573);
    activity.colors = [{ h: 0.3, s: 1, v: 1 }];
    activity.setGradient('No');
    activity.setMaxSpots(8);
    activity.setBirthGain(0);
    const lowAct = render(activity, 'audiospotlight.js', 17, 1, workerEFrame({ dt: 0.2, low: 0.1, mid: 0.1, high: 0.1 }), 'worker-e row57 lowAct');
    const highAct = render(activity, 'audiospotlight.js', 17, 1, workerEFrame({ dt: 0.2, low: 1, mid: 1, high: 1 }), 'worker-e row57 highAct');
    assert(workerEPeakV(highAct) >= workerEPeakV(lowAct), 'row57/higher activity should not reduce births');

    const wrap = loadScript('audiospotlight.js', 0xE574);
    wrap.colors = [{ h: 0, s: 0, v: 1 }];
    wrap.setGradient('No');
    wrap.setMaxSpots(1);
    wrap.setFadeSeconds(2);
    wrap.setBirthGain(0);
    render(wrap, 'audiospotlight.js', 17, 1, workerEFrame({ dt: 0, onset: true, onsetDelta: 1 }), 'worker-e row57 wrap birth');
    const wrapMap = render(wrap, 'audiospotlight.js', 17, 1, workerEFrame({ dt: 0.01 }), 'worker-e row57 wrap');
    const left = workerEHsvAt(wrapMap, 17, 0).v;
    const right = workerEHsvAt(wrapMap, 17, 16).v;
    assert(left > 0 || right > 0, 'row57/wrap should light at least one strip edge');

    const overlapMapAtBrightness = brightness => {
      const overlap = loadScript('audiospotlight.js', 0xE575);
      overlap.colors = [{ h: 1 / 12, s: 1, v: 1 }];
      overlap.setGradient('No');
      overlap.setWidth(0.6);
      overlap.setMaxSpots(2);
      overlap.setFadeSeconds(5);
      overlap.setBirthGain(30);
      overlap.setBrightness(brightness);
      return render(overlap, 'audiospotlight.js', 1, 1,
        workerEFrame({ dt: 0, low: 1, mid: 1, high: 1, onset: true, onsetDelta: 2, kickFired: true, kickDelta: 2 }),
        `worker-e row57 overlap brightness ${brightness}`);
    };
    const overlap1 = overlapMapAtBrightness(1);
    const overlap05 = overlapMapAtBrightness(0.5);
    const p1 = workerEHsvAt(overlap1, 1, 0);
    const p05 = workerEHsvAt(overlap05, 1, 0);
    assert(p1.v > 0.99, `row57/overlap expected clipped full-scale at brightness=1, got ${p1.v}`);
    assert(p05.v > 0.75, `row57/overlap brightness=0.5 lost overdrive before X stage, got ${p05.v}`);
    assert(p05.h < 0.12, `row57/overlap hue should preserve [2,1,0]-like ratio, got h=${p05.h}`);

    const grad = loadScript('audiospotlight.js', 0xE576);
    grad.colors = [{ h: 0.0, s: 1, v: 1 }, { h: 0.5, s: 1, v: 1 }];
    grad.setGradient('Yes');
    grad.setGradientSpeed(1.0);
    grad.setColorSpan(1.0);
    grad.setMaxSpots(1);
    grad.setBirthGain(0);
    const g1 = render(grad, 'audiospotlight.js', 11, 1, workerEFrame({ dt: 0, onset: true, onsetDelta: 1 }), 'worker-e row57 gradient first');
    const g2 = render(grad, 'audiospotlight.js', 11, 1, workerEFrame({ dt: 0.5 }), 'worker-e row57 gradient second');
    assert.notDeepStrictEqual(g1, g2, 'row57/gradient speed+span should move colors');

    markCase('row57-spotlight');
    console.log('PASS row57-spotlight');
  }

  assertWorkerERow03Bar();
  assertWorkerERow35MulticolorBar();
  assertWorkerERow31Marching();
  assertWorkerERow57Spotlight();
}

function assertWorkerHAtmosphereRows() {
function workerHFrame(overrides) {
    return audio(Object.assign({
        version: 6,
        dt: 1 / 60,
        timing: { deltaSeconds: 1 / 60 },
        bpm: 120,
        sourceId: 'worker-h-source',
        profileId: 88,
        sourceEpoch: 1,
        configRevision: 1,
        low: 0,
        mid: 0,
        high: 0,
        powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } },
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
        banks: { full: { count: 12, processed: new Array(12).fill(0), novelty: new Array(12).fill(0) } }
    }, overrides || {}));
}

function workerHHsvAt(map, index) {
    return {
        h: map[index * 3],
        s: map[index * 3 + 1],
        v: map[index * 3 + 2]
    };
}

function workerHPeakV(map) {
    var peak = 0;
    for (var i = 2; i < map.length; i += 3) peak = Math.max(peak, map[i]);
    return peak;
}

function workerHValues(map) {
    var out = [];
    for (var i = 2; i < map.length; i += 3) out.push(map[i]);
    return out;
}

function workerHRgbAt(map, index) {
    return hsvPixels(map)[index];
}

function workerHApprox(actual, expected, eps, label) {
    assert(Math.abs(actual - expected) <= eps, `${label}: expected ${expected}, got ${actual}`);
}

function workerHApproxRgb(actual, expected, eps, label) {
    for (var i = 0; i < 3; i++)
        workerHApprox(actual[i], expected[i], eps, `${label}[${i}]`);
}

function workerHClamp01(x) {
    return x < 0 ? 0 : (x > 1 ? 1 : x);
}

function workerHMod1(x) {
    var m = x - Math.floor(x);
    return m < 0 ? m + 1 : m;
}

function workerHSin01(x) {
    return 0.5 + 0.5 * Math.sin(2 * Math.PI * x);
}

function workerHTriangle(x) {
    return 1 - Math.abs(2 * workerHMod1(x) - 1);
}

function workerHHsvToRgb(h, s, v) {
    h = workerHMod1(h);
    s = workerHClamp01(s);
    var i = Math.floor(h * 6);
    var f = h * 6 - i;
    var p = v * (1 - s);
    var q = v * (1 - f * s);
    var t = v * (1 - (1 - f) * s);
    switch (i % 6) {
    case 0: return [v, t, p];
    case 1: return [q, v, p];
    case 2: return [p, v, t];
    case 3: return [p, q, v];
    case 4: return [t, p, v];
    default: return [v, p, q];
    }
}

function workerHGradientRgbAt(stops, t) {
    var clamped = workerHClamp01(t);
    var pos = clamped * (stops.length - 1);
    var idx = Math.min(stops.length - 2, Math.floor(pos));
    var frac = pos - idx;
    var slope = 1.5;
    var powT = Math.pow(frac, slope);
    var invPowT = Math.pow(1 - frac, slope);
    var eased = powT + invPowT > 0 ? powT / (powT + invPowT) : frac;
    var a = workerHHsvToRgb(stops[idx].h, stops[idx].s, stops[idx].v);
    var b = workerHHsvToRgb(stops[idx + 1].h, stops[idx + 1].s, stops[idx + 1].v);
    return [
        a[0] + (b[0] - a[0]) * eased,
        a[1] + (b[1] - a[1]) * eased,
        a[2] + (b[2] - a[2]) * eased
    ];
}

function workerHAssertRow07Blocks() {
    var algo = loadScript('audioblocks.js', 0x1701);
    algo.setMode('LedFx Block Reflections');
    algo.setSpeed(1.0);
    algo.setReactivity(0.5);
    algo.setFixHues('No');
    algo.colors = [{ h: 0.0, s: 1, v: 1 }, { h: 1 / 3, s: 1, v: 1 }, { h: 2 / 3, s: 1, v: 1 }];
    var frame = workerHFrame({
        dt: 1 / 60,
        timing: { deltaSeconds: 1 / 60 },
        low: 0.4,
        powers: { raw: { low: 0.4 } }
    });
    var map = render(algo, 'audioblocks.js', 7, 2, frame, 'worker-h row07 baseline');
    var seconds = 1 / 60;
    var dtMs = seconds * 1000;
    var speed = 1.0;
    var n = 14;
    var alpha = 1 - Math.pow(0.95, seconds * 60);
    var lows = 0.4 * alpha;
    var t1 = dtMs / (65536 / speed);
    var t2 = t1 * (Math.PI * Math.PI) + 0.8 * 0.5 * lows;
    var t3 = dtMs / (65536 / (speed * 5.0)) + 0.5 * lows;
    var t4 = dtMs / (65536 / (speed * 2.0)) * (Math.PI * Math.PI);
    var m = 0.3 + workerHTriangle(t1) * 0.2;
    var c = workerHTriangle(t3) * 10 + 4 * workerHSin01(t4);
    [1, 11].forEach(function(i) {
        var h = ((i - n / 2) / n) * c;
        h = ((h % m) + m) % m;
        h += workerHSin01(t2);
        var v = Math.abs(h);
        v = workerHMod1(v + Math.abs(m) + t1);
        v = v * v;
        var grad = workerHGradientRgbAt(algo.colors, workerHMod1(h));
        var expectedRgb = [grad[0] * v, grad[1] * v, grad[2] * v];
        workerHApproxRgb(workerHRgbAt(map, i), expectedRgb, 3e-3, `row07 expected rgb i=${i}`);
    });
    markCase('row07.blocks-formula-rgb');

    var flatA = loadScript('audioblocks.js', 0x1702);
    var flatB = loadScript('audioblocks.js', 0x1702);
    flatA.setMode('LedFx Block Reflections');
    flatB.setMode('LedFx Block Reflections');
    var frameFlat = workerHFrame({
        low: 0.4,
        powers: { raw: { low: 0.8 } }
    });
    var a = render(flatA, 'audioblocks.js', 7, 2, frameFlat, 'worker-h row07 7x2');
    var b = render(flatB, 'audioblocks.js', 14, 1, frameFlat, 'worker-h row07 14x1');
    assert.deepStrictEqual(a, b, 'row07: flattened geometry mismatch');
    markCase('row07.blocks-flattened-n');

    var ledLo = loadScript('audioblocks.js', 0x1703);
    var ledHi = loadScript('audioblocks.js', 0x1703);
    ledLo.setMode('LedFx Block Reflections');
    ledHi.setMode('LedFx Block Reflections');
    var lowRaw = render(ledLo, 'audioblocks.js', 7, 2, workerHFrame({ low: 0.6, powers: { raw: { low: 0.1 } } }), 'worker-h row07 led raw low');
    var highRaw = render(ledHi, 'audioblocks.js', 7, 2, workerHFrame({ low: 0.6, powers: { raw: { low: 0.9 } } }), 'worker-h row07 led raw high');
    assert.notDeepStrictEqual(lowRaw, highRaw, 'row07: LedFx should use raw low');
    markCase('row07.blocks-raw-low-source');

    var artLo = loadScript('audioblocks.js', 0x1704);
    var artHi = loadScript('audioblocks.js', 0x1704);
    var artA = render(artLo, 'audioblocks.js', 7, 2, workerHFrame({ low: 0.6, powers: { raw: { low: 0.1 } } }), 'worker-h row07 art raw low');
    var artB = render(artHi, 'audioblocks.js', 7, 2, workerHFrame({ low: 0.6, powers: { raw: { low: 0.9 } } }), 'worker-h row07 art raw high');
    assert.deepStrictEqual(artA, artB, 'row07: Artistic should not change from raw low decoy');
    markCase('row07.blocks-artistic-decoy');
}

function workerHAssertRow11Crawler() {
    var base = loadScript('audiocrawler.js', 0x1711);
    base.setMode('LedFx Crawler');
    base.setReactivity(0.25);
    var chop = loadScript('audiocrawler.js', 0x1711);
    chop.setMode('LedFx Crawler');
    chop.setReactivity(0.25);
    chop.setChop(70);
    var sway = loadScript('audiocrawler.js', 0x1711);
    sway.setMode('LedFx Crawler');
    sway.setReactivity(0.25);
    sway.setSway(40);
    var f = workerHFrame({ low: 0.4, powers: { raw: { low: 0.4 } }, timing: { deltaSeconds: 1 / 60 } });
    var mapBase = render(base, 'audiocrawler.js', 7, 2, f, 'worker-h row11 base');
    var mapChop = render(chop, 'audiocrawler.js', 7, 2, f, 'worker-h row11 chop');
    var mapSway = render(sway, 'audiocrawler.js', 7, 2, f, 'worker-h row11 sway');
    assert.notDeepStrictEqual(workerHValues(mapBase), workerHValues(mapChop), 'row11: chop should alter value field');
    assert.notDeepStrictEqual(workerHValues(mapBase), workerHValues(mapSway), 'row11: sway should alter field independently');
    markCase('row11.crawler-chop-independence');

    var ledLo = loadScript('audiocrawler.js', 0x1712);
    var ledHi = loadScript('audiocrawler.js', 0x1712);
    ledLo.setMode('LedFx Crawler');
    ledHi.setMode('LedFx Crawler');
    var mLo = render(ledLo, 'audiocrawler.js', 7, 2, workerHFrame({ low: 0.4, powers: { raw: { low: 0.1 } } }), 'worker-h row11 raw low');
    var mHi = render(ledHi, 'audiocrawler.js', 7, 2, workerHFrame({ low: 0.4, powers: { raw: { low: 0.9 } } }), 'worker-h row11 raw high');
    assert.notDeepStrictEqual(mLo, mHi, 'row11: raw-low pulse must move LedFx mode');
    markCase('row11.crawler-raw-low');

    var artA = loadScript('audiocrawler.js', 0x1713);
    var artB = loadScript('audiocrawler.js', 0x1713);
    var a = render(artA, 'audiocrawler.js', 7, 2, workerHFrame({ low: 0.4, powers: { raw: { low: 0.1 } } }), 'worker-h row11 art low');
    var b = render(artB, 'audiocrawler.js', 7, 2, workerHFrame({ low: 0.4, powers: { raw: { low: 0.9 } } }), 'worker-h row11 art high');
    assert.deepStrictEqual(a, b, 'row11: artistic path changed from raw decoy');
    markCase('row11.crawler-artistic-decoy');

    var safe = loadScript('audiocrawler.js', 0x1714);
    safe.setMode('LedFx Crawler');
    safe.setSpeed(0);
    safe.setStretch(0);
    var safeMap = render(safe, 'audiocrawler.js', 7, 2, workerHFrame({ low: 0, powers: { raw: { low: 0 } }, timing: { deltaSeconds: 0.25 } }), 'worker-h row11 speed0');
    assert(safeMap.every(Number.isFinite), 'row11: speed=0/stretch=0 must stay finite');
    markCase('row11.crawler-speed0-safe');
}

function workerHAssertRow19Fire() {
    var led = loadScript('audiofire.js', 0x1719);
    led.setMode('LedFx Fire');
    led.setSpeed(0.5);
    led.setIntensity(1);
    led.setFadeChance(0);
    var warm1 = render(led, 'audiofire.js', 12, 1, workerHFrame({ low: 0.7, powers: { raw: { low: 0.7 } } }), 'worker-h row19 warm1');
    var warm2 = render(led, 'audiofire.js', 12, 1, workerHFrame({ low: 0.7, powers: { raw: { low: 0.7 } } }), 'worker-h row19 warm2');
    assert(workerHPeakV(warm2) > 0.01, 'row19: spark heat did not appear');
    assert.notDeepStrictEqual(warm1, warm2, 'row19: spark should traverse between frames');
    led.setSpeed(0);
    var cool = render(led, 'audiofire.js', 12, 1, workerHFrame({ low: 0, powers: { raw: { low: 0 } }, timing: { deltaSeconds: 0.5 } }), 'worker-h row19 cool');
    assert(workerHPeakV(cool) < workerHPeakV(warm2), 'row19: cooling phase did not decay');
    markCase('row19.fire-lifecycle');

    var lo = loadScript('audiofire.js', 0x1720);
    var hi = loadScript('audiofire.js', 0x1720);
    lo.setMode('LedFx Fire');
    hi.setMode('LedFx Fire');
    render(lo, 'audiofire.js', 10, 2, workerHFrame({ low: 0.7, powers: { raw: { low: 0.5 } } }), 'worker-h row19 raw prime low');
    render(hi, 'audiofire.js', 10, 2, workerHFrame({ low: 0.7, powers: { raw: { low: 0.5 } } }), 'worker-h row19 raw prime high');
    var a = render(lo, 'audiofire.js', 10, 2, workerHFrame({ low: 0.7, powers: { raw: { low: 0.1 } }, timing: { deltaSeconds: 0.2 } }), 'worker-h row19 raw low');
    var b = render(hi, 'audiofire.js', 10, 2, workerHFrame({ low: 0.7, powers: { raw: { low: 0.9 } }, timing: { deltaSeconds: 0.2 } }), 'worker-h row19 raw high');
    assert.notDeepStrictEqual(a, b, 'row19: LedFx Fire should use raw low');
    markCase('row19.fire-raw-low-decoy');

    var tiny = loadScript('audiofire.js', 0x1721);
    tiny.setMode('LedFx Fire');
    var tinyMap = render(tiny, 'audiofire.js', 4, 1, workerHFrame({ low: 0.8, powers: { raw: { low: 0.8 } } }), 'worker-h row19 tiny');
    assert(tinyMap.every(Number.isFinite), 'row19: N<=5 diffusion guard failed');
    markCase('row19.fire-small-n-guard');
}

function workerHAssertRow24Glitch() {
    var lowT = loadScript('audioglitch.js', 0x1724);
    var highT = loadScript('audioglitch.js', 0x1724);
    lowT.setMode('LedFx Glitch');
    highT.setMode('LedFx Glitch');
    lowT.setSaturation(0.1);
    highT.setSaturation(0.9);
    var f = workerHFrame({ low: 0.4, timing: { deltaSeconds: 0.1 } });
    var a = render(lowT, 'audioglitch.js', 12, 1, f, 'worker-h row24 sat low');
    var b = render(highT, 'audioglitch.js', 12, 1, f, 'worker-h row24 sat high');
    assert.notDeepStrictEqual(a, b, 'row24: saturation threshold did not alter field');
    markCase('row24.glitch-saturation-threshold');

    var onsetOn = loadScript('audioglitch.js', 0x1725);
    var onsetOff = loadScript('audioglitch.js', 0x1725);
    onsetOn.setMode('LedFx Glitch');
    onsetOff.setMode('LedFx Glitch');
    var on = render(onsetOn, 'audioglitch.js', 12, 1, workerHFrame({
        low: 0.4,
        timing: { deltaSeconds: 0 },
        onset: true,
        onsetIntensity: 1,
        events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-h row24 onset on');
    var off = render(onsetOff, 'audioglitch.js', 12, 1, workerHFrame({
        low: 0.4,
        timing: { deltaSeconds: 0 },
        onset: false,
        onsetIntensity: 0,
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-h row24 onset off');
    assert.deepStrictEqual(on, off, 'row24: LedFx Glitch should ignore onset-only changes');
    markCase('row24.glitch-ignore-onset');

    var motion = loadScript('audioglitch.js', 0x1726);
    motion.setMode('LedFx Glitch');
    var m1 = render(motion, 'audioglitch.js', 10, 1, workerHFrame({ low: 0, timing: { deltaSeconds: 0.1 } }), 'worker-h row24 motion1');
    var m2 = render(motion, 'audioglitch.js', 10, 1, workerHFrame({ low: 0, timing: { deltaSeconds: 0.1 } }), 'worker-h row24 motion2');
    assert.notDeepStrictEqual(m1, m2, 'row24: low=0 should still advance with time');
    var safe = loadScript('audioglitch.js', 0x1727);
    safe.setMode('LedFx Glitch');
    safe.setSpeed(0);
    var safeMap = render(safe, 'audioglitch.js', 10, 1, workerHFrame({ low: 0.6, timing: { deltaSeconds: 0.2 } }), 'worker-h row24 speed0');
    assert(safeMap.every(Number.isFinite), 'row24: speed=0 must stay finite');
    markCase('row24.glitch-motion-and-speed0');
}

function workerHAssertRow29Lava() {
    var algo = loadScript('audiolava.js', 0x1729);
    algo.setMode('LedFx Lava Lamp');
    algo.setSpeed(7);
    algo.setContrast(0.6);
    algo.setReactivity(0.3);
    algo.colors = [{ h: 0.0, s: 1, v: 1 }, { h: 1 / 3, s: 1, v: 1 }, { h: 2 / 3, s: 1, v: 1 }];
    var map = render(algo, 'audiolava.js', 9, 1, workerHFrame({
        low: 0,
        powers: { raw: { low: 0 } },
        dt: 0,
        timing: { deltaSeconds: 0 }
    }), 'worker-h row29 formula');
    var picks = [0, 4, 8];
    picks.forEach(function(i) {
        var il = i / 8;
        var w1 = workerHSin01(il);
        var w2 = workerHSin01(-il);
        var w3 = workerHSin01((il + w1 + w2) % 1);
        var h = il;
        w1 += 0.1;
        var pattern = w1 * w2 * w3;
        h += pattern * 0.1;
        var v = Math.pow(pattern + (1 - 0.6), 2);
        var grad = workerHGradientRgbAt(algo.colors, workerHMod1(h));
        var expected = [grad[0] * v, grad[1] * v, grad[2] * v];
        workerHApproxRgb(workerHRgbAt(map, i), expected, 4e-3, `row29 expected rgb i=${i}`);
    });
    markCase('row29.lava-formula-rgb');

    var cLo = loadScript('audiolava.js', 0x1730);
    var cHi = loadScript('audiolava.js', 0x1730);
    cLo.setMode('LedFx Lava Lamp');
    cHi.setMode('LedFx Lava Lamp');
    cLo.setContrast(0.2);
    cHi.setContrast(0.9);
    var lo = render(cLo, 'audiolava.js', 9, 1, workerHFrame({ low: 0.3, powers: { raw: { low: 0.3 } } }), 'worker-h row29 contrast low');
    var hi = render(cHi, 'audiolava.js', 9, 1, workerHFrame({ low: 0.3, powers: { raw: { low: 0.3 } } }), 'worker-h row29 contrast high');
    assert.notDeepStrictEqual(lo, hi, 'row29: contrast should alter valleys');
    markCase('row29.lava-contrast');

    var rawLo = loadScript('audiolava.js', 0x1731);
    var rawHi = loadScript('audiolava.js', 0x1731);
    rawLo.setMode('LedFx Lava Lamp');
    rawHi.setMode('LedFx Lava Lamp');
    var a = render(rawLo, 'audiolava.js', 9, 2, workerHFrame({ low: 0.6, powers: { raw: { low: 0.1 } } }), 'worker-h row29 raw low');
    var b = render(rawHi, 'audiolava.js', 9, 2, workerHFrame({ low: 0.6, powers: { raw: { low: 0.9 } } }), 'worker-h row29 raw high');
    assert.notDeepStrictEqual(a, b, 'row29: raw low should alter shape and speed');
    markCase('row29.lava-raw-low');

    var bpmA = loadScript('audiolava.js', 0x1732);
    var bpmB = loadScript('audiolava.js', 0x1732);
    bpmA.setMode('LedFx Lava Lamp');
    bpmB.setMode('LedFx Lava Lamp');
    var mA = render(bpmA, 'audiolava.js', 9, 1, workerHFrame({ dt: 0.5, bpm: 90, timing: { deltaSeconds: 0.2 }, low: 0.4, powers: { raw: { low: 0.4 } } }), 'worker-h row29 bpmA');
    var mB = render(bpmB, 'audiolava.js', 9, 1, workerHFrame({ dt: 0.1, bpm: 180, timing: { deltaSeconds: 0.2 }, low: 0.4, powers: { raw: { low: 0.4 } } }), 'worker-h row29 bpmB');
    assert.deepStrictEqual(mA, mB, 'row29: equal seconds should ignore bpm');
    markCase('row29.lava-seconds-invariance');
}

function workerHAssertRow32Melt() {
    var algo = loadScript('audiomelt.js', 0x1732);
    algo.setMode('LedFx Melt');
    algo.setSpeed(0.5);
    algo.setReactivity(0.5);
    var m0 = render(algo, 'audiomelt.js', 7, 2, workerHFrame({
        low: 0.3,
        powers: { raw: { low: 0.1 } },
        timing: { deltaSeconds: 1 / 60 }
    }), 'worker-h row32 raw low');
    var m1 = render(algo, 'audiomelt.js', 7, 2, workerHFrame({
        low: 0.3,
        powers: { raw: { low: 0.9 } },
        timing: { deltaSeconds: 1 / 60 }
    }), 'worker-h row32 raw high');
    assert.notDeepStrictEqual(m0, m1, 'row32: raw low must move LedFx Melt');
    var peakRatio = workerHPeakV(m1) / Math.max(1e-9, workerHPeakV(m0));
    assert(peakRatio > 0.8 && peakRatio < 1.25, `row32: brightness should stay stable while phase moves, ratio=${peakRatio}`);
    markCase('row32.melt-raw-motion-stable-brightness');

    var safe = loadScript('audiomelt.js', 0x1733);
    safe.setMode('LedFx Melt');
    safe.setSpeed(0);
    var s1 = render(safe, 'audiomelt.js', 7, 2, workerHFrame({
        low: 0.7,
        powers: { raw: { low: 0.7 } },
        timing: { deltaSeconds: 0.2 }
    }), 'worker-h row32 speed0 first');
    var s2 = render(safe, 'audiomelt.js', 7, 2, workerHFrame({
        low: 0.7,
        powers: { raw: { low: 0.7 } },
        timing: { deltaSeconds: 0.2 }
    }), 'worker-h row32 speed0 second');
    assert(s1.every(Number.isFinite) && s2.every(Number.isFinite), 'row32: speed=0 generated non-finite values');
    assert.deepStrictEqual(s1, s2, 'row32: speed=0 should hold phase');
    markCase('row32.melt-speed0-finite');
}

function workerHAssertRow33MeltSparkle() {
    var ledHitAlgo = loadScript('audiomeltsparkle.js', 0x1733);
    ledHitAlgo.setMode('LedFx Melt and Sparkle');
    ledHitAlgo.setBgBright(0);
    ledHitAlgo.setStrobeBlur(0);
    ledHitAlgo.setStrobeThreshold(0.75);
    var ledHit = render(ledHitAlgo, 'audiomeltsparkle.js', 12, 1, workerHFrame({
        low: 0.3,
        mid: 0.3,
        high: 0.95,
        events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } },
        banks: { full: { count: 12, processed: [0,0,0,0,0,0,0,0,0,0,0.95,1], novelty: new Array(12).fill(0.1) } },
        timing: { deltaSeconds: 0.1 }
    }), 'worker-h row33 led hit');
    var ledNoOnsetAlgo = loadScript('audiomeltsparkle.js', 0x1733);
    ledNoOnsetAlgo.setMode('LedFx Melt and Sparkle');
    ledNoOnsetAlgo.setBgBright(0);
    ledNoOnsetAlgo.setStrobeBlur(0);
    ledNoOnsetAlgo.setStrobeThreshold(0.75);
    var ledNoOnset = render(ledNoOnsetAlgo, 'audiomeltsparkle.js', 12, 1, workerHFrame({
        low: 0.3,
        mid: 0.3,
        high: 0.95,
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
        banks: { full: { count: 12, processed: [0,0,0,0,0,0,0,0,0,0,0.95,1], novelty: new Array(12).fill(0.1) } },
        timing: { deltaSeconds: 0.1 }
    }), 'worker-h row33 led sustained');
    assert(workerHPeakV(ledHit) > workerHPeakV(ledNoOnset) + 0.05, 'row33: onset should increase LedFx sparkle intensity');
    assert(ledHit.some(function(_, i) { return i % 3 === 1 && ledHit[i] < 0.05; }), 'row33: LedFx sparkle missing low saturation patch');
    markCase('row33.melt-sparkle-hit');

    assert(workerHPeakV(ledNoOnset) <= workerHPeakV(ledHit), 'row33: sustained high without onset should not spawn stronger sparkle');
    markCase('row33.melt-sparkle-no-onset');

    var ledDecoy = loadScript('audiomeltsparkle.js', 0x1734);
    ledDecoy.setMode('LedFx Melt and Sparkle');
    ledDecoy.setBgBright(0);
    ledDecoy.setStrobeBlur(0);
    ledDecoy.setStrobeThreshold(0.75);
    var noSpectral = render(ledDecoy, 'audiomeltsparkle.js', 12, 1, workerHFrame({
        low: 0.3,
        mid: 0.3,
        high: 1.0,
        events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } },
        banks: { full: { count: 12, processed: new Array(12).fill(0.01), novelty: new Array(12).fill(1.0) } }
    }), 'worker-h row33 led spectral miss');
    assert(workerHPeakV(noSpectral) < 0.9, 'row33: LedFx should reject scalar-high decoy when upper processed segment is low');
    markCase('row33.melt-sparkle-spectral-threshold');

    var artistic = loadScript('audiomeltsparkle.js', 0x1734);
    artistic.setBgBright(0);
    artistic.setStrobeBlur(0);
    artistic.setStrobeThreshold(0.75);
    var artHit = render(artistic, 'audiomeltsparkle.js', 12, 1, workerHFrame({
        low: 0.3,
        mid: 0.3,
        high: 1.0,
        events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } },
        banks: { full: { count: 12, processed: new Array(12).fill(0.01), novelty: new Array(12).fill(1.0) } }
    }), 'worker-h row33 artistic scalar hit');
    assert(workerHPeakV(artHit) > workerHPeakV(noSpectral), 'row33: artistic scalar threshold behavior changed');
    markCase('row33.melt-sparkle-artistic-threshold');
}

function workerHAssertRow61Water() {
    var a = loadScript('audiowater.js', 0x1761);
    var b = loadScript('audiowater.js', 0x1761);
    a.setMode('LedFx Water');
    b.setMode('LedFx Water');
    a.setSpeed(1.2);
    b.setSpeed(1.8);
    var baseFrame = workerHFrame({
        low: 0.2,
        mid: 0.2,
        high: 0.2,
        banks: { full: { count: 12, processed: [1,1,0,0,0,0,0,0,0,0,1,1], novelty: new Array(12).fill(0) } }
    });
    var mapA = render(a, 'audiowater.js', 10, 1, baseFrame, 'worker-h row61 speed1.2');
    var mapB = render(b, 'audiowater.js', 10, 1, baseFrame, 'worker-h row61 speed1.8');
    assert.deepStrictEqual(mapA, mapB, 'row61: speed floor expected 1 ripple iteration for 1.2 and 1.8');
    markCase('row61.water-fractional-speed-floor');

    var pOnlyA = loadScript('audiowater.js', 0x1762);
    var pOnlyB = loadScript('audiowater.js', 0x1762);
    pOnlyA.setMode('LedFx Water');
    pOnlyB.setMode('LedFx Water');
    var pA = render(pOnlyA, 'audiowater.js', 10, 1, workerHFrame({
        low: 0.1,
        mid: 0.1,
        high: 0.1,
        banks: { full: { count: 12, processed: [1,1,0,0,0,0,0,0,0,0,0,0], novelty: new Array(12).fill(0) } }
    }), 'worker-h row61 p low');
    var pB = render(pOnlyB, 'audiowater.js', 10, 1, workerHFrame({
        low: 0.1,
        mid: 0.1,
        high: 0.1,
        banks: { full: { count: 12, processed: [0,0,0,0,0,0,0,0,0,0,1,1], novelty: new Array(12).fill(0) } }
    }), 'worker-h row61 p high');
    assert.notDeepStrictEqual(pA, pB, 'row61: processed thirds should drive emitters with fixed scalar decoy');
    markCase('row61.water-processed-thirds');

    var pre = loadScript('audiowater.js', 0x1763);
    pre.setMode('LedFx Water');
    pre.setSpeed(2.2);
    render(pre, 'audiowater.js', 10, 1, workerHFrame({
        low: 0.4,
        mid: 0.3,
        high: 0.2,
        banks: { full: { count: 12, processed: [1,1,0.5,0.5,0,0,0,0,0,0,0.8,0.8], novelty: new Array(12).fill(0) } }
    }), 'worker-h row61 seed');
    var post = render(pre, 'audiowater.js', 10, 1, workerHFrame({
        low: 0.4,
        mid: 0.3,
        high: 0.2,
        banks: { full: { count: 12, processed: [1,1,0.5,0.5,0,0,0,0,0,0,0.8,0.8], novelty: new Array(12).fill(0) } }
    }), 'worker-h row61 post');
    assert(workerHPeakV(post) > 0.05, 'row61: post-propagation insert should retain visible drops');
    markCase('row61.water-propagate-before-insert');

    var tiny = loadScript('audiowater.js', 0x1764);
    tiny.setMode('LedFx Water');
    var tinyMap = render(tiny, 'audiowater.js', 2, 2, workerHFrame({
        low: 0.8,
        mid: 0.8,
        high: 0.8,
        banks: { full: { count: 12, processed: new Array(12).fill(1), novelty: new Array(12).fill(0) } }
    }), 'worker-h row61 tiny');
    assert(tinyMap.every(Number.isFinite), 'row61: tiny-N baseline should stay bounded');
    markCase('row61.water-small-n-guard');
}

    workerHAssertRow07Blocks();
    workerHAssertRow11Crawler();
    workerHAssertRow19Fire();
    workerHAssertRow24Glitch();
    workerHAssertRow29Lava();
    workerHAssertRow32Melt();
    workerHAssertRow33MeltSparkle();
    workerHAssertRow61Water();
}

function assertWorkerIScanRows() {
    function hsvToRgb(h, s, v) {
        var i = Math.floor(h * 6);
        var f = h * 6 - i;
        var p = v * (1 - s);
        var q = v * (1 - f * s);
        var t = v * (1 - (1 - f) * s);
        switch (i % 6) {
        case 0: return [v, t, p];
        case 1: return [q, v, p];
        case 2: return [p, v, t];
        case 3: return [p, q, v];
        case 4: return [t, p, v];
        default: return [v, p, q];
        }
    }

    function rgbAt(map, index) {
        var o = index * 3;
        return hsvToRgb(map[o], map[o + 1], map[o + 2]);
    }

    function vAt(map, index) {
        return map[index * 3 + 2];
    }

    function litStarts(map, threshold) {
        var result = [];
        var on = false;
        for (var i = 0; i < map.length / 3; i++) {
            var lit = vAt(map, i) > threshold;
            if (lit && !on) result.push(i);
            on = lit;
        }
        return result;
    }

    function frame(overrides) {
        return audio(Object.assign({
            version: 6,
            bpm: 120,
            dt: 0.2,
            timing: { deltaSeconds: 0.1 },
            sourceId: "worker-i",
            profileId: 91,
            sourceEpoch: 1,
            configRevision: 1,
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
            banks: { full: { count: 12, processed: new Array(12).fill(0), novelty: new Array(12).fill(0) } }
        }, overrides || {}));
    }

    {
        var scan = loadScript("audioscan.js", 0x4810);
        scan.setMode("LedFx Scan");
        scan.setSpeed(50);
        scan.setMultiplier(1);
        scan.setScanWidth(30);
        scan.setCount(1);
        scan.setFrequencyRange("Lows (beat+bass)");
        scan.setUseGrad("No");
        scan.setFullGrad("No");
        scan.setColorIntensity("Yes");
        scan.setBounce("No");
        scan.setBlur(0);
        var first = render(scan, "audioscan.js", 20, 1, frame({ beat: 0.5, bass: 0.5 }), "row48 first");
        assert.strictEqual(litStarts(first, 0.05)[0], 1, "row48: speed/power step should move one cell");
        var wrapped = render(scan, "audioscan.js", 20, 1, frame({ beat: 0.5, bass: 0.5, timing: { deltaSeconds: 2.0 }, dt: 4.0 }), "row48 wrapped");
        assert.strictEqual(litStarts(wrapped, 0.05)[0], 1, "row48: wrap should preserve modulo position");

        var bounce = loadScript("audioscan.js", 0x4811);
        bounce.setMode("LedFx Scan");
        bounce.setSpeed(50);
        bounce.setMultiplier(1);
        bounce.setScanWidth(30);
        bounce.setCount(1);
        bounce.setBounce("Yes");
        bounce.setFrequencyRange("Lows (beat+bass)");
        bounce.setBlur(0);
        var atEdge = render(bounce, "audioscan.js", 20, 1, frame({ beat: 0.5, bass: 0.5, timing: { deltaSeconds: 2.0 }, dt: 4.0 }), "row48 edge");
        var back = render(bounce, "audioscan.js", 20, 1, frame({ beat: 0.5, bass: 0.5 }), "row48 return");
        assert(litStarts(back, 0.05)[0] < litStarts(atEdge, 0.05)[0], "row48: bounce must reverse direction");

        var count2 = loadScript("audioscan.js", 0x4812);
        count2.setMode("LedFx Scan");
        count2.setCount(2);
        count2.setSpeed(0);
        count2.setMultiplier(1);
        count2.setScanWidth(30);
        count2.setColorIntensity("No");
        count2.setBlur(0);
        var dual = render(count2, "audioscan.js", 20, 1, frame({ beat: 0.5, bass: 0.5 }), "row48 count2");
        assert.deepStrictEqual(litStarts(dual, 0.05), [0, 10], "row48: count=2 should render two windows");

        var gradOff = loadScript("audioscan.js", 0x4813);
        gradOff.setMode("LedFx Scan");
        gradOff.setUseGrad("No");
        gradOff.setSpeed(50);
        gradOff.setMultiplier(1);
        gradOff.setColorIntensity("No");
        gradOff.setBlur(0);
        var offA = render(gradOff, "audioscan.js", 20, 1, frame({ beat: 0.5, bass: 0.5 }), "row48 grad off A");
        var offB = render(gradOff, "audioscan.js", 20, 1, frame({ beat: 0.5, bass: 0.5 }), "row48 grad off B");
        var offIdxA = litStarts(offA, 0.05)[0];
        var offIdxB = litStarts(offB, 0.05)[0];
        assert(Math.abs(offA[offIdxA * 3] - offB[offIdxB * 3]) < 1e-6,
            "row48: fixed scan color should not sample gradient");

        var gradOn = loadScript("audioscan.js", 0x4814);
        gradOn.setMode("LedFx Scan");
        gradOn.setUseGrad("Yes");
        gradOn.setSpeed(50);
        gradOn.setMultiplier(1);
        gradOn.setColorIntensity("No");
        gradOn.setBlur(0);
        var onA = render(gradOn, "audioscan.js", 20, 1, frame({ beat: 0.5, bass: 0.5 }), "row48 grad on A");
        var onB = render(gradOn, "audioscan.js", 20, 1, frame({ beat: 0.5, bass: 0.5 }), "row48 grad on B");
        var onIdxA = litStarts(onA, 0.05)[0];
        var onIdxB = litStarts(onB, 0.05)[0];
        assert(Math.abs(onA[onIdxA * 3] - onB[onIdxB * 3]) > 1e-3,
            "row48: gradient scan color must sample moving position");

        var fullOff = loadScript("audioscan.js", 0x4815);
        fullOff.setMode("LedFx Scan");
        fullOff.setFullGrad("Yes");
        fullOff.setFullGradMod("Off");
        fullOff.setColorIntensity("Yes");
        fullOff.setBlur(0);
        var fullA = render(fullOff, "audioscan.js", 20, 1, frame({ beat: 0.3, bass: 0.3 }), "row48 full off");
        var fullSine = loadScript("audioscan.js", 0x4815);
        fullSine.setMode("LedFx Scan");
        fullSine.setFullGrad("Yes");
        fullSine.setFullGradMod("Sine");
        fullSine.setColorIntensity("Yes");
        fullSine.setBlur(0);
        var fullB = render(fullSine, "audioscan.js", 20, 1, frame({ beat: 0.3, bass: 0.3 }), "row48 full sine");
        assert.notDeepStrictEqual(fullA, fullB, "row48: full gradient modulation should alter output");

        var noIntensity = loadScript("audioscan.js", 0x4816);
        noIntensity.setMode("LedFx Scan");
        noIntensity.setColorIntensity("No");
        noIntensity.setSpeed(0);
        noIntensity.setMultiplier(1);
        noIntensity.setBlur(0);
        var zero = render(noIntensity, "audioscan.js", 20, 1, frame({ beat: 0, bass: 0 }), "row48 zero power");
        assert(zero.some(function(v, i) { return i % 3 === 2 && v > 0.05; }),
            "row48: zero power with Color Intensity off must keep color");

        var xOff = loadScript("audioscan.js", 0x4817);
        xOff.setMode("LedFx Scan");
        xOff.setSpeed(0);
        xOff.setMultiplier(1);
        xOff.setColorIntensity("No");
        xOff.setBlur(0);
        var base = render(xOff, "audioscan.js", 6, 1, frame({ beat: 0.5, bass: 0.5 }), "row48 x base");
        var xFlip = loadScript("audioscan.js", 0x4817);
        xFlip.setMode("LedFx Scan");
        xFlip.setSpeed(0);
        xFlip.setMultiplier(1);
        xFlip.setColorIntensity("No");
        xFlip.setFlip("On");
        xFlip.setBlur(0);
        var flipped = render(xFlip, "audioscan.js", 6, 1, frame({ beat: 0.5, bass: 0.5 }), "row48 x flip");
        for (var i = 0; i < 6; i++) {
            var a = i * 3;
            var b = (5 - i) * 3;
            assert(Math.abs(base[a] - flipped[b]) < 1e-6 &&
                   Math.abs(base[a + 1] - flipped[b + 1]) < 1e-6 &&
                   Math.abs(base[a + 2] - flipped[b + 2]) < 1e-6,
                "row48: flip transform must reverse strip");
        }
        markCase("row48.scan-wrap-bounce-width-input-g-x");
    }

    {
        var flare = loadScript("audioscanflare.js", 0x4910);
        flare.setMode("LedFx Scan and Flare");
        flare.setSpeed(50);
        flare.setMultiplier(100);
        flare.setWidth(20);
        flare.setSparkleThreshold(10);
        flare.setMaxSparkles(3);
        flare.setSparkleSize(20);
        flare.setSparkleTime(400);
        flare.setColorIntensity("No");
        flare.setUseGradient("No");
        flare.setFrequencyRange("Lows (beat+bass)");
        flare.setBounce("No");
        var f1 = render(flare, "audioscanflare.js", 20, 1, frame({ beat: 1, bass: 1 }), "row49 spawn");
        var f2 = render(flare, "audioscanflare.js", 20, 1, frame({ beat: 1, bass: 1 }), "row49 move");
        assert.notDeepStrictEqual(f1, f2, "row49: flare particles should move with constant power");
        var whiteish = 0;
        for (var i = 0; i < 20; i++) {
            var o = i * 3;
            if (f2[o + 2] > 0.05 && f2[o + 1] < 0.2) whiteish++;
        }
        assert(whiteish > 0 && whiteish < 20, "row49: flares must be local particles, not global flash");
        var fade1 = render(flare, "audioscanflare.js", 20, 1, frame({ beat: 0, bass: 0, timing: { deltaSeconds: 0.2 }, dt: 0.4 }), "row49 fade1");
        var fade2 = render(flare, "audioscanflare.js", 20, 1, frame({ beat: 0, bass: 0, timing: { deltaSeconds: 0.2 }, dt: 0.4 }), "row49 fade2");
        assert(fade2.reduce(function(m, v, idx) { return idx % 3 === 2 ? Math.max(m, v) : m; }, 0) <
               fade1.reduce(function(m, v, idx) { return idx % 3 === 2 ? Math.max(m, v) : m; }, 0),
            "row49: flare lifespan should decay and expire when below threshold");

        var bounceFlare = loadScript("audioscanflare.js", 0x4911);
        bounceFlare.setMode("LedFx Scan and Flare");
        bounceFlare.setBounce("Yes");
        bounceFlare.setSpeed(50);
        bounceFlare.setMultiplier(100);
        bounceFlare.setWidth(25);
        bounceFlare.setSparkleThreshold(10);
        bounceFlare.setMaxSparkles(1);
        bounceFlare.setSparkleSize(20);
        bounceFlare.setSparkleTime(800);
        bounceFlare.setColorIntensity("No");
        var edge = render(bounceFlare, "audioscanflare.js", 20, 1, frame({ beat: 1, bass: 1, timing: { deltaSeconds: 1.5 }, dt: 3.0 }), "row49 edge");
        var b1 = render(bounceFlare, "audioscanflare.js", 20, 1, frame({ beat: 1, bass: 1 }), "row49 back1");
        var b2 = render(bounceFlare, "audioscanflare.js", 20, 1, frame({ beat: 1, bass: 1 }), "row49 back2");
        assert(litStarts(b2, 0.05)[0] <= litStarts(b1, 0.05)[0] &&
               litStarts(b1, 0.05)[0] <= litStarts(edge, 0.05)[0],
            "row49: bounce return must reverse scan direction");
        markCase("row49.scanflare-moving-particles-rgboverlap");
    }

    {
        var multi = loadScript("audioscanmulti.js", 0x5010);
        multi.setMode("LedFx Scan Multi");
        multi.setSourceMode("Power");
        multi.setFilter("Off");
        multi.setGradient("No");
        multi.setBounce("No");
        multi.setWidth(20);
        multi.setSpeed(50);
        multi.setMultiplier(100);
        var haltA = render(multi, "audioscanmulti.js", 20, 1, frame({ low: 0, mid: 1, high: 1 }), "row50 halt A");
        var haltB = render(multi, "audioscanmulti.js", 20, 1, frame({ low: 0, mid: 1, high: 1 }), "row50 halt B");
        assert.deepStrictEqual(haltA, haltB, "row50: low=0 must halt cumulative downstream scanners");
        var moveA = render(multi, "audioscanmulti.js", 20, 1, frame({ low: 0.8, mid: 1, high: 1 }), "row50 move A");
        var moveB = render(multi, "audioscanmulti.js", 20, 1, frame({ low: 0.8, mid: 1, high: 1 }), "row50 move B");
        assert.notDeepStrictEqual(moveA, moveB, "row50: non-zero low must move cumulative scanners");

        var mel = loadScript("audioscanmulti.js", 0x5011);
        mel.setMode("LedFx Scan Multi");
        mel.setSourceMode("Melbank");
        mel.setMelbank("Processed");
        mel.setFilter("Off");
        mel.setGradient("No");
        var mA = render(mel, "audioscanmulti.js", 20, 1, frame({
            low: 0.4, mid: 0.4, high: 0.4,
            banks: { full: { count: 10, processed: [1, 1, 0, 0, 0, 0, 0.3, 0.3, 0.3, 0.3], novelty: new Array(10).fill(0) } }
        }), "row50 melbank A");
        var mB = render(mel, "audioscanmulti.js", 20, 1, frame({
            low: 0.4, mid: 0.4, high: 0.4,
            banks: { full: { count: 10, processed: [0, 0, 0, 0, 1, 1, 0.3, 0.3, 0.3, 0.3], novelty: new Array(10).fill(0) } }
        }), "row50 melbank B");
        assert.notDeepStrictEqual(mA, mB, "row50: Melbank mode must use P thirds, not scalar S");

        var filterOff = loadScript("audioscanmulti.js", 0x5012);
        var filterOn = loadScript("audioscanmulti.js", 0x5012);
        filterOff.setMode("LedFx Scan Multi");
        filterOn.setMode("LedFx Scan Multi");
        filterOff.setFilter("Off");
        filterOn.setFilter("On");
        var step1 = frame({ low: 1, mid: 0.2, high: 0.1 });
        render(filterOff, "audioscanmulti.js", 20, 1, step1, "row50 filter off seed");
        render(filterOn, "audioscanmulti.js", 20, 1, step1, "row50 filter on seed");
        var offOut = render(filterOff, "audioscanmulti.js", 20, 1, frame({ low: 0, mid: 0, high: 0 }), "row50 filter off");
        var onOut = render(filterOn, "audioscanmulti.js", 20, 1, frame({ low: 0, mid: 0, high: 0 }), "row50 filter on");
        assert.notDeepStrictEqual(offOut, onOut, "row50: filter On/Off must produce different decay response");

        var overlap = loadScript("audioscanmulti.js", 0x5013);
        overlap.setMode("LedFx Scan Multi");
        overlap.setFilter("Off");
        overlap.setGradient("No");
        overlap.setWidth(60);
        overlap.setSpeed(0);
        overlap.setColorIntensity("No");
        overlap.colors = [
            { h: 0, s: 1, v: 1 },
            { h: 1 / 3, s: 1, v: 1 },
            { h: 2 / 3, s: 1, v: 1 }
        ];
        var ov = render(overlap, "audioscanmulti.js", 12, 1, frame({ low: 1, mid: 1, high: 1 }), "row50 overlap");
        var mixed = false;
        for (var i = 0; i < 12; i++) {
            var rgb = rgbAt(ov, i);
            if (rgb[0] > 0.2 && rgb[1] > 0.2) {
                mixed = true;
                break;
            }
        }
        assert(mixed, "row50: overlapping scanners must blend RGB channels");
        markCase("row50.scanmulti-band-speeds-melbank-cumulative-filter");
    }

    {
        var power = loadScript("audiopower.js", 0x4210);
        power.setMode("LedFx Power");
        power.setFrequencyRange("Lows (beat+bass)");
        power.setSparksColor("#0000ff");
        power.setBassDecayRate(1);
        power.setSparksDecayRate(0);
        power.colors = [
            { h: 0, s: 1, v: 1 },
            { h: 0, s: 1, v: 1 },
            { h: 0, s: 1, v: 1 },
            { h: 0, s: 1, v: 1 },
            { h: 0, s: 1, v: 1 }
        ];
        var hit = render(power, "audiopower.js", 12, 1, frame({
            low: 1,
            banks: { full: { count: 12, processed: new Array(12).fill(0), novelty: new Array(12).fill(1) } },
            events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } },
            onset: false
        }), "row42 hit");
        var magenta = false;
        for (var i = 0; i < 12; i++) {
            var rgb = rgbAt(hit, i);
            if (rgb[0] > 0.2 && rgb[2] > 0.2) {
                magenta = true;
                break;
            }
        }
        assert(magenta, "row42: sparks and background must combine in RGB");

        var duplicate = render(power, "audiopower.js", 12, 1, frame({
            low: 1,
            timing: { deltaSeconds: 0 },
            dt: 0,
            banks: { full: { count: 12, processed: new Array(12).fill(0), novelty: new Array(12).fill(1) } },
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
            onset: true
        }), "row42 duplicate");
        assert.deepStrictEqual(duplicate, hit, "row42: duplicate boolean onset must not retrigger without delta");

        var decay = render(power, "audiopower.js", 12, 1, frame({
            low: 0,
            banks: { full: { count: 12, processed: new Array(12).fill(0), novelty: new Array(12).fill(0) } },
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
        }), "row42 decay");
        assert(decay.some(function(v, i) { return i % 3 === 2 && v > 0.2; }),
            "row42: spark ownership should persist independently from bass decay");

        var noveltyA = render(power, "audiopower.js", 12, 1, frame({
            low: 0.2,
            banks: { full: { count: 12, processed: new Array(12).fill(0), novelty: [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] } }
        }), "row42 novelty A");
        var noveltyB = render(power, "audiopower.js", 12, 1, frame({
            low: 0.2,
            banks: { full: { count: 12, processed: new Array(12).fill(1), novelty: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1] } }
        }), "row42 novelty B");
        assert.notDeepStrictEqual(noveltyA, noveltyB, "row42: background must follow novelty F placement");

        var tiny = loadScript("audiopower.js", 0x4211);
        tiny.setMode("LedFx Power");
        tiny.setSparksColor("#00ffff");
        var tinyMap = render(tiny, "audiopower.js", 4, 1, frame({
            events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } },
            banks: { full: { count: 4, processed: [0, 0, 0, 0], novelty: [0, 0, 0, 0] } }
        }), "row42 tiny");
        assert(tinyMap.some(function(v, i) { return i % 3 === 2 && v > 0; }),
            "row42: tiny layouts must still spawn at least one spark");
        markCase("row42.power-source-components-and-events");
    }

    {
        function rainProcessed(low, mid, high) {
            return [low, low, mid, mid, mid, high, high, high, high, high];
        }

        var expectedHeldLevels = [229, 206, 185, 166];
        var rainBandCases = [
            {
                name: "Lows",
                seed: 0x4410,
                band: "Lows",
                color: "#ff0000",
                colorSetter: "setRainLowsColor",
                sensitivitySetter: "setRainLowsSensitivity",
                selectedChannel: 0,
                processed: rainProcessed(1, 0, 0)
            },
            {
                name: "Mids",
                seed: 0x4411,
                band: "Mids",
                color: "#00ff00",
                colorSetter: "setRainMidsColor",
                sensitivitySetter: "setRainMidsSensitivity",
                selectedChannel: 1,
                processed: rainProcessed(0, 0.9, 0)
            },
            {
                name: "Highs",
                seed: 0x4412,
                band: "Highs",
                color: "#0000ff",
                colorSetter: "setRainHighsColor",
                sensitivitySetter: "setRainHighsSensitivity",
                selectedChannel: 2,
                processed: rainProcessed(0, 0, 1)
            }
        ];
        var firstHitByBand = {};

        for (var bandIndex = 0; bandIndex < rainBandCases.length; bandIndex++) {
            var bandCase = rainBandCases[bandIndex];
            var pulse = loadScript("audiopuddles.js", bandCase.seed);
            pulse.setMode("Rain Pulse");
            pulse.setRainBand(bandCase.band);
            pulse[bandCase.sensitivitySetter](0.05);
            pulse[bandCase.colorSetter](bandCase.color);
            render(pulse, "audiopuddles.js", 10, 1, frame({
                timing: { deltaSeconds: 0.02 }, dt: 0.04,
                banks: { full: { count: 10, processed: rainProcessed(0, 0, 0), novelty: rainProcessed(0, 0, 0) } }
            }), "row44 " + bandCase.name + " base");

            var heldMaps = [];
            for (var step = 0; step < expectedHeldLevels.length; step++) {
                heldMaps.push(render(pulse, "audiopuddles.js", 10, 1, frame({
                    timing: { deltaSeconds: 0.02 }, dt: 0.04,
                    banks: { full: { count: 10, processed: bandCase.processed, novelty: bandCase.processed } }
                }), "row44 " + bandCase.name + " held " + (step + 1)));
            }
            firstHitByBand[bandCase.band] = heldMaps[0];

            var heldLevels = heldMaps.map(function(map) {
                return Math.round(vAt(map, 0) * 255);
            });
            assert.deepStrictEqual(heldLevels, expectedHeldLevels,
                "row44: " + bandCase.name + " held levels must decay after first pulse");

            var hitRgb = rgbAt(heldMaps[0], 0);
            for (var channel = 0; channel < 3; channel++) {
                if (channel === bandCase.selectedChannel)
                    assert(hitRgb[channel] > 0.7, "row44: " + bandCase.name + " selected channel should dominate");
                else
                    assert(hitRgb[channel] < 0.2, "row44: " + bandCase.name + " unselected channels should stay dim");
            }

            var zeroRepeat = render(pulse, "audiopuddles.js", 10, 1, frame({
                timing: { deltaSeconds: 0 }, dt: 0,
                banks: { full: { count: 10, processed: bandCase.processed, novelty: bandCase.processed } }
            }), "row44 " + bandCase.name + " zero repeat");
            assert.deepStrictEqual(zeroRepeat, heldMaps[heldMaps.length - 1],
                "row44: " + bandCase.name + " zero-time repeat must not decay or retrigger");

            var fall = render(pulse, "audiopuddles.js", 10, 1, frame({
                timing: { deltaSeconds: 0.02 }, dt: 0.04,
                banks: { full: { count: 10, processed: rainProcessed(0, 0, 0), novelty: rainProcessed(0, 0, 0) } }
            }), "row44 " + bandCase.name + " fall");
            assert.strictEqual(Math.round(vAt(fall, 0) * 255), 149,
                "row44: " + bandCase.name + " fall frame should keep decaying");

            var retrigger = render(pulse, "audiopuddles.js", 10, 1, frame({
                timing: { deltaSeconds: 0.02 }, dt: 0.04,
                banks: { full: { count: 10, processed: bandCase.processed, novelty: bandCase.processed } }
            }), "row44 " + bandCase.name + " retrigger");
            assert.strictEqual(Math.round(vAt(retrigger, 0) * 255), 229,
                "row44: " + bandCase.name + " should retrigger after decay");
        }
        var midHit = firstHitByBand.Mids;

        var decoy = loadScript("audiopuddles.js", 0x4413);
        decoy.setMode("Rain Pulse");
        decoy.setRainBand("Mids");
        decoy.setRainMidsSensitivity(0.05);
        render(decoy, "audiopuddles.js", 10, 1, frame({
            timing: { deltaSeconds: 0.02 }, dt: 0.04,
            banks: { full: { count: 10, processed: rainProcessed(0, 0, 0), novelty: rainProcessed(0, 0, 0) } }
        }), "row44 decoy base");
        var highOnly = render(decoy, "audiopuddles.js", 10, 1, frame({
            timing: { deltaSeconds: 0.02 }, dt: 0.04,
            banks: { full: { count: 10, processed: rainProcessed(0, 0, 1), novelty: rainProcessed(0, 0, 1) } }
        }), "row44 decoy high");
        assert(highOnly.every(function(v, idx) { return idx % 3 !== 2 || v < 1e-6; }),
            "row44: unselected highs must not trigger mids pulse");

        var flatA = loadScript("audiopuddles.js", 0x4414);
        flatA.setMode("Rain Pulse");
        flatA.setRainBand("Mids");
        flatA.setRainMidsColor("#00ff00");
        render(flatA, "audiopuddles.js", 10, 1, frame({
            timing: { deltaSeconds: 0.02 }, dt: 0.04,
            banks: { full: { count: 10, processed: rainProcessed(0, 0.9, 0), novelty: rainProcessed(0, 0.9, 0) } }
        }), "row44 flat 10x1");
        var flatB = loadScript("audiopuddles.js", 0x4414);
        flatB.setMode("Rain Pulse");
        flatB.setRainBand("Mids");
        flatB.setRainMidsColor("#00ff00");
        var sameArea = render(flatB, "audiopuddles.js", 5, 2, frame({
            timing: { deltaSeconds: 0.02 }, dt: 0.04,
            banks: { full: { count: 10, processed: rainProcessed(0, 0.9, 0), novelty: rainProcessed(0, 0.9, 0) } }
        }), "row44 flat 5x2");
        assert.strictEqual(sameArea.length, 30, "row44: flattened output length mismatch");
        assert.deepStrictEqual(midHit, sameArea, "row44: flattened 1xN and Nx1 outputs should match");
        markCase("row44.rain-pulse-enum-colors-regions-scaling");
    }
}

function assertWorkerJMatrixRows() {
    const EPS = 1e-6;
    const utilSandbox = { Math, Float32Array };
    vm.createContext(utilSandbox);
    vm.runInContext(HSV_UTIL, utilSandbox, { filename: 'hsvutil.js' });
    const HSV = utilSandbox.HSVUtil;

    function approx(a, b, label, eps = EPS) {
        assert(Math.abs(a - b) <= eps, `${label}: expected ${b}, got ${a}`);
    }

    function approxMap(actual, expected, label, eps = EPS) {
        assert.strictEqual(actual.length, expected.length, `${label}: length mismatch`);
        for (let i = 0; i < actual.length; i++)
            approx(actual[i], expected[i], `${label}[${i}]`, eps);
    }

    function frame(overrides = {}) {
        return audio(Object.assign({
            version: 6,
            sourceId: 'worker-j-source',
            profileId: 91,
            sourceEpoch: 1,
            configRevision: 1,
            dt: 0,
            timing: { deltaSeconds: 0 },
            banks: { full: { count: 0, novelty: [], processed: [] } },
            powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } },
            events: { delta: { beat: 0, onset: 0, kick: 0, bar: 0 } }
        }, overrides));
    }

    function splitSizes(total, count) {
        const active = Math.max(1, Math.min(count, Math.max(1, total)));
        const base = Math.floor(total / active);
        const extra = total - base * active;
        return new Array(active).fill(0).map((_, i) => base + (i < extra ? 1 : 0));
    }

    function paintMask(mask, start, size, volume, align) {
        const lit = Math.max(0, Math.min(size, volume));
        if (!lit) return;
        if (align === 'Right') {
            for (let i = size - lit; i < size; i++) mask[start + i] = 1;
            return;
        }
        let shift = 0;
        if (align === 'Center') shift = Math.floor((size - lit) / 2);
        else if (align === 'Invert') shift = -Math.floor(lit / 2);
        for (let i = 0; i < lit; i++) {
            let pos = (i + shift) % size;
            if (pos < 0) pos += size;
            mask[start + pos] = 1;
        }
    }

    function expectedEqualizer(width, height, novelty, count, align, stops, transforms) {
        const n = width * height;
        const bins = HSV.interpolate(novelty, n).map(v => HSV.clamp01(Number.isFinite(v) ? v : 0));
        const mask = new Array(n).fill(0);
        const sizes = splitSizes(n, count);
        let start = 0;
        for (const size of sizes) {
            let sum = 0;
            for (let i = 0; i < size; i++) sum += bins[start + i];
            const lit = Math.floor((sum / size) * size);
            paintMask(mask, start, size, lit, align);
            start += size;
        }
        const map = Array.from(HSV.createMap(width, height));
        for (let p = 0; p < n; p++) {
            if (!mask[p]) continue;
            const hsv = HSV.gradientLedfxAt(stops, n <= 1 ? 0 : p / (n - 1));
            map[p * 3] = hsv.h;
            map[p * 3 + 1] = hsv.s;
            map[p * 3 + 2] = hsv.v;
        }
        if (transforms)
            HSV.applyStripTransforms(map, width, height, transforms);
        return { map, mask, sizes };
    }

    function litIndicesInRange(map, start, end) {
        const out = [];
        for (let i = start; i < end; i++) {
            if (map[i * 3 + 2] > 1e-9) out.push(i);
        }
        return out;
    }

    function selectedPower(audioFrame, range) {
        if (range === 'Beat') return audioFrame.beat || 0;
        if (range === 'Bass') return audioFrame.bass || 0;
        if (range === 'Mids') return audioFrame.mid || 0;
        if (range === 'High') return audioFrame.high || 0;
        return audioFrame.low || 0;
    }

    function expectedPlasma2d(width, height, seconds, power, params, stops) {
        const map = Array.from(HSV.createMap(width, height));
        const scale = params.lower + power * params.density;
        const xExtent = Math.min(width, width * scale);
        const yExtent = Math.min(height, height * scale);
        const xStep = width > 1 ? xExtent / (width - 1) : 0;
        const yStep = height > 1 ? yExtent / (height - 1) : 0;
        const values = [];
        let minV = Infinity;
        let maxV = -Infinity;
        for (let y = 0; y < height; y++) {
            const yy = y * yStep;
            for (let x = 0; x < width; x++) {
                const xx = x * xStep;
                const raw =
                    Math.sin(xx * 0.1 + seconds) * Math.cos(yy * 0.1 - seconds) +
                    Math.sin((xx * params.density_vertical + yy * params.twist + seconds) * 2.5) +
                    Math.sin(Math.sqrt(xx * xx + yy * yy) * params.radius - seconds);
                values.push(raw);
                minV = Math.min(minV, raw);
                maxV = Math.max(maxV, raw);
            }
        }
        const span = maxV - minV;
        for (let i = 0; i < values.length; i++) {
            const t = span > 0 ? HSV.clamp01((values[i] - minV) / span) : 0;
            const hsv = HSV.gradientLedfxAt(stops, t);
            map[i * 3] = hsv.h;
            map[i * 3 + 1] = hsv.s;
            map[i * 3 + 2] = hsv.v;
        }
        return map;
    }

    const SIN8 = new Array(256);
    const COS8 = new Array(256);
    for (let i = 0; i < 256; i++) {
        SIN8[i] = Math.max(0, Math.min(255, Math.floor(Math.sin(i * (2 * Math.PI / 255)) * 127.5 + 127.5)));
        COS8[i] = Math.max(0, Math.min(255, Math.floor(Math.cos(i * (2 * Math.PI / 255)) * 127.5 + 127.5)));
    }

    function toUint8(v) {
        if (!Number.isFinite(v)) return 0;
        const t = v < 0 ? Math.ceil(v) : Math.floor(v);
        let wrapped = t % 256;
        if (wrapped < 0) wrapped += 256;
        return wrapped;
    }

    function expectedPlasmaWledStep(width, height, state, audioFrame, params, stops) {
        const power = HSV.clamp01(selectedPower(audioFrame, params.range)) * 2;
        const dtSec = HSV.audioSeconds(audioFrame);
        const sizeb = power * Math.max(0, params.sizeMultiplier);
        const speedb = power * Math.max(0, params.speedMultiplier);
        let a;
        if (params.speedMultiplier > 0) {
            state.motion += speedb * dtSec * 60;
            a = Math.floor(state.motion * 1000) / (Math.max(0.01, params.speedDivisor) + 1);
        } else {
            state.freeSeconds += dtSec;
            a = Math.floor(state.freeSeconds * 1000) / (Math.max(0.01, params.speedDivisor) + 1);
        }
        const hStretch = Math.max(0.01, params.horizontalStretch - (sizeb * params.horizontalStretch / 3));
        const vStretch = Math.max(0.01, params.verticalStretch - (sizeb * params.verticalStretch / 3));
        const map = Array.from(HSV.createMap(width, height));
        for (let y = 0; y < height; y++) {
            for (let x = 0; x < width; x++) {
                const xVal = y * hStretch / 16 + a / 3;
                const yVal = x * vStretch / 16 + a / 4;
                const wave = SIN8[toUint8(COS8[toUint8(xVal)] + SIN8[toUint8(yVal)] + a)] / 255;
                const hsv = HSV.gradientLedfxAt(stops, HSV.clamp01(wave));
                const o = (y * width + x) * 3;
                map[o] = hsv.h;
                map[o + 1] = hsv.s;
                map[o + 2] = hsv.v;
            }
        }
        return map;
    }

    function clone2dRgb(src) {
        return src.map(row => row.map(pixel => [pixel[0], pixel[1], pixel[2]]));
    }

    function makeSoapOracle(seed, width, height) {
        const rand = seededRandom(seed);
        rand();
        rand();
        const state = {
            phaseX: rand() * 256,
            phaseY: rand() * 256,
            noise: new Array(height),
            prev: new Array(height),
            needSeed: true,
            debug: {}
        };
        for (let y = 0; y < height; y++) {
            state.noise[y] = new Array(width).fill(0);
            state.prev[y] = new Array(width).fill(null).map(() => [0, 0, 0]);
        }
        return state;
    }

    function smearRgb(source, palette, amounts, axis) {
        const h = source.length;
        const w = source[0].length;
        const out = new Array(h).fill(null).map(() => new Array(w));
        let usedOob = false;
        let usedInBounds = false;
        let sawNegativeFractional = false;
        if (axis === 1) {
            for (let row = 0; row < h; row++) {
                const amt = amounts[row];
                const sgn = amt > 0 ? 1 : (amt < 0 ? -1 : 0);
                const mag = Math.abs(amt);
                const di = Math.floor(mag);
                const frac = mag - di;
                if (amt < 0 && frac > 0 && frac < 1) sawNegativeFractional = true;
                const wB = frac * frac * (3 - 2 * frac);
                const wA = 1 - wB;
                for (let x = 0; x < w; x++) {
                    const zD = x + sgn * di;
                    const zF = zD + sgn;
                    const inA = zD >= 0 && zD < w;
                    const inB = zF >= 0 && zF < w;
                    const a = inA ? source[row][zD] : palette[row][Math.max(0, Math.min(w - 1, zD))];
                    const b = inB ? source[row][zF] : palette[row][Math.max(0, Math.min(w - 1, zF))];
                    usedInBounds = usedInBounds || inA || inB;
                    usedOob = usedOob || !inA || !inB;
                    out[row][x] = [
                        a[0] * wA + b[0] * wB,
                        a[1] * wA + b[1] * wB,
                        a[2] * wA + b[2] * wB
                    ];
                }
            }
            return { out, usedOob, usedInBounds, sawNegativeFractional };
        }
        for (let col = 0; col < w; col++) {
            const amt = amounts[col];
            const sgn = amt > 0 ? 1 : (amt < 0 ? -1 : 0);
            const mag = Math.abs(amt);
            const di = Math.floor(mag);
            const frac = mag - di;
            if (amt < 0 && frac > 0 && frac < 1) sawNegativeFractional = true;
            const wB = frac * frac * (3 - 2 * frac);
            const wA = 1 - wB;
            for (let y = 0; y < h; y++) {
                const zD = y + sgn * di;
                const zF = zD + sgn;
                const inA = zD >= 0 && zD < h;
                const inB = zF >= 0 && zF < h;
                const a = inA ? source[zD][col] : palette[Math.max(0, Math.min(h - 1, zD))][col];
                const b = inB ? source[zF][col] : palette[Math.max(0, Math.min(h - 1, zF))][col];
                usedInBounds = usedInBounds || inA || inB;
                usedOob = usedOob || !inA || !inB;
                out[y][col] = [
                    a[0] * wA + b[0] * wB,
                    a[1] * wA + b[1] * wB,
                    a[2] * wA + b[2] * wB
                ];
            }
        }
        return { out, usedOob, usedInBounds, sawNegativeFractional };
    }

    function soapOracleStep(state, audioFrame, params, gradient) {
        const width = state.prev[0].length;
        const height = state.prev.length;
        const dtSec = HSV.audioSeconds(audioFrame);
        const power = HSV.clamp01(selectedPower(audioFrame, params.range));
        const impulse = power * 6;
        const audioSpeed = params.intensity === 0
            ? params.speed
            : params.speed * impulse * params.intensity;
        if (dtSec > 0) {
            const move = audioSpeed * audioSpeed * 0.5 * dtSec;
            state.phaseX += move;
            state.phaseY += move;
            const spanX = 6;
            const spanY = 6;
            const stepX = spanX / Math.max(1, width - 1);
            const stepY = spanY / Math.max(1, height - 1);
            const x0 = state.phaseX - spanX * 0.5;
            const y0 = state.phaseY - spanY * 0.5;
            const alpha = 1 - Math.pow(0.5, dtSec * 60);
            for (let y = 0; y < height; y++) {
                const ny = y0 + y * stepY;
                for (let x = 0; x < width; x++) {
                    const nx = x0 + x * stepX;
                    const n = (HSV.simplex2d(nx * 0.3, ny * 0.3) + 1) * 0.5;
                    state.noise[y][x] = state.noise[y][x] * (1 - alpha) + n * alpha;
                }
            }
        }

        const palette = new Array(height);
        for (let y = 0; y < height; y++) {
            palette[y] = new Array(width);
            for (let x = 0; x < width; x++) {
                let palIdx = ((1 - state.noise[y][x]) * 3) % 1;
                if (palIdx < 0) palIdx += 1;
                palette[y][x] = HSV.gradientRgbAt(gradient, palIdx);
            }
        }

        if (state.needSeed) {
            state.prev = clone2dRgb(palette);
            state.needSeed = false;
        }

        if (dtSec <= 0) {
            const still = Array.from(HSV.createMap(width, height));
            for (let y = 0; y < height; y++) {
                for (let x = 0; x < width; x++) {
                    const hsv = HSV.rgbToHsvUnclipped(state.prev[y][x][0], state.prev[y][x][1], state.prev[y][x][2]);
                    const o = (y * width + x) * 3;
                    still[o] = HSV.clamp01(hsv.h);
                    still[o + 1] = HSV.clamp01(hsv.s);
                    still[o + 2] = HSV.clamp01(hsv.v);
                }
            }
            state.debug = { usedOob: false, usedInBounds: false, sawNegativeFractional: false };
            return still;
        }

        const ampFactor = 1 + 7 * params.density;
        const ampX = Math.max(1, (width - 8) / 8) * ampFactor;
        const ampY = Math.max(1, (height - 8) / 8) * ampFactor;
        const amtRows = new Array(height);
        const amtCols = new Array(width);
        for (let y = 0; y < height; y++) amtRows[y] = (state.noise[y][0] - 0.5) * ampX;
        for (let x = 0; x < width; x++) amtCols[x] = (state.noise[0][x] - 0.5) * ampY;

        const rowStep = smearRgb(state.prev, palette, amtRows, 1);
        const colStep = smearRgb(rowStep.out, palette, amtCols, 0);
        state.prev = colStep.out;
        state.debug = {
            usedOob: rowStep.usedOob || colStep.usedOob,
            usedInBounds: rowStep.usedInBounds && colStep.usedInBounds,
            sawNegativeFractional: rowStep.sawNegativeFractional || colStep.sawNegativeFractional
        };

        const map = Array.from(HSV.createMap(width, height));
        for (let y = 0; y < height; y++) {
            for (let x = 0; x < width; x++) {
                const hsv = HSV.rgbToHsvUnclipped(state.prev[y][x][0], state.prev[y][x][1], state.prev[y][x][2]);
                const o = (y * width + x) * 3;
                map[o] = HSV.clamp01(hsv.h);
                map[o + 1] = HSV.clamp01(hsv.s);
                map[o + 2] = HSV.clamp01(hsv.v);
            }
        }
        return map;
    }

    {
        const script = 'audioequalizer.js';
        const novelty = [1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0];
        const decoyProcessed = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1];
        const eq = loadScript(script, 0x1500);
        eq.setMode('Segment Equalizer');
        eq.setSegmentCount(3);
        eq.setSegmentAlign('Left');
        const leftMap = render(eq, script, 12, 1, frame({
            banks: { full: { count: novelty.length, novelty: novelty.slice(), processed: decoyProcessed.slice() } }
        }), 'worker-j row15 left');
        const expectedLeft = expectedEqualizer(12, 1, novelty, 3, 'Left', eq.colors);
        approxMap(leftMap, expectedLeft.map, 'row15/left-exact');
        assert.deepStrictEqual(
            [0, 1, 2].map(i => litIndicesInRange(leftMap, i * 4, i * 4 + 4).length),
            [1, 0, 3],
            'row15/mean-based counts mismatch'
        );
        const globalHue = HSV.gradientLedfxAt(eq.colors, 8 / 11).h;
        approx(leftMap[8 * 3], globalHue, 'row15/global gradient', 1e-5);

        const oddNovelty = [1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0];
        const expectedMiddle = {
            Left: [5, 6, 7],
            Right: [7, 8, 9],
            Center: [6, 7, 8],
            Invert: [5, 6, 9]
        };
        for (const align of Object.keys(expectedMiddle)) {
            const algo = loadScript(script, 0x1510 + align.length);
            algo.setMode('Segment Equalizer');
            algo.setSegmentCount(3);
            algo.setSegmentAlign(align);
            const map = render(algo, script, 15, 1, frame({
                banks: { full: { count: oddNovelty.length, novelty: oddNovelty.slice(), processed: novelty.slice() } }
            }), `worker-j row15 ${align}`);
            approxMap(
                map,
                expectedEqualizer(15, 1, oddNovelty, 3, align, algo.colors).map,
                `row15/${align}-exact`
            );
            assert.deepStrictEqual(
                litIndicesInRange(map, 5, 10),
                expectedMiddle[align],
                `row15/${align} odd-volume alignment`
            );
        }

        const singleBin = loadScript(script, 0x1520);
        singleBin.setMode('Segment Equalizer');
        singleBin.setSegmentCount(16);
        singleBin.setSegmentAlign('Left');
        const singleMap = render(singleBin, script, 5, 1, frame({
            banks: { full: { count: 1, novelty: [1], processed: [0] } }
        }), 'worker-j row15 count-cap-singlebin');
        const singleExpected = expectedEqualizer(5, 1, [1], 16, 'Left', singleBin.colors);
        approxMap(singleMap, singleExpected.map, 'row15/single-bin');
        assert.deepStrictEqual(litIndicesInRange(singleMap, 0, 5), [0, 1, 2, 3, 4], 'row15/count clamp');

        eq.setPresetSegmentFlip('On');
        const flipped = render(eq, script, 12, 1, frame({
            banks: { full: { count: novelty.length, novelty: novelty.slice(), processed: decoyProcessed.slice() } }
        }), 'worker-j row15 flip');
        const flipExpected = expectedEqualizer(12, 1, novelty, 3, 'Left', eq.colors, {
            flip: 'On',
            mirror: 'Off',
            backgroundMode: 'Off',
            backgroundColor: '#000000',
            backgroundBrightness: 1,
            brightness: 1,
            blur: 0
        });
        approxMap(flipped, flipExpected.map, 'row15/x-flip');
        markCase('row15-equalizer-segment');
        console.log('PASS row15-equalizer-segment');
    }

    {
        const script = 'audioplasma.js';
        const plasma = loadScript(script, 0x4001);
        plasma.setMode('Plasma2d');
        plasma.setFrequencyRange('Mids');
        plasma.setDensity(0.5);
        plasma.setLower(0.01);
        plasma.setDensityVertical(0.1);
        plasma.setTwist(0.07);
        plasma.setRadius(0.2);
        plasma.colors = [{ h: 0, s: 0, v: 0 }, { h: 0, s: 0, v: 1 }];
        plasma.color = plasma.colors[0];

        const f = frame({ mid: 0.6, low: 0.1, high: 0.95, timing: { deltaSeconds: 0.25 }, dt: 0.125, bpm: 60 });
        const map = render(plasma, script, 9, 7, f, 'worker-j row40 exact');
        const expected = expectedPlasma2d(
            9,
            7,
            0.25,
            0.6,
            { density: 0.5, lower: 0.01, density_vertical: 0.1, twist: 0.07, radius: 0.2 },
            plasma.colors
        );
        approxMap(map, expected, 'row40/exact');
        const values = [];
        for (let i = 2; i < map.length; i += 3) values.push(map[i]);
        approx(Math.min(...values), 0, 'row40/min endpoint', 1e-5);
        approx(Math.max(...values), 1, 'row40/max endpoint', 1e-5);

        const bpm60 = loadScript(script, 0x4002);
        const bpm120 = loadScript(script, 0x4003);
        bpm60.setMode('Plasma2d');
        bpm120.setMode('Plasma2d');
        bpm60.setFrequencyRange('Mids');
        bpm120.setFrequencyRange('Mids');
        bpm60.colors = plasma.colors;
        bpm120.colors = plasma.colors;
        bpm60.color = bpm60.colors[0];
        bpm120.color = bpm120.colors[0];
        const map60 = render(bpm60, script, 9, 7, frame({ mid: 0.4, timing: { deltaSeconds: 0.2 }, dt: 0.2, bpm: 60 }), 'worker-j row40 bpm60');
        const map120 = render(bpm120, script, 9, 7, frame({ mid: 0.4, timing: { deltaSeconds: 0.2 }, dt: 0.4, bpm: 120 }), 'worker-j row40 bpm120');
        approxMap(map60, map120, 'row40/seconds invariance');

        const twistLow = loadScript(script, 0x4004);
        const radiusLow = loadScript(script, 0x4005);
        twistLow.setMode('Plasma2d');
        radiusLow.setMode('Plasma2d');
        twistLow.setTwist(0.01);
        radiusLow.setTwist(0.25);
        twistLow.setRadius(0.25);
        radiusLow.setRadius(0.01);
        const twistMap = render(twistLow, script, 9, 7, frame({ low: 0.7, timing: { deltaSeconds: 0.2 } }), 'worker-j row40 twist');
        const radiusMap = render(radiusLow, script, 9, 7, frame({ low: 0.7, timing: { deltaSeconds: 0.2 } }), 'worker-j row40 radius');
        assert.notDeepStrictEqual(twistMap, radiusMap, 'row40/twist-radius controls collapsed');

        const s1 = loadScript(script, 0x4006);
        const s2 = loadScript(script, 0x4007);
        s1.setMode('Plasma2d');
        s2.setMode('Plasma2d');
        s1.setFrequencyRange('Beat');
        s2.setFrequencyRange('Beat');
        s1.setSmoothing(1);
        s2.setSmoothing(10);
        const smoothA = render(s1, script, 7, 3, frame({ beat: 0.55, timing: { deltaSeconds: 0.1 } }), 'worker-j row40 smoothA');
        const smoothB = render(s2, script, 7, 3, frame({ beat: 0.55, timing: { deltaSeconds: 0.1 } }), 'worker-j row40 smoothB');
        approxMap(smoothA, smoothB, 'row40/no-local-filter');

        const oneByOne = loadScript(script, 0x4008);
        oneByOne.setMode('Plasma2d');
        oneByOne.setDensity(0);
        oneByOne.setLower(0);
        oneByOne.colors = [{ h: 0.33, s: 0.8, v: 0.7 }];
        oneByOne.color = oneByOne.colors[0];
        const single = render(oneByOne, script, 1, 1, frame({ low: 0.9, timing: { deltaSeconds: 0.2 } }), 'worker-j row40 1x1');
        approx(single[0], 0.33, 'row40/1x1 hue');
        approx(single[1], 0.8, 'row40/1x1 sat');
        approx(single[2], 0.7, 'row40/1x1 value');

        const identityCarry = loadScript(script, 0x4009);
        const identityControl = loadScript(script, 0x400A);
        identityCarry.setMode('Plasma2d');
        identityControl.setMode('Plasma2d');
        identityCarry.setFrequencyRange('Mids');
        identityControl.setFrequencyRange('Mids');
        identityCarry.colors = plasma.colors;
        identityControl.colors = plasma.colors;
        identityCarry.color = identityCarry.colors[0];
        identityControl.color = identityControl.colors[0];
        const stableIdentityFrame = frame({
            mid: 0.5,
            sourceId: 'row40-source',
            profileId: 7,
            sourceEpoch: 9,
            configRevision: 3,
            timing: { deltaSeconds: 0.2 },
            dt: 0.2
        });
        render(identityCarry, script, 9, 7, stableIdentityFrame, 'worker-j row40 identity-carry-1');
        render(identityCarry, script, 9, 7, frame({
            mid: 0.5,
            sourceId: '',
            profileId: 7,
            sourceEpoch: 9,
            configRevision: 3,
            available: false,
            status: 'reset',
            timing: { deltaSeconds: 0 },
            dt: 0
        }), 'worker-j row40 identity-unavailable-gap');
        const carryResume = render(identityCarry, script, 9, 7, stableIdentityFrame, 'worker-j row40 identity-carry-2');
        render(identityControl, script, 9, 7, stableIdentityFrame, 'worker-j row40 identity-control-1');
        const controlResume = render(identityControl, script, 9, 7, stableIdentityFrame, 'worker-j row40 identity-control-2');
        approxMap(carryResume, controlResume, 'row40/identity-helper-unavailable-hold');

        const switchedProfileFrame = frame({
            mid: 0.5,
            sourceId: 'row40-source',
            profileId: 8,
            sourceEpoch: 9,
            configRevision: 3,
            timing: { deltaSeconds: 0.2 },
            dt: 0.2
        });
        const switchedProfileMap = render(identityCarry, script, 9, 7, switchedProfileFrame, 'worker-j row40 identity-profile-switch');
        const freshProfile = loadScript(script, 0x400B);
        freshProfile.setMode('Plasma2d');
        freshProfile.setFrequencyRange('Mids');
        freshProfile.colors = plasma.colors;
        freshProfile.color = freshProfile.colors[0];
        const freshProfileMap = render(freshProfile, script, 9, 7, switchedProfileFrame, 'worker-j row40 identity-profile-fresh');
        approxMap(switchedProfileMap, freshProfileMap, 'row40/identity-helper-profile-reset');
        markCase('row40-plasma2d');
        console.log('PASS row40-plasma2d');
    }

    {
        const script = 'audioplasma.js';
        const seed = 0x4101;
        const algo = loadScript(script, seed);
        algo.setMode('PlasmaWled2d');
        algo.setFrequencyRange('High');
        algo.colors = [{ h: 0, s: 0, v: 0 }, { h: 0, s: 0, v: 1 }];
        algo.color = algo.colors[0];
        algo.setSpeedDivisor(1);
        algo.setHorizontalStretch(255);
        algo.setVerticalStretch(255);
        algo.setSizeMultiplier(0);
        algo.setSpeedMultiplier(0);
        const wledFrame = frame({ high: 0.7, low: 0.05, timing: { deltaSeconds: 0.6 }, dt: 0.1, bpm: 200 });
        const map = render(algo, script, 4, 3, wledFrame, 'worker-j row41 exact');
        const expected = expectedPlasmaWledStep(4, 3, { motion: 0, freeSeconds: 0 }, wledFrame, {
            range: 'High',
            speedDivisor: 1,
            horizontalStretch: 255,
            verticalStretch: 255,
            sizeMultiplier: 0,
            speedMultiplier: 0
        }, algo.colors);
        approxMap(map, expected, 'row41/exact');

        const hold = loadScript(script, 0x4102);
        hold.setMode('PlasmaWled2d');
        hold.setFrequencyRange('High');
        hold.setSpeedMultiplier(1);
        hold.setSizeMultiplier(0);
        const holdA = render(hold, script, 6, 4, frame({ high: 0, timing: { deltaSeconds: 0.3 } }), 'worker-j row41 holdA');
        const holdB = render(hold, script, 6, 4, frame({ high: 0, timing: { deltaSeconds: 0.3 } }), 'worker-j row41 holdB');
        approxMap(holdA, holdB, 'row41/positive speed multiplier holds with zero power');

        const free = loadScript(script, 0x4103);
        free.setMode('PlasmaWled2d');
        free.setSpeedMultiplier(0);
        free.setSizeMultiplier(0);
        const freeA = render(free, script, 6, 4, frame({ low: 0, timing: { deltaSeconds: 0.3 } }), 'worker-j row41 freeA');
        const freeB = render(free, script, 6, 4, frame({ low: 0, timing: { deltaSeconds: 0.3 } }), 'worker-j row41 freeB');
        assert.notDeepStrictEqual(freeA, freeB, 'row41/multiplier0 should free-run');

        const size0 = loadScript(script, 0x4104);
        const size1 = loadScript(script, 0x4105);
        size0.setMode('PlasmaWled2d');
        size1.setMode('PlasmaWled2d');
        size0.setSpeedMultiplier(0);
        size1.setSpeedMultiplier(0);
        size0.setSizeMultiplier(0);
        size1.setSizeMultiplier(1);
        const sizeMap0 = render(size0, script, 7, 3, frame({ low: 0.5, timing: { deltaSeconds: 0.2 } }), 'worker-j row41 size0');
        const sizeMap1 = render(size1, script, 7, 3, frame({ low: 0.5, timing: { deltaSeconds: 0.2 } }), 'worker-j row41 size1');
        assert.notDeepStrictEqual(sizeMap0, sizeMap1, 'row41/size multiplier did not change geometry');

        const speedSlow = loadScript(script, 0x4106);
        const speedFast = loadScript(script, 0x4107);
        speedSlow.setMode('PlasmaWled2d');
        speedFast.setMode('PlasmaWled2d');
        speedSlow.setSizeMultiplier(0);
        speedFast.setSizeMultiplier(0);
        speedSlow.setSpeedMultiplier(0.2);
        speedFast.setSpeedMultiplier(1);
        const speedMapSlow = render(speedSlow, script, 7, 3, frame({ low: 0.6, timing: { deltaSeconds: 0.2 } }), 'worker-j row41 speed slow');
        const speedMapFast = render(speedFast, script, 7, 3, frame({ low: 0.6, timing: { deltaSeconds: 0.2 } }), 'worker-j row41 speed fast');
        assert.notDeepStrictEqual(speedMapSlow, speedMapFast, 'row41/speed multiplier did not change clock');

        const stretchFloor = loadScript(script, 0x4108);
        stretchFloor.setMode('PlasmaWled2d');
        stretchFloor.setHorizontalStretch(1);
        stretchFloor.setVerticalStretch(1);
        stretchFloor.setSizeMultiplier(100);
        stretchFloor.setSpeedMultiplier(1);
        const one = render(stretchFloor, script, 1, 1, frame({ low: 1, timing: { deltaSeconds: 0.4 } }), 'worker-j row41 1x1');
        assert(one.every(v => Number.isFinite(v) && v >= 0 && v <= 1), 'row41/1x1 lower-stretch bound failed');

        const identityCarry = loadScript(script, 0x4109);
        const identityControl = loadScript(script, 0x410A);
        identityCarry.setMode('PlasmaWled2d');
        identityControl.setMode('PlasmaWled2d');
        identityCarry.setFrequencyRange('High');
        identityControl.setFrequencyRange('High');
        identityCarry.setSpeedMultiplier(1);
        identityControl.setSpeedMultiplier(1);
        identityCarry.setSizeMultiplier(0);
        identityControl.setSizeMultiplier(0);
        const stableIdentityFrame = frame({
            high: 0.6,
            sourceId: 'row41-source',
            profileId: 17,
            sourceEpoch: 2,
            configRevision: 5,
            timing: { deltaSeconds: 0.2 },
            dt: 0.2
        });
        render(identityCarry, script, 6, 4, stableIdentityFrame, 'worker-j row41 identity-carry-1');
        render(identityCarry, script, 6, 4, frame({
            high: 0.6,
            sourceId: '',
            profileId: 17,
            sourceEpoch: 2,
            configRevision: 5,
            available: false,
            status: 'reset',
            timing: { deltaSeconds: 0 },
            dt: 0
        }), 'worker-j row41 identity-unavailable-gap');
        const carryResume = render(identityCarry, script, 6, 4, stableIdentityFrame, 'worker-j row41 identity-carry-2');
        render(identityControl, script, 6, 4, stableIdentityFrame, 'worker-j row41 identity-control-1');
        const controlResume = render(identityControl, script, 6, 4, stableIdentityFrame, 'worker-j row41 identity-control-2');
        approxMap(carryResume, controlResume, 'row41/identity-helper-unavailable-hold');

        const switchedSourceFrame = frame({
            high: 0.6,
            sourceId: 'row41-source-next',
            profileId: 17,
            sourceEpoch: 2,
            configRevision: 5,
            timing: { deltaSeconds: 0.2 },
            dt: 0.2
        });
        const switchedSourceMap = render(identityCarry, script, 6, 4, switchedSourceFrame, 'worker-j row41 identity-source-switch');
        const freshSource = loadScript(script, 0x410B);
        freshSource.setMode('PlasmaWled2d');
        freshSource.setFrequencyRange('High');
        freshSource.setSpeedMultiplier(1);
        freshSource.setSizeMultiplier(0);
        const freshSourceMap = render(freshSource, script, 6, 4, switchedSourceFrame, 'worker-j row41 identity-source-fresh');
        approxMap(switchedSourceMap, freshSourceMap, 'row41/identity-helper-source-reset');
        markCase('row41-plasmawled2d');
        console.log('PASS row41-plasmawled2d');
    }

    {
        const script = 'audiosoap.js';
        const seed = 0x5501;
        const width = 4;
        const height = 3;
        const soap = loadScript(script, seed);
        soap.setMode('LedFx Soap');
        soap.setDensity(0.9);
        soap.setSpeed(0.7);
        soap.setIntensity(0);
        soap.setFrequencyRange('Bass');
        soap.colors = [{ h: 0, s: 0, v: 0 }, { h: 0, s: 0, v: 1 }];
        soap.color = soap.colors[0];

        const oracle = makeSoapOracle(seed, width, height);
        const params = { density: 0.9, speed: 0.7, intensity: 0, range: 'Bass' };
        const f1 = frame({ bass: 0.5, low: 0.05, timing: { deltaSeconds: 0.1 } });
        const f2 = frame({ bass: 0.5, low: 0.9, timing: { deltaSeconds: 0.1 } });
        const map1 = render(soap, script, width, height, f1, 'worker-j row55 exact-1');
        const expected1 = soapOracleStep(oracle, f1, params, soap.colors);
        approxMap(map1, expected1, 'row55/exact-frame1');
        const map2 = render(soap, script, width, height, f2, 'worker-j row55 exact-2');
        const expected2 = soapOracleStep(oracle, f2, params, soap.colors);
        approxMap(map2, expected2, 'row55/exact-frame2');
        assert(oracle.debug.usedInBounds, 'row55/interior samples not used');
        assert(oracle.debug.usedOob, 'row55/edge palette fallback not used');
        assert(oracle.debug.sawNegativeFractional, 'row55/negative fractional displacement not exercised');

        const freeze = render(soap, script, width, height, frame({ bass: 0.5, timing: { deltaSeconds: 0 } }), 'worker-j row55 freeze1');
        const freeze2 = render(soap, script, width, height, frame({ bass: 0.5, timing: { deltaSeconds: 0 } }), 'worker-j row55 freeze2');
        approxMap(freeze, freeze2, 'row55/zero-dt freeze');

        const freeRun = loadScript(script, 0x5502);
        freeRun.setMode('LedFx Soap');
        freeRun.setIntensity(0);
        freeRun.setSpeed(0.8);
        const freeA = render(freeRun, script, width, height, frame({ low: 0, timing: { deltaSeconds: 0.15 } }), 'worker-j row55 freeA');
        const freeB = render(freeRun, script, width, height, frame({ low: 0, timing: { deltaSeconds: 0.15 } }), 'worker-j row55 freeB');
        assert.notDeepStrictEqual(freeA, freeB, 'row55/intensity0 must free-run');

        const speedSlow = loadScript(script, 0x5503);
        const speedFast = loadScript(script, 0x5503);
        speedSlow.setMode('LedFx Soap');
        speedFast.setMode('LedFx Soap');
        speedSlow.setIntensity(1);
        speedFast.setIntensity(1);
        speedSlow.setSpeed(0.2);
        speedFast.setSpeed(0.9);
        const holdA = render(speedSlow, script, width, height, frame({ low: 0, timing: { deltaSeconds: 0.1 } }), 'worker-j row55 holdA');
        const holdB = render(speedFast, script, width, height, frame({ low: 0, timing: { deltaSeconds: 0.1 } }), 'worker-j row55 holdB');
        approxMap(holdA, holdB, 'row55/intensity-positive zero-power holds phase');

        const speedZero = loadScript(script, 0x5504);
        speedZero.setMode('LedFx Soap');
        speedZero.setSpeed(0);
        speedZero.setIntensity(0);
        const stillMovesA = render(speedZero, script, width, height, frame({ low: 0.6, timing: { deltaSeconds: 0.1 } }), 'worker-j row55 speed0-a');
        const stillMovesB = render(speedZero, script, width, height, frame({ low: 0.6, timing: { deltaSeconds: 0.1 } }), 'worker-j row55 speed0-b');
        assert.notDeepStrictEqual(stillMovesA, stillMovesB, 'row55/speed0 must still transport feedback');

        const identityCarry = loadScript(script, 0x5505);
        const identityControl = loadScript(script, 0x5505);
        identityCarry.setMode('LedFx Soap');
        identityControl.setMode('LedFx Soap');
        identityCarry.setDensity(0.9);
        identityControl.setDensity(0.9);
        identityCarry.setSpeed(0.7);
        identityControl.setSpeed(0.7);
        identityCarry.setIntensity(0);
        identityControl.setIntensity(0);
        identityCarry.setFrequencyRange('Bass');
        identityControl.setFrequencyRange('Bass');
        identityCarry.colors = soap.colors;
        identityControl.colors = soap.colors;
        identityCarry.color = identityCarry.colors[0];
        identityControl.color = identityControl.colors[0];
        const stableIdentityFrame = frame({
            bass: 0.55,
            sourceId: 'row55-source',
            profileId: 29,
            sourceEpoch: 4,
            configRevision: 6,
            timing: { deltaSeconds: 0.1 },
            dt: 0.1
        });
        render(identityCarry, script, width, height, stableIdentityFrame, 'worker-j row55 identity-carry-1');
        render(identityCarry, script, width, height, frame({
            bass: 0.55,
            sourceId: '',
            profileId: 29,
            sourceEpoch: 4,
            configRevision: 6,
            available: false,
            status: 'reset',
            timing: { deltaSeconds: 0 },
            dt: 0
        }), 'worker-j row55 identity-unavailable-gap');
        const carryResume = render(identityCarry, script, width, height, stableIdentityFrame, 'worker-j row55 identity-carry-2');
        render(identityControl, script, width, height, stableIdentityFrame, 'worker-j row55 identity-control-1');
        const controlResume = render(identityControl, script, width, height, stableIdentityFrame, 'worker-j row55 identity-control-2');
        approxMap(carryResume, controlResume, 'row55/identity-helper-unavailable-hold');

        const switchedEpochFrame = frame({
            bass: 0.55,
            sourceId: 'row55-source',
            profileId: 29,
            sourceEpoch: 5,
            configRevision: 6,
            timing: { deltaSeconds: 0.1 },
            dt: 0.1
        });
        const switchedEpochMap = render(identityCarry, script, width, height, switchedEpochFrame, 'worker-j row55 identity-epoch-switch');
        const freshEpoch = loadScript(script, 0x5505);
        freshEpoch.setMode('LedFx Soap');
        freshEpoch.setDensity(0.9);
        freshEpoch.setSpeed(0.7);
        freshEpoch.setIntensity(0);
        freshEpoch.setFrequencyRange('Bass');
        freshEpoch.colors = soap.colors;
        freshEpoch.color = freshEpoch.colors[0];
        const freshEpochMap = render(freshEpoch, script, width, height, switchedEpochFrame, 'worker-j row55 identity-epoch-fresh');
        approxMap(switchedEpochMap, freshEpochMap, 'row55/identity-helper-epoch-reset');
        markCase('row55-ledfx-soap');
        console.log('PASS row55-ledfx-soap');
    }
}

function assertWorkerEOutputRows() {
  const xCaseIds = {
    row03: 'row03-bar-x',
    row35: 'row35-multibar-x',
    row31: 'row31-marching-x',
    row57: 'row57-spotlight-x'
  };

  const xApprox = (a, b, eps = 1e-6) => Math.abs(a - b) <= eps;
  const xFrame = ({
    phase = 0,
    valid = true,
    beatDelta = 0,
    onsetDelta = 0,
    kickDelta = 0,
    beatFired = false,
    onset = false,
    kickFired = false,
    dt = 0,
    low = 0,
    mid = 0,
    high = 0,
    sourceId = 'worker-e-x',
    profileId = 1,
    sourceEpoch = 1,
    configRevision = 1
  } = {}) => audio({
    version: 6,
    phase,
    dt,
    timing: { deltaSeconds: dt },
    low,
    mid,
    high,
    beatFired,
    onset,
    kickFired,
    sourceId,
    profileId,
    sourceEpoch,
    configRevision,
    tempo: { valid, bpm: 120, beatPhase: phase, barPhase: phase },
    events: { delta: { beat: beatDelta, onset: onsetDelta, kick: kickDelta, bar: 0 } }
  });

  const xHsv = (map, width, index) => {
    const i = index * 3;
    return { h: map[i], s: map[i + 1], v: map[i + 2] };
  };

  const xLit = map => {
    const out = [];
    for (let i = 0; i < map.length / 3; i++) if (map[i * 3 + 2] > 1e-6) out.push(i);
    return out;
  };

  function assertOutputMetadata(scriptFile) {
    const algo = loadScript(scriptFile, 0xEE01);
    const names = algo.properties.map(p => {
      const part = p.split('|').find(x => x.indexOf('name:') === 0);
      return part ? part.slice(5) : '';
    });
    const unique = new Set(names);
    assert.strictEqual(names.length, unique.size, `${scriptFile}: duplicate property names`);

    const expectedX = ['flip', 'mirror', 'backgroundMode', 'backgroundColor', 'backgroundBrightness', 'brightness', 'blur'];
    for (const key of expectedX) {
      const count = names.filter(n => n === key).length;
      assert.strictEqual(count, 1, `${scriptFile}: missing or duplicate ${key}`);
    }

    assert.strictEqual(algo.getFlip(), 'Off', `${scriptFile}: default flip`);
    assert.strictEqual(algo.getMirror(), 'Off', `${scriptFile}: default mirror`);
    assert.strictEqual(algo.getBackgroundMode(), 'Off', `${scriptFile}: default backgroundMode`);
    assert.strictEqual(algo.getBackgroundColor(), '#000000', `${scriptFile}: default backgroundColor`);
    assert.strictEqual(algo.getBackgroundBrightness(), 1, `${scriptFile}: default backgroundBrightness`);
    assert.strictEqual(algo.getBrightness(), 1, `${scriptFile}: default brightness`);
    assert.strictEqual(algo.getBlur(), 0, `${scriptFile}: default blur`);

    algo.setFlip('Yes');
    algo.setMirror('No');
    assert.strictEqual(algo.getFlip(), 'On', `${scriptFile}: Yes/No normalization for flip`);
    assert.strictEqual(algo.getMirror(), 'Off', `${scriptFile}: Yes/No normalization for mirror`);
  }

  {
    const scriptFile = 'audiobpmbar.js';
    assertOutputMetadata(scriptFile);
    const algo = loadScript(scriptFile, 0xEE31);
    algo.colors = [{ h: 0.02, s: 1, v: 1 }, { h: 0.62, s: 1, v: 1 }];
    algo.setMode('wipe');
    algo.setEaseMethod('linear');

    const base = render(algo, scriptFile, 8, 1, xFrame({ phase: 0.375 }), 'row03 x base');
    assert.deepStrictEqual(xLit(base), [0, 1, 2], 'row03/x baseline lit region');

    algo.setFlip('On');
    algo.setMirror('On');
    const flipMirror = render(algo, scriptFile, 8, 1, xFrame({ phase: 0.375 }), 'row03 x flip+mirror');
    assert.deepStrictEqual(xLit(flipMirror), [0, 1, 6, 7], 'row03/x flip then paired-max mirror ordering');

    const add = loadScript(scriptFile, 0xEE32);
    add.colors = algo.colors;
    add.setMode('wipe');
    add.setEaseMethod('linear');
    add.setBeatSkip('even');
    add.setBackgroundMode('Additive');
    add.setBackgroundColor('#00ff00');
    add.setBackgroundBrightness(1);
    add.setBrightness(0.5);
    const additive = render(add, scriptFile, 8, 1, xFrame({ phase: 0.375 }), 'row03 x additive+brightness');
    for (let i = 0; i < 8; i++) {
      const p = xHsv(additive, 8, i);
      assert(xApprox(p.h, 1 / 3, 0.02), `row03/x additive hue mismatch at ${i}: ${p.h}`);
      assert(xApprox(p.s, 1, 1e-4), `row03/x additive saturation mismatch at ${i}: ${p.s}`);
      assert(xApprox(p.v, 0.5, 0.02), `row03/x additive-before-brightness mismatch at ${i}: ${p.v}`);
    }

    markCase(xCaseIds.row03);
    console.log('PASS row03-bar-x');
  }

  {
    const scriptFile = 'audiomulticolorbar.js';
    assertOutputMetadata(scriptFile);
    const algo = loadScript(scriptFile, 0xEE35);
    algo.colors = [{ h: 0.0, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];
    algo.setMode('wipe');
    algo.setEaseMethod('linear');
    algo.setColorStep(0.5);

    const sharp = render(algo, scriptFile, 12, 1, xFrame({ phase: 0.5 }), 'row35 x sharp');
    algo.setBlur(2);
    const blurred = render(algo, scriptFile, 12, 1, xFrame({ phase: 0.5 }), 'row35 x blurred');
    assert.notDeepStrictEqual(blurred, sharp, 'row35/x blur should modify edge profile');

    markCase(xCaseIds.row35);
    console.log('PASS row35-multibar-x');
  }

  {
    const scriptFile = 'audiomarching.js';
    assertOutputMetadata(scriptFile);
    const algo = loadScript(scriptFile, 0xEE41);
    algo.colors = [{ h: 0.58, s: 1, v: 1 }, { h: 0.08, s: 1, v: 1 }];
    algo.setSpeed(0.2);
    algo.setReactivity(0.8);

    const base = render(algo, scriptFile, 17, 1, xFrame({ dt: 0.2, low: 0.7 }), 'row31 x base');
    const defaults = loadScript(scriptFile, 0xEE41);
    defaults.colors = algo.colors;
    defaults.setSpeed(0.2);
    defaults.setReactivity(0.8);
    const identity = render(defaults, scriptFile, 17, 1, xFrame({ dt: 0.2, low: 0.7 }), 'row31 x identity');
    assert.deepStrictEqual(base, identity, 'row31/x identity defaults should preserve baseline output');

    const bg = loadScript(scriptFile, 0xEE42);
    bg.colors = algo.colors;
    bg.setSpeed(0.2);
    bg.setReactivity(0.8);
    bg.setBackgroundMode('Additive');
    bg.setBackgroundColor('#0000ff');
    bg.setBackgroundBrightness(0.5);
    bg.setBrightness(0.5);
    const mixed = render(bg, scriptFile, 17, 1, xFrame({ dt: 0.2, low: 0.7 }), 'row31 x background+brightness');
    assert.notDeepStrictEqual(mixed, base, 'row31/x additive background should affect output before brightness');

    markCase(xCaseIds.row31);
    console.log('PASS row31-marching-x');
  }

  {
    const scriptFile = 'audiospotlight.js';
    assertOutputMetadata(scriptFile);

    const halfLifeExpected = Math.pow(0.5, 1.4);
    const decay = loadScript(scriptFile, 0xEE57);
    decay.colors = [{ h: 0, s: 0, v: 1 }, { h: 0.66, s: 1, v: 1 }];
    decay.setGradient('No');
    decay.setMaxSpots(1);
    decay.setFadeSeconds(0.8);
    decay.setBirthGain(0);
    const born = render(decay, scriptFile, 11, 1, xFrame({ dt: 0, onset: true, onsetDelta: 1 }), 'row57 x born');
    const half = render(decay, scriptFile, 11, 1, xFrame({ dt: 0.4 }), 'row57 x half-life');
    const ratio = Math.max(...half.filter((_, i) => i % 3 === 2)) / Math.max(1e-9, Math.max(...born.filter((_, i) => i % 3 === 2)));
    assert(Math.abs(ratio - halfLifeExpected) < 0.03,
      `row57/x life ratio expected ${halfLifeExpected}, got ${ratio}`);

    const overlapMap = brightness => {
      const s = loadScript(scriptFile, 0xEE58);
      s.colors = [{ h: 1 / 12, s: 1, v: 1 }];
      s.setGradient('No');
      s.setWidth(0.6);
      s.setMaxSpots(2);
      s.setFadeSeconds(5);
      s.setBirthGain(30);
      s.setBrightness(brightness);
      return render(s, scriptFile, 1, 1,
        xFrame({ dt: 0, low: 1, mid: 1, high: 1, onset: true, onsetDelta: 2, kickFired: true, kickDelta: 2 }),
        `row57 x overlap b${brightness}`);
    };
    const b1 = overlapMap(1);
    const b05 = overlapMap(0.5);
    const p1 = xHsv(b1, 1, 0);
    const p05 = xHsv(b05, 1, 0);
    assert(p1.v > 0.99, `row57/x overlap expected clipped v=1 at brightness1, got ${p1.v}`);
    assert(p05.v > 0.55, `row57/x overlap brightness0.5 should preserve overdrive before final clip, got ${p05.v}`);
    assert(p05.h < 0.12, `row57/x overlap hue should preserve unclipped RGB ratio, got ${p05.h}`);

    const withBlur = loadScript(scriptFile, 0xEE59);
    withBlur.colors = [{ h: 0, s: 0, v: 1 }];
    withBlur.setGradient('No');
    withBlur.setMaxSpots(1);
    withBlur.setFadeSeconds(2);
    withBlur.setBirthGain(0);
    const sharp = render(withBlur, scriptFile, 17, 1, xFrame({ dt: 0, onset: true, onsetDelta: 1 }), 'row57 x sharp');
    withBlur.setBlur(2);
    const blur = render(withBlur, scriptFile, 17, 1, xFrame({ dt: 0.01 }), 'row57 x blur');
    assert.notDeepStrictEqual(blur, sharp, 'row57/x blur should alter strip distribution');

    markCase(xCaseIds.row57);
    console.log('PASS row57-spotlight-x');
  }
}

function assertWorkerERegressionRows() {
  const approx = (a, b, eps = 1e-6) => Math.abs(a - b) <= eps;
  const maxV = map => {
    let peak = 0;
    for (let i = 2; i < map.length; i += 3) peak = Math.max(peak, map[i]);
    return peak;
  };
  const isLit = map => maxV(map) > 1e-6;
  const xFrame = ({
    phase = 0,
    valid = true,
    beatDelta = 0,
    onsetDelta = 0,
    kickDelta = 0,
    beatFired = false,
    onset = false,
    kickFired = false,
    dt = 0,
    low = 0,
    mid = 0,
    high = 0,
    beat = 0,
    bass = 0,
    sourceId = 'mic-a',
    profileId = 7,
    sourceEpoch = 3,
    configRevision = 2,
    available = true,
    status = 'ok'
  } = {}) => audio({
    version: 6,
    dt,
    timing: { deltaSeconds: dt },
    low,
    mid,
    high,
    beat,
    bass,
    phase,
    beatFired,
    onset,
    kickFired,
    sourceId,
    profileId,
    sourceEpoch,
    configRevision,
    available,
    status,
    tempo: { valid, bpm: 120, beatPhase: phase, barPhase: phase },
    events: { delta: { beat: beatDelta, onset: onsetDelta, kick: kickDelta, bar: 0 } }
  });

  function assertSpotlightDeltaAliasDedupe() {
    const configure = algo => {
      algo.colors = [{ h: 0.1, s: 1, v: 1 }];
      algo.setGradient('No');
      algo.setFadeSeconds(1.0);
      algo.setMaxSpots(6);
      algo.setBirthGain(0);
      algo.setSource('Lows (beat+bass)');
    };

    const onsetTrue = loadScript('audiospotlight.js', 0xE5A1);
    const onsetFalse = loadScript('audiospotlight.js', 0xE5A1);
    configure(onsetTrue);
    configure(onsetFalse);
    const onsetWithAlias = render(onsetTrue, 'audiospotlight.js', 9, 1,
      xFrame({ dt: 0, onsetDelta: 1, onset: true }), 'spotlight onset delta+alias');
    const onsetDeltaOnly = render(onsetFalse, 'audiospotlight.js', 9, 1,
      xFrame({ dt: 0, onsetDelta: 1, onset: false }), 'spotlight onset delta only');
    assert.deepStrictEqual(
      onsetWithAlias,
      onsetDeltaOnly,
      'spotlight consumed onset delta + alias twice'
    );

    const kickTrue = loadScript('audiospotlight.js', 0xE5A2);
    const kickFalse = loadScript('audiospotlight.js', 0xE5A2);
    configure(kickTrue);
    configure(kickFalse);
    const kickWithAlias = render(kickTrue, 'audiospotlight.js', 9, 1,
      xFrame({ dt: 0, kickDelta: 1, kickFired: true }), 'spotlight kick delta+alias');
    const kickDeltaOnly = render(kickFalse, 'audiospotlight.js', 9, 1,
      xFrame({ dt: 0, kickDelta: 1, kickFired: false }), 'spotlight kick delta only');
    assert.deepStrictEqual(
      kickWithAlias,
      kickDeltaOnly,
      'spotlight consumed kick delta + alias twice'
    );

    markCase('row57-spotlight-delta-alias-dedupe');
    console.log('PASS row57-spotlight-delta-alias-dedupe');
  }

  function assertSpotlightCarryBoundedAtCap() {
    const algo = loadScript('audiospotlight.js', 0xE5B1);
    algo.colors = [{ h: 0.2, s: 1, v: 1 }];
    algo.setGradient('No');
    algo.setMaxSpots(1);
    algo.setFadeSeconds(0.2);
    algo.setBirthGain(0);
    algo.setSource('Lows (beat+bass)');

    render(algo, 'audiospotlight.js', 9, 1,
      xFrame({ dt: 0, onsetDelta: 1, onset: false, low: 0, beat: 0, bass: 0 }),
      'spotlight cap seed');

    for (let i = 0; i < 40; i++) {
      render(algo, 'audiospotlight.js', 9, 1,
        xFrame({ dt: 0, onsetDelta: 1, onset: false, low: 0, beat: 0, bass: 0 }),
        `spotlight capped backlog ${i}`);
    }

    const quietLit = [];
    for (let i = 0; i < 6; i++) {
      const map = render(algo, 'audiospotlight.js', 9, 1,
        xFrame({ dt: 0.21, onsetDelta: 0, kickDelta: 0, low: 0, mid: 0, high: 0, beat: 0, bass: 0 }),
        `spotlight quiet pacing ${i}`);
      quietLit.push(isLit(map));
    }

    const quietPattern = quietLit.map(v => (v ? '1' : '0')).join('');
    assert(quietLit.includes(false),
      `spotlight quiet pacing caught up from unbounded backlog: pattern=${quietPattern}`);
    let run = 0;
    let maxRun = 0;
    for (const lit of quietLit) {
      run = lit ? run + 1 : 0;
      if (run > maxRun) maxRun = run;
    }
    assert(maxRun <= 2,
      `spotlight backlog still dominates quiet pacing (max consecutive lit=${maxRun}, pattern=${quietPattern})`);

    markCase('row57-spotlight-carry-cap');
    console.log('PASS row57-spotlight-carry-cap');
  }

  function assertTransientUnavailableNoReset(scriptFile, seed, configure, size, frame) {
    const a = loadScript(scriptFile, seed);
    const b = loadScript(scriptFile, seed);
    configure(a);
    configure(b);

    render(a, scriptFile, size[0], size[1], frame, `${scriptFile} warmup A`);
    render(b, scriptFile, size[0], size[1], frame, `${scriptFile} warmup B`);

    render(a, scriptFile, size[0], size[1], xFrame({
      dt: 0,
      phase: frame.phase,
      low: frame.low,
      mid: frame.mid,
      high: frame.high,
      beat: frame.beat,
      bass: frame.bass,
      profileId: frame.profileId,
      available: false,
      sourceId: '',
      sourceEpoch: 0,
      configRevision: 0,
      status: 'reset'
    }), `${scriptFile} transient unavailable`);

    render(b, scriptFile, size[0], size[1], xFrame({
      dt: 0,
      phase: frame.phase,
      low: frame.low,
      mid: frame.mid,
      high: frame.high,
      beat: frame.beat,
      bass: frame.bass,
      profileId: frame.profileId,
      sourceId: frame.sourceId,
      sourceEpoch: frame.sourceEpoch,
      configRevision: frame.configRevision
    }), `${scriptFile} transient available`);

    const nextA = render(a, scriptFile, size[0], size[1], frame, `${scriptFile} post transient unavailable`);
    const nextB = render(b, scriptFile, size[0], size[1], frame, `${scriptFile} post transient available`);
    assert.deepStrictEqual(nextA, nextB, `${scriptFile}: transient unavailable caused reset`);
  }

  function assertResetVsFresh(scriptFile, seed, configure, warmFrame, resetFrame, warmSize, resetSize) {
    const mutated = loadScript(scriptFile, seed);
    const fresh = loadScript(scriptFile, seed);
    configure(mutated);
    configure(fresh);

    render(mutated, scriptFile, warmSize[0], warmSize[1], warmFrame, `${scriptFile} warmup`);
    const afterReset = render(mutated, scriptFile, resetSize[0], resetSize[1], resetFrame, `${scriptFile} after reset trigger`);
    const freshFirst = render(fresh, scriptFile, resetSize[0], resetSize[1], resetFrame, `${scriptFile} fresh baseline`);
    assert.deepStrictEqual(afterReset, freshFirst, `${scriptFile}: reset state diverged from fresh`);
  }

  function assertIdentityAndGeometryResets() {
    const bpmConfigure = algo => {
      algo.colors = [{ h: 0.0, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];
      algo.setMode('wipe');
      algo.setEaseMethod('linear');
      algo.setColorStep(0.125);
    };
    const bpmWarm = xFrame({
      phase: 0.9, dt: 0.05, beatDelta: 0,
      sourceId: 'bpm-source', profileId: 9, sourceEpoch: 1, configRevision: 1
    });
    assertTransientUnavailableNoReset('audiobpmbar.js', 0xE5C1, bpmConfigure, [8, 1], bpmWarm);
    assertResetVsFresh(
      'audiobpmbar.js', 0xE5C2, bpmConfigure,
      bpmWarm,
      xFrame({
        phase: 0.25, dt: 0.05,
        sourceId: 'bpm-source', profileId: 9, sourceEpoch: 2, configRevision: 1
      }),
      [8, 1], [8, 1]
    );
    assertResetVsFresh(
      'audiobpmbar.js', 0xE5C3, bpmConfigure,
      bpmWarm,
      xFrame({
        phase: 0.25, dt: 0.05,
        sourceId: 'bpm-source', profileId: 9, sourceEpoch: 1, configRevision: 1
      }),
      [8, 1], [1, 8]
    );
    markCase('row03-bpm-reset-geometry');
    console.log('PASS row03-bpm-reset-geometry');

    const multiConfigure = algo => {
      algo.colors = [{ h: 0.0, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];
      algo.setMode('wipe');
      algo.setEaseMethod('linear');
      algo.setColorStep(0.125);
    };
    const multiWarm = xFrame({
      phase: 0.25, dt: 0.05, beatDelta: 1, beatFired: true,
      sourceId: 'multi-source', profileId: 10, sourceEpoch: 1, configRevision: 1
    });
    assertTransientUnavailableNoReset('audiomulticolorbar.js', 0xE5D1, multiConfigure, [8, 1], multiWarm);
    assertResetVsFresh(
      'audiomulticolorbar.js', 0xE5D2, multiConfigure,
      multiWarm,
      xFrame({
        phase: 0.25, dt: 0.05, beatDelta: 0,
        sourceId: 'multi-source', profileId: 10, sourceEpoch: 2, configRevision: 1
      }),
      [8, 1], [8, 1]
    );
    assertResetVsFresh(
      'audiomulticolorbar.js', 0xE5D3, multiConfigure,
      multiWarm,
      xFrame({
        phase: 0.25, dt: 0.05, beatDelta: 0,
        sourceId: 'multi-source', profileId: 10, sourceEpoch: 1, configRevision: 1
      }),
      [8, 1], [1, 8]
    );
    markCase('row35-multibar-reset-geometry');
    console.log('PASS row35-multibar-reset-geometry');

    const marchConfigure = algo => {
      algo.colors = [{ h: 0.2, s: 0.3, v: 1 }, { h: 0.7, s: 0.6, v: 1 }];
      algo.setSpeed(0.15);
      algo.setReactivity(0.8);
    };
    const marchWarm = xFrame({
      dt: 0.2, low: 0.8,
      sourceId: 'march-source', profileId: 11, sourceEpoch: 1, configRevision: 1
    });
    assertTransientUnavailableNoReset('audiomarching.js', 0xE5E1, marchConfigure, [17, 1], marchWarm);
    assertResetVsFresh(
      'audiomarching.js', 0xE5E2, marchConfigure,
      marchWarm,
      xFrame({
        dt: 0.2, low: 0.8,
        sourceId: 'march-source', profileId: 11, sourceEpoch: 2, configRevision: 1
      }),
      [17, 1], [17, 1]
    );
    assertResetVsFresh(
      'audiomarching.js', 0xE5E3, marchConfigure,
      marchWarm,
      xFrame({
        dt: 0.2, low: 0.8,
        sourceId: 'march-source', profileId: 11, sourceEpoch: 1, configRevision: 1
      }),
      [17, 1], [1, 17]
    );
    markCase('row31-marching-reset-geometry');
    console.log('PASS row31-marching-reset-geometry');
  }

  function assertFlattenedSequenceEquivalence() {
    const bpmA = loadScript('audiobpmbar.js', 0xE5F1);
    const bpmB = loadScript('audiobpmbar.js', 0xE5F1);
    bpmA.colors = bpmB.colors = [{ h: 0.03, s: 1, v: 1 }, { h: 0.55, s: 1, v: 1 }];
    bpmA.setMode('wipe'); bpmB.setMode('wipe');
    bpmA.setEaseMethod('linear'); bpmB.setEaseMethod('linear');
    const bpmFrame = xFrame({ phase: 0.375, sourceId: 'flat-bpm', profileId: 12 });
    const bpmWide = render(bpmA, 'audiobpmbar.js', 8, 1, bpmFrame, 'bpm 8x1');
    const bpmTall = render(bpmB, 'audiobpmbar.js', 1, 8, bpmFrame, 'bpm 1x8');
    assert.deepStrictEqual(bpmWide, bpmTall, 'bpm bar did not use flattened N indexing');
    markCase('row03-bpm-flattened-n');
    console.log('PASS row03-bpm-flattened-n');

    const multiA = loadScript('audiomulticolorbar.js', 0xE5F2);
    const multiB = loadScript('audiomulticolorbar.js', 0xE5F2);
    multiA.colors = multiB.colors = [{ h: 0.0, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];
    multiA.setMode('wipe'); multiB.setMode('wipe');
    multiA.setEaseMethod('linear'); multiB.setEaseMethod('linear');
    const multiFrame = xFrame({ phase: 0.25, sourceId: 'flat-multi', profileId: 13 });
    const multiWide = render(multiA, 'audiomulticolorbar.js', 8, 1, multiFrame, 'multibar 8x1');
    const multiTall = render(multiB, 'audiomulticolorbar.js', 1, 8, multiFrame, 'multibar 1x8');
    assert.deepStrictEqual(multiWide, multiTall, 'multicolor bar did not use flattened N indexing');
    markCase('row35-multibar-flattened-n');
    console.log('PASS row35-multibar-flattened-n');
  }

  function assertMarchingPaletteSaturation() {
    const algo = loadScript('audiomarching.js', 0xE601);
    algo.colors = [{ h: 0.2, s: 0.25, v: 1 }];
    algo.setSpeed(0.2);
    algo.setReactivity(0.6);
    const map = render(algo, 'audiomarching.js', 17, 1,
      xFrame({ dt: 0.2, low: 0.7, sourceId: 'march-sat', profileId: 14 }),
      'marching desaturated palette');
    let lit = 0;
    for (let i = 0; i < map.length; i += 3) {
      if (map[i + 2] <= 0.05) continue;
      lit++;
      assert(approx(map[i + 1], 0.25, 1e-6),
        `marching saturation should track palette.s for lit pixels, got ${map[i + 1]}`);
    }
    assert(lit > 0, 'marching saturation test produced no lit pixels');
    markCase('row31-marching-palette-saturation');
    console.log('PASS row31-marching-palette-saturation');
  }

  assertSpotlightDeltaAliasDedupe();
  assertSpotlightCarryBoundedAtCap();
  assertIdentityAndGeometryResets();
  assertFlattenedSequenceEquivalence();
  assertMarchingPaletteSaturation();
}

function assertWorkerGReferenceRows() {
function wgFrame(overrides) {
    return audio(Object.assign({
        version: 6,
        dt: 1 / 30,
        timing: { deltaSeconds: 1 / 60 },
        sourceId: 'worker-g-source',
        profileId: 70,
        sourceEpoch: 1,
        configRevision: 1,
        tempo: { valid: true, bpm: 120, beatPhase: 0, barPhase: 0 },
        banks: { full: { count: 10, processed: new Array(10).fill(0), novelty: new Array(10).fill(0) } },
        powers: { raw: { low: 0, mid: 0, high: 0 } },
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
    }, overrides || {}));
}

function wgRgb(map, index) {
    return hsvPixels(map)[index];
}

function wgApprox(actual, expected, eps, label) {
    assert(Math.abs(actual - expected) <= eps,
        `${label}: expected ${expected}, got ${actual}`);
}

function wgApproxRgb(actual, expected, eps, label) {
    for (let c = 0; c < 3; c++)
        wgApprox(actual[c], expected[c], eps, `${label} ch${c}`);
}

function wgPeakV(map) {
    let peak = 0;
    for (let i = 2; i < map.length; i += 3) peak = Math.max(peak, map[i]);
    return peak;
}

function wgPixelV(map, pixelIndex) {
    return map[pixelIndex * 3 + 2];
}

function wgAssertFiniteMap(map, label) {
    for (let i = 0; i < map.length; i++)
        assert(Number.isFinite(map[i]), `${label}: non-finite at index ${i} (${map[i]})`);
}

function wgEnergyRows() {
    const processed = [0.625, 0.625, 0.3125, 0.3125, 0.3125, 0.1041667, 0.1041667, 0.1041667, 0.1041667, 0.1041667];
    const lowRelease = new Array(10).fill(0);

    const additive = loadScript('audioenergy.js', 0x701);
    additive.setMode('Reference');
    additive.setReferenceBlur(0);
    additive.setReferenceMirror('Off');
    additive.setReferenceBrightness(1);
    additive.setReferenceMixingMode('Additive');
    const addMap = render(additive, 'audioenergy.js', 6, 1, wgFrame({
        frameSequence: 1,
        banks: { full: { count: processed.length, processed, novelty: new Array(processed.length).fill(0) } }
    }), 'worker-g energy additive');
    wgApproxRgb(wgRgb(addMap, 0), [1, 1, 1], 1e-6, 'row13 additive idx0 white');
    wgApproxRgb(wgRgb(addMap, 1), [1, 1, 0], 1e-6, 'row13 additive idx1 yellow');
    wgApproxRgb(wgRgb(addMap, 5), [1, 0, 0], 1e-6, 'row13 additive idx5 red');

    const overlap = loadScript('audioenergy.js', 0x701);
    overlap.setMode('Reference');
    overlap.setReferenceBlur(0);
    overlap.setReferenceMirror('Off');
    overlap.setReferenceBrightness(1);
    overlap.setReferenceMixingMode('Overlap');
    const ovMap = render(overlap, 'audioenergy.js', 6, 1, wgFrame({
        frameSequence: 1,
        banks: { full: { count: processed.length, processed, novelty: new Array(processed.length).fill(0) } }
    }), 'worker-g energy overlap');
    wgApproxRgb(wgRgb(ovMap, 0), [0, 0, 1], 1e-6, 'row13 overlap idx0 blue');
    wgApproxRgb(wgRgb(ovMap, 1), [0, 1, 0], 1e-6, 'row13 overlap idx1 green');
    wgApproxRgb(wgRgb(ovMap, 5), [1, 0, 0], 1e-6, 'row13 overlap idx5 red');

    const sensitive = loadScript('audioenergy.js', 0x702);
    sensitive.setMode('Reference');
    sensitive.setReferenceBlur(0);
    sensitive.setReferenceMirror('Off');
    sensitive.setReferenceBrightness(1);
    sensitive.setReferenceMixingMode('Additive');
    sensitive.setReferenceSensitivity(0.6);
    render(sensitive, 'audioenergy.js', 6, 1, wgFrame({
        frameSequence: 1,
        banks: { full: { count: processed.length, processed, novelty: new Array(processed.length).fill(0) } }
    }), 'worker-g energy sensitivity prime');
    const decayed = render(sensitive, 'audioenergy.js', 6, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 1 / 60 },
        banks: { full: { count: lowRelease.length, processed: lowRelease, novelty: new Array(lowRelease.length).fill(0) } }
    }), 'worker-g energy sensitivity decay');
    wgApprox(wgRgb(decayed, 0)[0], 0.65, 1e-6, 'row13 sensitivity retain');

    const cycler = loadScript('audioenergy.js', 0x703);
    cycler.setMode('Reference');
    cycler.setReferenceBlur(0);
    cycler.setReferenceMirror('Off');
    cycler.setReferenceBrightness(1);
    cycler.setReferenceMixingMode('Overlap');
    cycler.setReferenceSensitivity(0.99);
    cycler.setReferenceColorCycler('On');
    cycler.setReferenceCyclePalette('#00ffff');
    render(cycler, 'audioenergy.js', 6, 1, wgFrame({
        frameSequence: 1,
        banks: { full: { count: processed.length, processed, novelty: new Array(processed.length).fill(0) } }
    }), 'worker-g energy cycler baseline');
    const kick1 = render(cycler, 'audioenergy.js', 6, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 1 / 60 },
        banks: { full: { count: processed.length, processed, novelty: new Array(processed.length).fill(0) } },
        events: { delta: { onset: 0, beat: 0, kick: 1, bar: 0 } }
    }), 'worker-g energy cycler kick1');
    assert(wgRgb(kick1, 0)[1] < 0.1, 'row13 kick1 should keep highs on first kick');
    assert(wgRgb(kick1, 1)[2] > 0.9, 'row13 kick1 should change mids first');
    const kick2 = render(cycler, 'audioenergy.js', 6, 1, wgFrame({
        frameSequence: 3,
        timing: { deltaSeconds: 1 / 60 },
        banks: { full: { count: processed.length, processed, novelty: new Array(processed.length).fill(0) } },
        events: { delta: { onset: 0, beat: 0, kick: 1, bar: 0 } }
    }), 'worker-g energy cycler kick2');
    assert(wgRgb(kick2, 0)[1] > 0.9, 'row13 kick2 should change highs second');
    const kick3 = render(cycler, 'audioenergy.js', 6, 1, wgFrame({
        frameSequence: 4,
        timing: { deltaSeconds: 1 / 60 },
        banks: { full: { count: processed.length, processed, novelty: new Array(processed.length).fill(0) } },
        events: { delta: { onset: 0, beat: 0, kick: 1, bar: 0 } }
    }), 'worker-g energy cycler kick3');
    assert(wgRgb(kick3, 5)[2] > 0.9, 'row13 kick3 should change lows third');
    const duplicateKick = render(cycler, 'audioenergy.js', 6, 1, wgFrame({
        frameSequence: 4,
        timing: { deltaSeconds: 0 },
        banks: { full: { count: processed.length, processed, novelty: new Array(processed.length).fill(0) } },
        events: { delta: { onset: 0, beat: 0, kick: 1, bar: 0 } }
    }), 'worker-g energy cycler duplicate');
    assert.deepStrictEqual(duplicateKick, kick3, 'row13 duplicate kick delta consumed twice');

    const cropped = loadScript('audioenergy.js', 0x704);
    cropped.setMode('Reference');
    cropped.setReferenceBlur(0);
    cropped.setReferenceMirror('Off');
    cropped.setReferenceBrightness(1);
    cropped.setReferenceRangeStart(0.2);
    cropped.setReferenceRangeEnd(0.2);
    const empty = render(cropped, 'audioenergy.js', 6, 1, wgFrame({
        frameSequence: 1,
        banks: { full: { count: processed.length, processed, novelty: new Array(processed.length).fill(0) } }
    }), 'worker-g energy empty range');
    assert(wgPeakV(empty) < 1e-9, 'row13 empty cropped thirds must be dark');

    markCase('worker-g-row13-energy-lengths-mixing');
    markCase('worker-g-row13-energy-sensitivity-cycler');
}

function wgEnergy2AndWavelengthRows() {
    const ledFx = loadScript('audioenergy2.js', 0x705);
    ledFx.setMode('LedFx Energy 2');
    const decoyA = loadScript('audioenergy2.js', 0x705);
    const decoyB = loadScript('audioenergy2.js', 0x705);
    decoyA.setMode('LedFx Energy 2');
    decoyB.setMode('LedFx Energy 2');
    decoyA.setSpeed(0);
    decoyB.setSpeed(0);
    const lowA = render(decoyA, 'audioenergy2.js', 7, 1, wgFrame({
        frameSequence: 1,
        low: 0.2,
        powers: { raw: { low: 0.1, mid: 0, high: 0 } }
    }), 'worker-g energy2 raw low A');
    const lowDecoy = render(decoyB, 'audioenergy2.js', 7, 1, wgFrame({
        frameSequence: 1,
        low: 0.9,
        powers: { raw: { low: 0.1, mid: 0, high: 0 } }
    }), 'worker-g energy2 filtered decoy');
    assert.deepStrictEqual(lowA, lowDecoy, 'row14 LedFx Energy2 should ignore filtered low decoy');
    const lowRaw = render(ledFx, 'audioenergy2.js', 7, 1, wgFrame({
        frameSequence: 1,
        low: 0.2,
        powers: { raw: { low: 0.9, mid: 0, high: 0 } }
    }), 'worker-g energy2 raw high');
    assert.notDeepStrictEqual(lowA, lowRaw, 'row14 LedFx Energy2 must use raw low source');

    const satMap = render(ledFx, 'audioenergy2.js', 7, 1, wgFrame({
        frameSequence: 4,
        low: 0.2,
        powers: { raw: { low: 0.6, mid: 0, high: 0 } },
        timing: { deltaSeconds: 1 / 60 }
    }), 'worker-g energy2 sat test');
    const sats = [];
    const hues = [];
    for (let i = 0; i < 7; i++) {
        hues.push(satMap[i * 3]);
        sats.push(satMap[i * 3 + 1]);
        assert(satMap[i * 3 + 1] === 0 || satMap[i * 3 + 1] === 1,
            `row14 binary saturation expected, got ${satMap[i * 3 + 1]}`);
    }
    assert(sats.some(v => v === 0) && sats.some(v => v === 1),
        'row14 needs neutral and saturated regions');
    for (let i = 1; i < hues.length; i++)
        wgApprox(hues[i], hues[0], 1e-9, 'row14 shared hue coordinate');

    const satLow = render(ledFx, 'audioenergy2.js', 7, 1, wgFrame({
        frameSequence: 5,
        low: 0.2,
        powers: { raw: { low: 0.1, mid: 0, high: 0 } }
    }), 'worker-g energy2 sat threshold low');
    const satHigh = render(ledFx, 'audioenergy2.js', 7, 1, wgFrame({
        frameSequence: 6,
        low: 0.2,
        powers: { raw: { low: 0.9, mid: 0, high: 0 } }
    }), 'worker-g energy2 sat threshold high');
    const satCount = map => {
        let out = 0;
        for (let i = 1; i < map.length; i += 3) if (map[i] > 0.5) out++;
        return out;
    };
    assert(satCount(satHigh) < satCount(satLow),
        'row14 threshold movement should reduce saturated pixels at high raw low');

    const artistic = loadScript('audioenergy2.js', 0x705);
    artistic.setMode('Artistic');
    const artA = render(artistic, 'audioenergy2.js', 7, 1, wgFrame({ frameSequence: 1, low: 0.2, powers: { raw: { low: 0.1 } } }), 'worker-g energy2 artistic A');
    const artB = render(artistic, 'audioenergy2.js', 7, 1, wgFrame({ frameSequence: 2, low: 0.9, powers: { raw: { low: 0.1 } } }), 'worker-g energy2 artistic B');
    assert.notDeepStrictEqual(artA, artB, 'row14 Artistic path should still use filtered low');

    const wave = loadScript('audioenergy2.js', 0x706);
    wave.setMode('Reference');
    wave.setReferenceBlur(0);
    wave.setReferenceMirror('Off');
    wave.setReferenceBrightness(1);
    wave.setReferencePalette('#ff0000,#00ff00,#0000ff');
    const novelty = [0.1, 0.3, 0.5, 0.7, 0.9];
    const expected = [
        [0.1, 0, 0],
        [0.151772, 0.148228, 0],
        [0, 0.5, 0],
        [0, 0.354134, 0.345866],
        [0, 0, 0.9]
    ];
    const waveMap = render(wave, 'audioenergy2.js', 5, 1, wgFrame({
        frameSequence: 1,
        banks: { full: { count: novelty.length, processed: [1, 1, 1, 1, 1], novelty } }
    }), 'worker-g wavelength golden');
    for (let i = 0; i < expected.length; i++)
        wgApproxRgb(wgRgb(waveMap, i), expected[i], 0.005, `row63 golden pixel ${i}`);

    const waveDecoy = render(wave, 'audioenergy2.js', 5, 1, wgFrame({
        frameSequence: 2,
        banks: { full: { count: novelty.length, processed: [0, 0, 0, 0, 0], novelty } }
    }), 'worker-g wavelength processed decoy');
    assert.deepStrictEqual(waveDecoy, waveMap, 'row63 processed-only change should not affect Reference Wavelength');

    wave.setReferenceRoll(0.5);
    const rollA = render(wave, 'audioenergy2.js', 5, 1, wgFrame({
        frameSequence: 3,
        timing: { deltaSeconds: 0.2 },
        banks: { full: { count: novelty.length, processed: [0, 0, 0, 0, 0], novelty } }
    }), 'worker-g wavelength roll A');
    const rollB = render(wave, 'audioenergy2.js', 5, 1, wgFrame({
        frameSequence: 4,
        timing: { deltaSeconds: 0.2 },
        banks: { full: { count: novelty.length, processed: [0, 0, 0, 0, 0], novelty } }
    }), 'worker-g wavelength roll B');
    assert.notDeepStrictEqual(rollA, rollB, 'row63 roll should move palette with fixed novelty');

    wave.setReferencePalette('#ff0000,#00ff00,#0000ff|0,0.1,1');
    const irregular = render(wave, 'audioenergy2.js', 5, 1, wgFrame({
        frameSequence: 5,
        timing: { deltaSeconds: 0 },
        banks: { full: { count: novelty.length, processed: [0, 0, 0, 0, 0], novelty } }
    }), 'worker-g wavelength irregular stops');
    assert.notDeepStrictEqual(irregular, waveMap, 'row63 irregular positions must alter gradient geometry');

    wave.setReferencePalette('#00ff00');
    const single = render(wave, 'audioenergy2.js', 1, 1, wgFrame({
        frameSequence: 6,
        banks: { full: { count: 1, processed: [0], novelty: [0.5] } }
    }), 'worker-g wavelength one stop');
    wgApproxRgb(wgRgb(single, 0), [0, 0.5, 0], 1e-6, 'row63 one stop N1');

    markCase('worker-g-row14-energy2-source-triangle');
    markCase('worker-g-row63-wavelength-golden-roll');
}

function wgScrollRows() {
    const processed = [0.2, 0.1, 0.5, 0.1, 0.2, 0.8, 0.3, 0.1, 0.2, 0.4];
    const silent = new Array(processed.length).fill(0);
    const ref = loadScript('audiobarcode.js', 0x707);
    ref.setMode('Reference');
    ref.setReferenceBlur(0);
    ref.setReferenceMirror('Off');
    ref.setReferenceBrightness(1);
    ref.setReferencePalette('#ff0000,#00ff00,#0000ff');
    ref.setReferenceSpeed(3);
    ref.setReferenceDecay(0.97);
    ref.setReferenceThreshold(0);

    const first = render(ref, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 1,
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll first');
    wgApproxRgb(wgRgb(first, 0), [0.04, 0.25, 0.64], 1e-6, 'row51 first color');
    wgApproxRgb(wgRgb(first, 2), [0.04, 0.25, 0.64], 1e-6, 'row51 first color span');

    const second = render(ref, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 2,
        banks: { full: { count: silent.length, processed: silent, novelty: silent } }
    }), 'worker-g scroll decay');
    wgApproxRgb(wgRgb(second, 3), [0.0388, 0.2425, 0.6208], 1e-4, 'row51 shifted retained');

    const thresholdPass = loadScript('audiobarcode.js', 0x708);
    thresholdPass.setMode('Reference');
    thresholdPass.setReferenceBlur(0);
    thresholdPass.setReferenceMirror('Off');
    thresholdPass.setReferenceBrightness(1);
    thresholdPass.setReferencePalette('#ff0000,#00ff00,#0000ff');
    thresholdPass.setReferenceSpeed(1);
    thresholdPass.setReferenceDecay(1);
    thresholdPass.setReferenceThreshold(1);
    const pass = render(thresholdPass, 'audiobarcode.js', 6, 1, wgFrame({
        frameSequence: 1,
        banks: { full: { count: 10, processed: [0.3162278, 0, 0, 0, 0, 0, 0, 0, 0, 0], novelty: silent } }
    }), 'worker-g scroll threshold pass');
    const thresholdFail = loadScript('audiobarcode.js', 0x708);
    thresholdFail.setMode('Reference');
    thresholdFail.setReferenceBlur(0);
    thresholdFail.setReferenceMirror('Off');
    thresholdFail.setReferenceBrightness(1);
    thresholdFail.setReferencePalette('#ff0000,#00ff00,#0000ff');
    thresholdFail.setReferenceSpeed(1);
    thresholdFail.setReferenceDecay(1);
    thresholdFail.setReferenceThreshold(1);
    const fail = render(thresholdFail, 'audiobarcode.js', 6, 1, wgFrame({
        frameSequence: 1,
        banks: { full: { count: 10, processed: [0.3, 0, 0, 0, 0, 0, 0, 0, 0, 0], novelty: silent } }
    }), 'worker-g scroll threshold fail');
    assert(wgPeakV(pass) > 0.001, 'row51 threshold pass case should emit color');
    assert(wgPeakV(fail) < 1e-9, 'row51 threshold fail case should be dark');

    const tiny = loadScript('audiobarcode.js', 0x709);
    tiny.setMode('Reference');
    tiny.setReferenceBlur(0);
    tiny.setReferenceMirror('Off');
    tiny.setReferenceBrightness(1);
    tiny.setReferencePalette('#ff0000,#00ff00,#0000ff');
    tiny.setReferenceSpeed(10);
    tiny.setReferenceDecay(1);
    const tinyMap = render(tiny, 'audiobarcode.js', 2, 1, wgFrame({
        frameSequence: 1,
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll tiny speed bound');
    assert(wgPeakV(tinyMap) > 0.001, 'row51 tiny strip should still fill when speed>N');

    const plus = loadScript('audiobarcode.js', 0x70a);
    plus.setMode('Scroll+');
    plus.setReferenceBlur(0);
    plus.setReferenceMirror('Off');
    plus.setReferenceBrightness(1);
    plus.setReferencePalette('#ff0000,#00ff00,#0000ff');
    plus.setScrollPlusSpeed(0.5);
    plus.setScrollPlusDecay(0.5);
    plus.setScrollPlusThreshold(0.1);

    const half = render(plus, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll+ half shift');
    assert(wgPeakV(half) < 1e-9, 'row52 first half-step should not inject');

    const full = render(plus, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll+ full shift');
    assert(wgPeakV(full) > 0.4, 'row52 second half-step should inject one color');

    const plusX = loadScript('audiobarcode.js', 0x70a1);
    plusX.setMode('Scroll+');
    plusX.setReferencePalette('#ff0000,#00ff00,#0000ff');
    plusX.setReferenceBlur(1.5);
    plusX.setReferenceMirror('On');
    plusX.setReferenceBrightness(0.7);
    plusX.setScrollPlusSpeed(0.5);
    plusX.setScrollPlusDecay(0.5);
    plusX.setScrollPlusThreshold(0.1);

    render(plusX, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll+ x half shift');
    const fullX = render(plusX, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll+ x full shift');
    assert.notDeepStrictEqual(fullX, full, 'row52 nondefault X controls should alter scroll+ output');
    assert(wgPeakV(fullX) > 0.15, 'row52 nondefault X output should remain visibly energized');

    const decayA = render(plus, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 3,
        timing: { deltaSeconds: 0.05 },
        banks: { full: { count: silent.length, processed: silent, novelty: silent } }
    }), 'worker-g scroll+ decay A');
    const decayB = render(plus, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 4,
        timing: { deltaSeconds: 0.05 },
        banks: { full: { count: silent.length, processed: silent, novelty: silent } }
    }), 'worker-g scroll+ decay B');
    const splitRatio = wgPixelV(decayB, 0) / wgPixelV(full, 0);
    wgApprox(splitRatio, 0.855625, 1e-6, 'row52 fractional partition decay two half-steps');

    const single = loadScript('audiobarcode.js', 0x70a2);
    single.setMode('Scroll+');
    single.setReferenceBlur(0);
    single.setReferenceMirror('Off');
    single.setReferenceBrightness(1);
    single.setReferencePalette('#ff0000,#00ff00,#0000ff');
    single.setScrollPlusSpeed(0.5);
    single.setScrollPlusDecay(0.5);
    single.setScrollPlusThreshold(0.1);
    render(single, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll+ single half shift');
    const singleFull = render(single, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll+ single full shift');
    const singleDecay = render(single, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 3,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: silent.length, processed: silent, novelty: silent } }
    }), 'worker-g scroll+ single decay');
    const singleRatio = wgPixelV(singleDecay, 0) / wgPixelV(singleFull, 0);
    wgApprox(singleRatio, 0.85, 1e-6, 'row52 single-step decay');
    assert(Math.abs(splitRatio - singleRatio) > 0.005, 'row52 split-vs-single decay must be distinguishable');

    const zeroRepeat = render(plus, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 4,
        timing: { deltaSeconds: 0 },
        banks: { full: { count: silent.length, processed: silent, novelty: silent } }
    }), 'worker-g scroll+ zero repeat');
    assert.deepStrictEqual(zeroRepeat, decayB, 'row52 zero-delta duplicate changed state');

    const bounded = loadScript('audiobarcode.js', 0x70b);
    bounded.setMode('Scroll+');
    bounded.setReferencePalette('#ff0000,#00ff00,#0000ff');
    bounded.setScrollPlusSpeed(2);
    bounded.setScrollPlusDecay(2);
    bounded.setScrollPlusThreshold(0);
    const big = render(bounded, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 1 },
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll+ large dt bound');
    assert(wgPeakV(big) > 0.001, 'row52 large dt should still inject bounded strip color');
    const cleared = render(bounded, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 1 },
        banks: { full: { count: silent.length, processed: silent, novelty: silent } }
    }), 'worker-g scroll+ large dt clear');
    assert(wgPeakV(cleared) < wgPeakV(big), 'row52 linear decay should clear stale colors at high dt');

    const timingRecover = loadScript('audiobarcode.js', 0x70a3);
    timingRecover.setMode('Scroll+');
    timingRecover.setReferenceBlur(0);
    timingRecover.setReferenceMirror('Off');
    timingRecover.setReferenceBrightness(1);
    timingRecover.setReferencePalette('#ff0000,#00ff00,#0000ff');
    timingRecover.setScrollPlusSpeed(0.5);
    timingRecover.setScrollPlusDecay(0.5);
    timingRecover.setScrollPlusThreshold(0.1);
    const invalidTiming = render(timingRecover, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 1,
        dt: Number.NaN,
        timing: { deltaSeconds: Number.NaN },
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll+ invalid timing');
    wgAssertFiniteMap(invalidTiming, 'row52 invalid timing frame');
    const recoverHalf = render(timingRecover, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll+ recover half');
    assert(wgPeakV(recoverHalf) < 1e-9, 'row52 post-invalid first valid frame should be half-step dark');
    const recoverFull = render(timingRecover, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 3,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: processed.length, processed, novelty: silent } }
    }), 'worker-g scroll+ recover full');
    assert(wgPeakV(recoverFull) > 0.4, 'row52 post-invalid valid frame should recover full-step injection');

    markCase('worker-g-row51-scroll-maxima-threshold-retention');
    markCase('worker-g-row52-scroll-plus-fractional-delta');
    markCase('worker-g-row52-scroll-plus-nondefault-x-forwarding');
    markCase('worker-g-row52-scroll-plus-single-vs-split-decay');
    markCase('worker-g-row52-scroll-plus-invalid-timing-recovery');
}

function wgSpectrumRow() {
    const expected = [0.0784313725, 0.1960784314, 0.4901960784];

    for (let mix = 0; mix < 6; mix++) {
        const algo = loadScript('audiospectrum.js', 0x70c + mix);
        algo.setMode('Reference');
        algo.setReferenceBlur(0);
        algo.setReferenceMirror('Off');
        algo.setReferenceBrightness(1);
        algo.setRgbMix(mix);
        algo.referenceDiagnostics = true;
        render(algo, 'audiospectrum.js', 1, 1, wgFrame({
            frameSequence: 1,
            banks: { full: { count: 1, processed: [0.1], novelty: [0] } }
        }), `worker-g spectrum mix ${mix} prime`);
        const mapped = render(algo, 'audiospectrum.js', 1, 1, wgFrame({
            frameSequence: 2,
            banks: { full: { count: 1, processed: [0.15], novelty: [0.02] } }
        }), `worker-g spectrum mix ${mix} expected`);
        const rgb = wgRgb(mapped, 0);
        const perm = [
            [0, 1, 2], [0, 2, 1], [1, 0, 2],
            [1, 2, 0], [2, 0, 1], [2, 1, 0]
        ][mix];
        wgApprox(rgb[0], expected[perm.indexOf(0)], 1e-6, `row56 mix ${mix} red`);
        wgApprox(rgb[1], expected[perm.indexOf(1)], 1e-6, `row56 mix ${mix} green`);
        wgApprox(rgb[2], expected[perm.indexOf(2)], 1e-6, `row56 mix ${mix} blue`);

        const held = render(algo, 'audiospectrum.js', 1, 1, wgFrame({
            frameSequence: 3,
            banks: { full: { count: 1, processed: [0.15], novelty: [0] } }
        }), `worker-g spectrum mix ${mix} held`);
        const heldRgb = wgRgb(held, 0);
        wgApprox(heldRgb[perm[1]], 0, 1e-6, `row56 mix ${mix} held difference cleared`);
        assert(heldRgb[perm[2]] > 0.49, `row56 mix ${mix} held envelope must remain`);

        const fOnlyA = render(algo, 'audiospectrum.js', 1, 1, wgFrame({
            frameSequence: 4,
            banks: { full: { count: 1, processed: [0.15], novelty: [0.02] } }
        }), `worker-g spectrum mix ${mix} F-only A`);
        const fOnlyB = render(algo, 'audiospectrum.js', 1, 1, wgFrame({
            frameSequence: 5,
            banks: { full: { count: 2, processed: [0.15, 0.15], novelty: [0.04, 0.04] } }
        }), `worker-g spectrum mix ${mix} F-only B`);
        const aRgb = wgRgb(fOnlyA, 0);
        const bRgb = wgRgb(fOnlyB, 0);
        assert(bRgb[perm[0]] > aRgb[perm[0]], `row56 mix ${mix} F-only change should raise novelty channel`);

        render(algo, 'audiospectrum.js', 1, 1, wgFrame({
            frameSequence: 6,
            banks: { full: { count: 1, processed: [0.15], novelty: [0.5] } }
        }), `worker-g spectrum mix ${mix} overdrive`);
        assert(algo.referenceFrame && algo.referenceFrame.pre && algo.referenceFrame.pre[0][perm[0]] > 1,
            `row56 mix ${mix} should keep >1 intermediates before final clip`);
    }

    markCase('worker-g-row56-spectrum-six-permutations');
}

function wgStrobeRows() {
    const reference = loadScript('audiostrobe.js', 0x70d);
    reference.setMode('Reference');
    reference.setReferenceBlur(0);
    reference.setReferenceMirror('Off');
    reference.setReferenceBrightness(1);
    reference.setReferencePalette('#ff0000,#00ff00,#0000ff');
    reference.setReferencePulseCount(2);
    reference.setReferenceStrobeDecay(1.5);
    reference.setReferenceBeatDecay(2);
    reference.setReferencePattern('****');

    const baseline = render(reference, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 1,
        tempo: { valid: true, bpm: 120, beatPhase: 0.25, barPhase: 0 }
    }), 'worker-g strobe baseline');
    wgApprox(wgRgb(baseline, 0)[0], 0.03955078125, 1e-9, 'row58 baseline red amplitude');
    wgApprox(wgRgb(baseline, 0)[1], 0, 1e-9, 'row58 baseline green');
    wgApprox(wgRgb(baseline, 0)[2], 0, 1e-9, 'row58 baseline blue');

    const phaseBoundary = render(reference, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 2,
        tempo: { valid: true, bpm: 120, beatPhase: 0, barPhase: 0 }
    }), 'worker-g strobe phase boundary');
    assert(wgPeakV(phaseBoundary) < 1e-9, 'row58 pulse boundary phase=0 should be dark');

    reference.setReferencePulseCount(32);
    const pulse32 = render(reference, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 3,
        tempo: { valid: true, bpm: 120, beatPhase: 0.17, barPhase: 0.1 }
    }), 'worker-g strobe pulse32');
    assert(wgPeakV(pulse32) > 1e-6, 'row58 pulse32 should emit non-zero output');
    reference.setReferencePulseCount(2);

    const masks = ['****', '*.*.', '.*.*', '*...', '...*'];
    const bars = [0.0, 0.26, 0.51, 0.76];
    for (const mask of masks) {
        reference.setReferencePattern(mask);
        for (let i = 0; i < bars.length; i++) {
            const map = render(reference, 'audiostrobe.js', 8, 1, wgFrame({
                frameSequence: 10 + i,
                tempo: { valid: true, bpm: 120, beatPhase: 0.25, barPhase: bars[i] }
            }), `worker-g strobe mask ${mask} beat ${i}`);
            const lit = wgPeakV(map) > 1e-8;
            const expected = mask.charAt(i) === '*';
            assert.strictEqual(lit, expected,
                `row58 mask ${mask} beat ${i} expected ${expected ? 'lit' : 'dark'}`);
        }
    }

    reference.setReferencePattern('****');
    const invalidDetected = render(reference, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 20,
        tempo: { valid: false, bpm: 0, beatPhase: 0.25, barPhase: 0 }
    }), 'worker-g strobe invalid detected');
    assert(wgPeakV(invalidDetected) < 1e-9, 'row58 detected clock invalid must be black');
    reference.setReferenceClock('Extrapolated');
    const invalidExtrapolated = render(reference, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 21,
        tempo: { valid: false, bpm: 0, beatPhase: 0.25, barPhase: 0 }
    }), 'worker-g strobe invalid extrapolated');
    assert(wgPeakV(invalidExtrapolated) > 1e-6, 'row58 extrapolated clock must still run on phase');

    const percussive = loadScript('audiostrobe.js', 0x70e);
    percussive.setMode('Percussive RGB');
    percussive.setStrobeColor('#0000ff');
    percussive.setStrobeWidth(2);
    percussive.setStrobeDecayRate(1);
    percussive.setBassStrobeDecayRate(0);
    percussive.setColorShiftDelay(1);
    percussive.colors = [{ h: 0, s: 1, v: 1 }, { h: 0, s: 1, v: 1 }, { h: 0, s: 1, v: 1 }];

    const magenta = render(percussive, 'audiostrobe.js', 4, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.25 },
        tempo: { valid: true, bpm: 120, beatPhase: 0, barPhase: 0 },
        kickFired: true,
        events: { delta: { onset: 1, beat: 0, kick: 1, bar: 0 } }
    }), 'worker-g percussive magenta');
    const magentaRgb = hsvPixels(magenta);
    assert(magentaRgb.some(pixel => pixel[0] > 0.99 && pixel[2] > 0.99),
        'row47 blue onset over red bass should produce magenta overlap');

    const afterDecay = render(percussive, 'audiostrobe.js', 4, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        kickFired: false,
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive independent lifetime');
    const afterRgb = hsvPixels(afterDecay);
    assert(afterRgb.some(pixel => pixel[0] > 0.99 && pixel[2] < 1e-6),
        'row47 percussion decay should clear blue while bass red remains');

    const cooldown = loadScript('audiostrobe.js', 0x70f);
    cooldown.setMode('Percussive RGB');
    cooldown.setStrobeWidth(0);
    cooldown.setBassStrobeDecayRate(0.9);
    cooldown.colors = [{ h: 0, s: 1, v: 1 }, { h: 0, s: 1, v: 1 }, { h: 0, s: 1, v: 1 }];
    const kickA = render(cooldown, 'audiostrobe.js', 6, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.25 },
        kickFired: true,
        events: { delta: { onset: 0, beat: 0, kick: 1, bar: 0 } }
    }), 'worker-g percussive cooldown first');
    const kickB = render(cooldown, 'audiostrobe.js', 6, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.05 },
        kickFired: true,
        events: { delta: { onset: 0, beat: 0, kick: 1, bar: 0 } }
    }), 'worker-g percussive cooldown blocked');
    assert(wgPeakV(kickB) < wgPeakV(kickA), 'row47 200ms cooldown should block immediate re-trigger');

    const widthZero = loadScript('audiostrobe.js', 0x710);
    widthZero.setMode('Percussive RGB');
    widthZero.setStrobeWidth(0);
    widthZero.setBassStrobeDecayRate(1);
    const noPatch = render(widthZero, 'audiostrobe.js', 6, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive width0');
    assert(wgPeakV(noPatch) < 1e-9, 'row47 width0 should disable percussion patch');

    const widthFull = loadScript('audiostrobe.js', 0x711);
    widthFull.setMode('Percussive RGB');
    widthFull.setStrobeColor('#00ff00');
    widthFull.setStrobeWidth(1000);
    widthFull.setBassStrobeDecayRate(1);
    const fullPatch = render(widthFull, 'audiostrobe.js', 6, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive width full');
    const fullRgb = hsvPixels(fullPatch);
    assert(fullRgb.every(pixel => pixel[1] > 0.99 && pixel[0] < 1e-6 && pixel[2] < 1e-6),
        'row47 width>=N should fill full strip with onset color');

    const noBassInsert = loadScript('audiostrobe.js', 0x712);
    noBassInsert.setMode('Percussive RGB');
    noBassInsert.setStrobeWidth(0);
    noBassInsert.setBassStrobeDecayRate(1);
    const darkKick = render(noBassInsert, 'audiostrobe.js', 6, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.3 },
        kickFired: true,
        events: { delta: { onset: 0, beat: 0, kick: 1, bar: 0 } }
    }), 'worker-g percussive bass decay suppress');
    assert(wgPeakV(darkKick) < 1e-9, 'row47 bass decay=1 should suppress bass insertion');

    const queue = loadScript('audiostrobe.js', 0x713);
    queue.setMode('Percussive RGB');
    queue.setStrobeColor('#0000ff');
    queue.setStrobeWidth(1);
    queue.setBassStrobeDecayRate(1);
    const queued = render(queue, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 1 / 60 },
        events: { delta: { onset: 3, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive queue first');
    const queued2 = render(queue, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0 },
        events: { delta: { onset: 3, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive queue duplicate');
    assert(wgPeakV(queued2) < wgPeakV(queued), 'row47 duplicate counters should not respawn full-strength onset patch');
    const queued3 = render(queue, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0 },
        events: { delta: { onset: 3, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive queue duplicate repeat');
    assert.deepStrictEqual(queued3, queued2, 'row47 zero-time duplicate should be stable after first consume');

    const identity = loadScript('audiostrobe.js', 0x7140);
    identity.setMode('Percussive RGB');
    identity.setStrobeColor('#00ff00');
    identity.setStrobeWidth(2);
    identity.setStrobeDecayRate(0);
    identity.setBassStrobeDecayRate(1);
    const idLit = render(identity, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        sourceId: 'stable-source',
        profileId: 220,
        sourceEpoch: 5,
        configRevision: 9,
        events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive identity lit');
    assert(wgPeakV(idLit) > 0.9, 'row47 identity setup should create lit onset patch');
    const idTransient = render(identity, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        sourceId: 'stable-source',
        profileId: 220,
        sourceEpoch: 5,
        configRevision: 9,
        banks: { full: { count: 0, processed: [], novelty: [] } },
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive identity transient');
    assert(wgPeakV(idTransient) > 0.9, 'row47 transient loss should retain same-source overlay state');
    const idRebind = render(identity, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        sourceId: 'stable-source',
        profileId: 220,
        sourceEpoch: 6,
        configRevision: 9,
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive identity rebind');
    assert(wgPeakV(idRebind) < 1e-9, 'row47 true source rebind should clear overlays and queue');

    const geometry = loadScript('audiostrobe.js', 0x7141);
    geometry.setMode('Percussive RGB');
    geometry.setStrobeColor('#00ff00');
    geometry.setStrobeWidth(2);
    geometry.setStrobeDecayRate(0);
    geometry.setBassStrobeDecayRate(1);
    const geoLit = render(geometry, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        sourceId: 'geom-source',
        profileId: 221,
        sourceEpoch: 1,
        configRevision: 1,
        events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive geometry lit');
    assert(wgPeakV(geoLit) > 0.9, 'row47 geometry setup should create lit onset patch');
    const geoHeightChanged = render(geometry, 'audiostrobe.js', 8, 2, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        sourceId: 'geom-source',
        profileId: 221,
        sourceEpoch: 1,
        configRevision: 1,
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive geometry height change');
    assert(wgPeakV(geoHeightChanged) < 1e-9, 'row47 height change should clear overlays');

    const timeColor = loadScript('audiostrobe.js', 0x714);
    timeColor.setMode('Percussive RGB');
    timeColor.setStrobeWidth(0);
    timeColor.setBassStrobeDecayRate(0);
    timeColor.setColorShiftDelay(0.05);
    timeColor.setColorStep(0.25);
    const c1 = render(timeColor, 'audiostrobe.js', 6, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.25 },
        kickFired: true,
        events: { delta: { onset: 0, beat: 0, kick: 1, bar: 0 } }
    }), 'worker-g percussive color step first');
    render(timeColor, 'audiostrobe.js', 6, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g percussive color step wait');
    const c2 = render(timeColor, 'audiostrobe.js', 6, 1, wgFrame({
        frameSequence: 3,
        timing: { deltaSeconds: 0.25 },
        kickFired: true,
        events: { delta: { onset: 0, beat: 0, kick: 1, bar: 0 } }
    }), 'worker-g percussive color step second');
    assert.notDeepStrictEqual(c1, c2, 'row47 time-only palette stepping should affect later kick color');

    markCase('worker-g-row58-bpm-strobe-reference-controls');
    markCase('worker-g-row47-percussive-rgb-branches');
    markCase('worker-g-row47-percussive-identity-geometry-resets');
}

function wgXAndOptionsForwarding() {
    function peakV(map) {
        let peak = 0;
        for (let i = 2; i < map.length; i += 3) peak = Math.max(peak, map[i]);
        return peak;
    }
    function withRef(file, seed) {
        const algo = loadScript(file, seed);
        algo.setMode('Reference');
        algo.setReferenceBlur(0);
        algo.setReferenceMirror('Off');
        algo.setReferenceBrightness(1);
        algo.setReferenceFlip('Off');
        algo.setReferenceBackgroundMode('Off');
        algo.setReferenceBackgroundColor('#000000');
        algo.setReferenceBackgroundBrightness(1);
        return algo;
    }
    function assertXForward(file, seed, frame, width, height, opts) {
        opts = opts || {};
        const base = withRef(file, seed);
        const mBase = render(base, file, width, height, frame, `${file} x base`);
        if (opts.flip !== false) {
            const flip = withRef(file, seed);
            flip.setReferenceFlip('On');
            const mFlip = render(flip, file, width, height, frame, `${file} x flip`);
            assert.notDeepStrictEqual(mFlip, mBase, `${file}: referenceFlip not forwarded`);
        }
        if (opts.mirror !== false) {
            const mirror = withRef(file, seed);
            mirror.setReferenceMirror('On');
            const mMirror = render(mirror, file, width, height, frame, `${file} x mirror`);
            assert.notDeepStrictEqual(mMirror, mBase, `${file}: referenceMirror not forwarded`);
        }
        const bright = withRef(file, seed);
        bright.setReferenceBrightness(0.4);
        const mBright = render(bright, file, width, height, frame, `${file} x brightness`);
        assert.notDeepStrictEqual(mBright, mBase, `${file}: referenceBrightness not forwarded`);
        if (opts.blur !== false) {
            const blur = withRef(file, seed);
            blur.setReferenceBlur(2);
            const mBlur = render(blur, file, width, height, frame, `${file} x blur`);
            assert.notDeepStrictEqual(mBlur, mBase, `${file}: referenceBlur not forwarded`);
        }
        const bg = withRef(file, seed);
        bg.setReferenceBackgroundMode('Additive');
        bg.setReferenceBackgroundColor('#00ff00');
        bg.setReferenceBackgroundBrightness(1);
        const mBg = render(bg, file, width, height, frame, `${file} x bg`);
        assert.notDeepStrictEqual(mBg, mBase, `${file}: referenceBackground not forwarded`);
    }

    const energyFrame = wgFrame({
        frameSequence: 1,
        banks: { full: { count: 10, processed: [0.7, 0.6, 0.3, 0.2, 0.2, 0.6, 0.4, 0.2, 0.1, 0.1], novelty: new Array(10).fill(0) } }
    });
    assertXForward('audioenergy.js', 0x721, energyFrame, 8, 1);
    const energyRangeA = withRef('audioenergy.js', 0x722);
    const energyRangeB = withRef('audioenergy.js', 0x722);
    energyRangeA.setReferenceRangeStart(0);
    energyRangeA.setReferenceRangeEnd(0.4);
    energyRangeB.setReferenceRangeStart(0.6);
    energyRangeB.setReferenceRangeEnd(1);
    assert.notDeepStrictEqual(
        render(energyRangeA, 'audioenergy.js', 8, 1, energyFrame, 'energy range A'),
        render(energyRangeB, 'audioenergy.js', 8, 1, energyFrame, 'energy range B'),
        'audioenergy.js: reference range not forwarded'
    );

    const waveFrame = wgFrame({
        frameSequence: 1,
        banks: { full: { count: 10, processed: new Array(10).fill(0), novelty: [0.1, 0.3, 0.5, 0.7, 0.9, 0.4, 0.2, 0.6, 0.8, 0.1] } }
    });
    assertXForward('audioenergy2.js', 0x723, waveFrame, 10, 1);
    const wavePaletteA = withRef('audioenergy2.js', 0x724);
    const wavePaletteB = withRef('audioenergy2.js', 0x724);
    wavePaletteA.setReferencePalette('#ff0000,#00ff00,#0000ff|0,0.5,1');
    wavePaletteB.setReferencePalette('#ff0000,#00ff00,#0000ff|0,0.1,1');
    assert.notDeepStrictEqual(
        render(wavePaletteA, 'audioenergy2.js', 10, 1, waveFrame, 'wavelength positions A'),
        render(wavePaletteB, 'audioenergy2.js', 10, 1, waveFrame, 'wavelength positions B'),
        'audioenergy2.js: positioned G not forwarded'
    );
    const waveRoll = withRef('audioenergy2.js', 0x725);
    waveRoll.setReferenceRoll(0.5);
    const roll1 = render(waveRoll, 'audioenergy2.js', 10, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.2 },
        banks: waveFrame.banks
    }), 'wavelength roll 1');
    const roll2 = render(waveRoll, 'audioenergy2.js', 10, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.2 },
        banks: waveFrame.banks
    }), 'wavelength roll 2');
    assert.notDeepStrictEqual(roll1, roll2, 'audioenergy2.js: referenceRoll not forwarded');

    const barcodeFrame = wgFrame({
        frameSequence: 1,
        banks: { full: { count: 10, processed: [0.8, 0.7, 0.6, 0.1, 0.1, 0.4, 0.4, 0.2, 0.1, 0.1], novelty: new Array(10).fill(0) } }
    });
    assertXForward('audiobarcode.js', 0x726, barcodeFrame, 10, 1);
    const scrollRangeA = withRef('audiobarcode.js', 0x727);
    const scrollRangeB = withRef('audiobarcode.js', 0x727);
    scrollRangeA.setReferenceRangeStart(0);
    scrollRangeA.setReferenceRangeEnd(0.3);
    scrollRangeB.setReferenceRangeStart(0.7);
    scrollRangeB.setReferenceRangeEnd(1);
    assert.notDeepStrictEqual(
        render(scrollRangeA, 'audiobarcode.js', 10, 1, barcodeFrame, 'scroll range A'),
        render(scrollRangeB, 'audiobarcode.js', 10, 1, barcodeFrame, 'scroll range B'),
        'audiobarcode.js: reference range not forwarded'
    );

    const spectrumFrame = wgFrame({
        frameSequence: 1,
        banks: { full: { count: 10, processed: [0.005, 0.01, 0.02, 0.015, 0.01, 0.012, 0.011, 0.008, 0.006, 0.004], novelty: [0.001, 0.002, 0.003, 0.002, 0.002, 0.0015, 0.001, 0.001, 0.001, 0.0005] } }
    });
    assertXForward('audiospectrum.js', 0x728, spectrumFrame, 10, 1);
    const spectrumA = withRef('audiospectrum.js', 0x729);
    const spectrumB = withRef('audiospectrum.js', 0x729);
    spectrumA.setReferenceRangeStart(0);
    spectrumA.setReferenceRangeEnd(0.4);
    spectrumB.setReferenceRangeStart(0.6);
    spectrumB.setReferenceRangeEnd(1);
    spectrumA.setRgbMix(5);
    spectrumB.setRgbMix(5);
    render(spectrumA, 'audiospectrum.js', 10, 1, spectrumFrame, 'spectrum range prime A');
    render(spectrumB, 'audiospectrum.js', 10, 1, spectrumFrame, 'spectrum range prime B');
    assert.notDeepStrictEqual(
        render(spectrumA, 'audiospectrum.js', 10, 1, Object.assign({}, spectrumFrame, { frameSequence: 2 }), 'spectrum range A'),
        render(spectrumB, 'audiospectrum.js', 10, 1, Object.assign({}, spectrumFrame, { frameSequence: 2 }), 'spectrum range B'),
        'audiospectrum.js: reference range not forwarded'
    );

    const strobeFrame = wgFrame({
        frameSequence: 1,
        tempo: { valid: true, bpm: 120, beatPhase: 0.25, barPhase: 0.35 }
    });
    assertXForward('audiostrobe.js', 0x72a, strobeFrame, 8, 1, { flip: false, mirror: false, blur: false });
    const strobePaletteA = withRef('audiostrobe.js', 0x72b);
    const strobePaletteB = withRef('audiostrobe.js', 0x72b);
    strobePaletteA.setReferencePositions('0,0.5,1');
    strobePaletteB.setReferencePositions('0,0.1,1');
    assert.notDeepStrictEqual(
        render(strobePaletteA, 'audiostrobe.js', 8, 1, strobeFrame, 'strobe positions A'),
        render(strobePaletteB, 'audiostrobe.js', 8, 1, strobeFrame, 'strobe positions B'),
        'audiostrobe.js: positioned G not forwarded'
    );
    const strobeRoll = withRef('audiostrobe.js', 0x72c);
    strobeRoll.setReferenceRoll(0.5);
    const sr1 = render(strobeRoll, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.2 },
        tempo: { valid: true, bpm: 120, beatPhase: 0.25, barPhase: 0.35 }
    }), 'strobe roll 1');
    const sr2 = render(strobeRoll, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.2 },
        tempo: { valid: true, bpm: 120, beatPhase: 0.25, barPhase: 0.35 }
    }), 'strobe roll 2');
    assert.notDeepStrictEqual(sr1, sr2, 'audiostrobe.js: referenceRoll not forwarded');

    markCase('worker-g-x-forwarding-all-five');
    markCase('worker-g-g-range-roll-forwarding');
}

function runWorkerGRows() {
    wgEnergyRows();
    wgEnergy2AndWavelengthRows();
    wgScrollRows();
    wgSpectrumRow();
    wgStrobeRows();
    wgXAndOptionsForwarding();
}

runWorkerGRows();
}

function assertWorkerBOutputRows() {
    function workerBOutputAudio(overrides) {
        return audio(Object.assign({
            version: 6,
            sourceId: 'worker-b-output-source',
            profileId: 420,
            sourceEpoch: 1,
            configRevision: 1,
            dt: 0,
            timing: { deltaSeconds: 0 },
            low: 0,
            mid: 0,
            high: 0,
            beat: 0,
            bass: 0,
            powers: { raw: { low: 0, mid: 0, high: 0, bass: 0, beat: 0 } },
            tempo: { beatPhase: 0, valid: false },
            banks: { full: { count: 4, processed: [0, 0, 0, 0], novelty: [0, 0, 0, 0] } }
        }, overrides || {}));
    }

    function mapAt(map, width, x, y) {
        var i = (y * width + x) * 3;
        return { h: map[i], s: map[i + 1], v: map[i + 2] };
    }

    function maxDelta(a, b) {
        assert.strictEqual(a.length, b.length, 'map length mismatch');
        var m = 0;
        for (var i = 0; i < a.length; i++) m = Math.max(m, Math.abs(a[i] - b[i]));
        return m;
    }

    function assertMapClose(actual, expected, label, eps) {
        var delta = maxDelta(actual, expected);
        assert(delta <= (eps || 1e-6), label + ' max delta ' + delta);
    }

    function clamp01(v) {
        return v < 0 ? 0 : (v > 1 ? 1 : v);
    }

    function mod1(v) {
        var m = v - Math.floor(v);
        return m < 0 ? m + 1 : m;
    }

    function hsvToRgb(h, s, v) {
        h = mod1(h);
        s = clamp01(s);
        v = Math.max(0, v);
        var i = Math.floor(h * 6);
        var f = h * 6 - i;
        var p = v * (1 - s);
        var q = v * (1 - f * s);
        var t = v * (1 - (1 - f) * s);
        switch (i % 6) {
            case 0: return [v, t, p];
            case 1: return [q, v, p];
            case 2: return [p, v, t];
            case 3: return [p, q, v];
            case 4: return [t, p, v];
            default: return [v, p, q];
        }
    }

    function rgbToHsv(r, g, b) {
        r = clamp01(r);
        g = clamp01(g);
        b = clamp01(b);
        var max = Math.max(r, g, b);
        var min = Math.min(r, g, b);
        var delta = max - min;
        var h = 0;
        if (delta > 0) {
            if (max === r) h = ((g - b) / delta) / 6;
            else if (max === g) h = (2 + (b - r) / delta) / 6;
            else h = (4 + (r - g) / delta) / 6;
        }
        return { h: mod1(h), s: max > 0 ? delta / max : 0, v: max };
    }

    function parseHexRgb(hex) {
        var ok = typeof hex === 'string' && /^#[0-9a-f]{6}$/i.test(hex);
        if (!ok) return [0, 0, 0];
        return [
            parseInt(hex.slice(1, 3), 16) / 255,
            parseInt(hex.slice(3, 5), 16) / 255,
            parseInt(hex.slice(5, 7), 16) / 255
        ];
    }

    function expectedWithTransforms(baseMap, width, height, opts) {
        opts = opts || {};
        var n = width * height;
        if (n <= 0) return Array.from(baseMap);
        var brightness = Number(opts.brightness);
        if (!isFinite(brightness)) brightness = 1;
        brightness = clamp01(brightness);
        var sigma = Number(opts.blur);
        if (!isFinite(sigma)) sigma = 0;
        sigma = Math.max(0, Math.min(10, sigma));

        var pixels = new Array(n);
        for (var i = 0; i < n; i++) {
            var o = i * 3;
            pixels[i] = hsvToRgb(baseMap[o], baseMap[o + 1], baseMap[o + 2]);
        }

        if (opts.flip === 'On') pixels.reverse();
        if (opts.mirror === 'On') {
            var mirrored = new Array(n * 2);
            for (var m = 0; m < n; m++) {
                mirrored[m] = pixels[n - 1 - m];
                mirrored[n + m] = pixels[m];
            }
            var folded = new Array(n);
            for (var f = 0; f < n; f++) {
                var a = mirrored[f * 2];
                var b = mirrored[f * 2 + 1];
                folded[f] = [
                    Math.max(a[0], b[0]),
                    Math.max(a[1], b[1]),
                    Math.max(a[2], b[2])
                ];
            }
            pixels = folded;
        }

        if (opts.backgroundMode === 'Additive') {
            var bg = parseHexRgb(opts.backgroundColor || '#000000');
            var bgScale = Number(opts.backgroundBrightness);
            if (!isFinite(bgScale)) bgScale = 1;
            bgScale = clamp01(bgScale);
            for (var bgi = 0; bgi < n; bgi++) {
                pixels[bgi][0] += bg[0] * bgScale;
                pixels[bgi][1] += bg[1] * bgScale;
                pixels[bgi][2] += bg[2] * bgScale;
            }
        }

        for (var bi = 0; bi < n; bi++) {
            pixels[bi][0] *= brightness;
            pixels[bi][1] *= brightness;
            pixels[bi][2] *= brightness;
        }

        if (sigma > 0 && n > 3) {
            var radius = Math.max(1, Math.min(Math.floor((n - 1) / 2), Math.round(4 * sigma)));
            var kernel = new Array(radius * 2 + 1);
            var sum = 0;
            for (var d = -radius; d <= radius; d++) {
                var w = Math.exp(-(d * d) / (2 * sigma * sigma));
                kernel[d + radius] = w;
                sum += w;
            }
            for (var k = 0; k < kernel.length; k++) kernel[k] /= sum;
            var blurred = new Array(n);
            for (var pi = 0; pi < n; pi++) {
                var acc = [0, 0, 0];
                for (var dd = -radius; dd <= radius; dd++) {
                    var idx = pi + dd;
                    if (idx < 0 || idx >= n) continue;
                    var kw = kernel[dd + radius];
                    acc[0] += pixels[idx][0] * kw;
                    acc[1] += pixels[idx][1] * kw;
                    acc[2] += pixels[idx][2] * kw;
                }
                blurred[pi] = acc;
            }
            pixels = blurred;
        }

        var out = new Array(baseMap.length);
        for (var oi = 0; oi < n; oi++) {
            var hsv = rgbToHsv(pixels[oi][0], pixels[oi][1], pixels[oi][2]);
            var oo = oi * 3;
            out[oo] = hsv.h;
            out[oo + 1] = hsv.s;
            out[oo + 2] = hsv.v;
        }
        return out;
    }

    function manualFlipMirror(baseMap, width, height) {
        var n = width * height;
        var pixels = new Array(n);
        for (var i = 0; i < n; i++) {
            var o = i * 3;
            pixels[i] = hsvToRgb(baseMap[o], baseMap[o + 1], baseMap[o + 2]);
        }
        pixels.reverse();
        var mirrored = new Array(n * 2);
        for (var j = 0; j < n; j++) {
            mirrored[j] = pixels[n - 1 - j];
            mirrored[n + j] = pixels[j];
        }
        var out = new Array(baseMap.length);
        for (var k = 0; k < n; k++) {
            var a = mirrored[k * 2];
            var b = mirrored[k * 2 + 1];
            var hsv = rgbToHsv(
                Math.max(a[0], b[0]),
                Math.max(a[1], b[1]),
                Math.max(a[2], b[2])
            );
            var p = k * 3;
            out[p] = hsv.h;
            out[p + 1] = hsv.s;
            out[p + 2] = hsv.v;
        }
        return out;
    }

    function assertXMetadata(scriptFile) {
        var algo = loadScript(scriptFile, 0xB00 + scriptFile.length);
        var props = algo.properties.map(parseProperty);
        var seenNames = {};
        for (var i = 0; i < props.length; i++) {
            var name = props[i].name;
            assert(!seenNames[name], scriptFile + ': duplicate property name ' + name);
            seenNames[name] = true;
        }
        var expected = [
            { name: 'flip', display: 'Flip', read: 'getFlip' },
            { name: 'mirror', display: 'Mirror', read: 'getMirror' },
            { name: 'backgroundMode', display: 'Background Mode', read: 'getBackgroundMode' },
            { name: 'backgroundColor', display: 'Background Color', read: 'getBackgroundColor' },
            { name: 'backgroundBrightness', display: 'Background Brightness', read: 'getBackgroundBrightness' },
            { name: 'brightness', display: 'Brightness', read: 'getBrightness' },
            { name: 'blur', display: 'Blur', read: 'getBlur' }
        ];
        for (var j = 0; j < expected.length; j++) {
            var found = props.filter(function(p) { return p.name === expected[j].name; });
            assert.strictEqual(found.length, 1, scriptFile + ': missing or duplicate ' + expected[j].name);
            assert.strictEqual(found[0].display, expected[j].display, scriptFile + ': display mismatch for ' + expected[j].name);
            assert.strictEqual(typeof algo[expected[j].read], 'function', scriptFile + ': missing reader ' + expected[j].read);
        }
        assert.strictEqual(algo.getFlip(), 'Off', scriptFile + ': default flip');
        assert.strictEqual(algo.getMirror(), 'Off', scriptFile + ': default mirror');
        assert.strictEqual(algo.getBackgroundMode(), 'Off', scriptFile + ': default background mode');
        assert.strictEqual(algo.getBackgroundColor(), '#000000', scriptFile + ': default background color');
        assert.strictEqual(Number(algo.getBackgroundBrightness()), 1, scriptFile + ': default background brightness');
        assert.strictEqual(Number(algo.getBrightness()), 1, scriptFile + ': default brightness');
        assert.strictEqual(Number(algo.getBlur()), 0, scriptFile + ': default blur');
    }

    function test_row12_output_x_digitalrain2d() {
        assertXMetadata('audiodigitalrain.js');

        var width = 8;
        var height = 6;
        var algo = loadScript('audiodigitalrain.js', 0x1201);
        algo.setCount(2.0);
        algo.setAddSpeed(30);
        algo.setRunSeconds(4);
        algo.setLineWidth(10);
        algo.setTail(75);
        algo.setTailSegments(8);
        algo.setMultiplier(4);
        algo.colors = [{ h: 0.00, s: 1, v: 1 }, { h: 0.33, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];

        var warm = workerBOutputAudio({
            sourceId: 'dr-x',
            dt: 0.25,
            timing: { deltaSeconds: 0.25 },
            powers: { raw: { low: 0.9, mid: 0.3, high: 0.1, bass: 0, beat: 0 } },
            tempo: { beatPhase: 0.35, valid: true }
        });
        render(algo, 'audiodigitalrain.js', width, height, warm, 'row12 x warm 1');
        render(algo, 'audiodigitalrain.js', width, height, warm, 'row12 x warm 2');

        var stable = workerBOutputAudio({
            sourceId: 'dr-x',
            dt: 0,
            timing: { deltaSeconds: 0 },
            powers: { raw: { low: 0.9, mid: 0.3, high: 0.1, bass: 0, beat: 0 } },
            tempo: { beatPhase: 0.35, valid: true }
        });
        var baseMap = render(algo, 'audiodigitalrain.js', width, height, stable, 'row12 x base');
        assert(baseMap.some(function(v, idx) { return (idx % 3) === 2 && v > 0.01; }), 'row12 x base must contain lit pixels');

        var defaultExpected = expectedWithTransforms(baseMap, width, height, {
            flip: 'Off',
            mirror: 'Off',
            backgroundMode: 'Off',
            backgroundColor: '#000000',
            backgroundBrightness: 1,
            brightness: 1,
            blur: 0
        });
        assertMapClose(baseMap, defaultExpected, 'row12 x defaults must be identity');

        algo.setFlip('On');
        algo.setMirror('On');
        algo.setBackgroundMode('Additive');
        algo.setBackgroundColor('#112233');
        algo.setBackgroundBrightness(0.6);
        algo.setBrightness(0.7);
        algo.setBlur(1.5);
        var transformed = render(algo, 'audiodigitalrain.js', width, height, stable, 'row12 x transformed');
        var expected = expectedWithTransforms(baseMap, width, height, {
            flip: 'On',
            mirror: 'On',
            backgroundMode: 'Additive',
            backgroundColor: '#112233',
            backgroundBrightness: 0.6,
            brightness: 0.7,
            blur: 1.5
        });
        assertMapClose(transformed, expected, 'row12 x transformed mismatch');
        assert(maxDelta(baseMap, transformed) > 1e-4, 'row12 x transforms should change output');

        var bgOnly = loadScript('audiodigitalrain.js', 0x1202);
        bgOnly.setBackgroundMode('Additive');
        bgOnly.setBackgroundColor('#204060');
        bgOnly.setBackgroundBrightness(1);
        bgOnly.setBrightness(0.5);
        var emptyFrame = workerBOutputAudio({
            sourceId: 'dr-bg',
            dt: 0,
            timing: { deltaSeconds: 0 },
            powers: { raw: { low: 0, mid: 0, high: 0, bass: 0, beat: 0 } }
        });
        var earlyMap = render(bgOnly, 'audiodigitalrain.js', 4, 1, emptyFrame, 'row12 x early background');
        var earlyExpected = expectedWithTransforms(new Array(12).fill(0), 4, 1, {
            flip: 'Off',
            mirror: 'Off',
            backgroundMode: 'Additive',
            backgroundColor: '#204060',
            backgroundBrightness: 1,
            brightness: 0.5,
            blur: 0
        });
        assertMapClose(earlyMap, earlyExpected, 'row12 x early background mismatch');
    }

    function test_row62_output_x_waterfall2d() {
        assertXMetadata('audiowaterfall.js');

        var width = 4;
        var height = 1;
        var algo = loadScript('audiowaterfall.js', 0x6201);
        algo.setBands(4);
        algo.setAggregation('Mean');
        algo.setCenterMode('Off');
        algo.setDropSeconds(10);
        algo.setFadeOut(0);
        algo.colors = [
            { h: 0.00, s: 1, v: 1 },
            { h: 0.16, s: 1, v: 1 },
            { h: 0.50, s: 1, v: 1 },
            { h: 0.83, s: 1, v: 1 }
        ];

        var frame = workerBOutputAudio({
            sourceId: 'wf-x',
            dt: 0,
            timing: { deltaSeconds: 0 },
            banks: { full: { count: 4, novelty: [0, 0.34, 0.67, 1], processed: [0, 0.34, 0.67, 1] } }
        });
        var baseMap = render(algo, 'audiowaterfall.js', width, height, frame, 'row62 x base');
        var defaultExpected = expectedWithTransforms(baseMap, width, height, {
            flip: 'Off',
            mirror: 'Off',
            backgroundMode: 'Off',
            backgroundColor: '#000000',
            backgroundBrightness: 1,
            brightness: 1,
            blur: 0
        });
        assertMapClose(baseMap, defaultExpected, 'row62 x defaults must be identity');
        assert(Math.abs(mapAt(baseMap, width, 0, 0).h - mapAt(baseMap, width, 3, 0).h) > 0.05,
            'row62 x base should contain distinct pixels');

        algo.setFlip('On');
        algo.setMirror('On');
        var flipMirror = render(algo, 'audiowaterfall.js', width, height, frame, 'row62 x flip+mirror');
        var manual = manualFlipMirror(baseMap, width, height);
        assertMapClose(flipMirror, manual, 'row62 x flip+mirror paired-max mismatch');

        algo.setMirror('Off');
        algo.setBlur(1.25);
        var blurred = render(algo, 'audiowaterfall.js', width, height, frame, 'row62 x blur');
        var blurExpected = expectedWithTransforms(baseMap, width, height, {
            flip: 'On',
            mirror: 'Off',
            backgroundMode: 'Off',
            backgroundColor: '#000000',
            backgroundBrightness: 1,
            brightness: 1,
            blur: 1.25
        });
        assertMapClose(blurred, blurExpected, 'row62 x blur mismatch');
        assert(maxDelta(flipMirror, blurred) > 1e-4, 'row62 x nonzero blur should change output');

        var overdriveBase = loadScript('audiowaterfall.js', 0x6202);
        overdriveBase.setBands(1);
        overdriveBase.setDropSeconds(10);
        overdriveBase.setFadeOut(0);
        overdriveBase.colors = [{ h: 1 / 12, s: 1, v: 1 }];
        var overdriveFrame = workerBOutputAudio({
            sourceId: 'wf-overdrive',
            dt: 0,
            timing: { deltaSeconds: 0 },
            banks: { full: { count: 1, novelty: [1], processed: [1] } }
        });
        var baseSingle = render(overdriveBase, 'audiowaterfall.js', 1, 1, overdriveFrame, 'row62 x overdrive base');

        var overdrive = loadScript('audiowaterfall.js', 0x6202);
        overdrive.setBands(1);
        overdrive.setDropSeconds(10);
        overdrive.setFadeOut(0);
        overdrive.colors = [{ h: 1 / 12, s: 1, v: 1 }];
        overdrive.setBackgroundMode('Additive');
        overdrive.setBackgroundColor('#ff8000');
        overdrive.setBackgroundBrightness(1);
        overdrive.setBrightness(0.25);
        var overdriveMap = render(overdrive, 'audiowaterfall.js', 1, 1, overdriveFrame, 'row62 x overdrive transformed');
        var overdriveExpected = expectedWithTransforms(baseSingle, 1, 1, {
            flip: 'Off',
            mirror: 'Off',
            backgroundMode: 'Additive',
            backgroundColor: '#ff8000',
            backgroundBrightness: 1,
            brightness: 0.25,
            blur: 0
        });
        assertMapClose(overdriveMap, overdriveExpected, 'row62 x overdrive mismatch');
        var px = mapAt(overdriveMap, 1, 0, 0);
        assert(Math.abs(px.v - 0.5) < 0.02, 'row62 x overdrive should retain additive product before brightness');
    }

    test_row12_output_x_digitalrain2d();
    markCase('row12-digitalrain2d-output-x');
    console.log('PASS row12 digitalrain2d output x');

    test_row62_output_x_waterfall2d();
    markCase('row62-waterfall2d-output-x');
    console.log('PASS row62 waterfall2d output x');
}

function assertWorkerCOutputRows() {
    const EPS = 1e-6;

    function approx(actual, expected, eps, label) {
        assert(Math.abs(actual - expected) <= eps,
            `${label}: expected ${expected}, got ${actual}`);
    }

    function assertArrayClose(actual, expected, eps, label) {
        assert.strictEqual(actual.length, expected.length, `${label}: length mismatch`);
        for (let i = 0; i < actual.length; i++)
            approx(actual[i], expected[i], eps, `${label} @${i}`);
    }

    function meanAbsDiff(a, b) {
        assert.strictEqual(a.length, b.length, 'meanAbsDiff length mismatch');
        let sum = 0;
        for (let i = 0; i < a.length; i++) sum += Math.abs(a[i] - b[i]);
        return sum / a.length;
    }

    function hasDifference(a, b, eps = 1e-9) {
        for (let i = 0; i < a.length; i++)
            if (Math.abs(a[i] - b[i]) > eps) return true;
        return false;
    }

    function makeFrame() {
        return audio({
            version: 6,
            sourceId: 'worker-c-output',
            profileId: 6,
            sourceEpoch: 1,
            configRevision: 1,
            frameSequence: 1,
            dt: 0,
            timing: { deltaSeconds: 0 },
            powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } },
            events: { delta: { kick: 0, beat: 0 } }
        });
    }

    function loadUtilOracle() {
        const utilSandbox = { Math, Float32Array };
        vm.createContext(utilSandbox);
        vm.runInContext(
            fs.readFileSync(path.join(__dirname, '..', 'resources', 'huescripts', 'hsvutil.js'), 'utf8'),
            utilSandbox,
            { filename: 'hsvutil.js' }
        );
        return utilSandbox.HSVUtil;
    }

    function toRgbPixels(map, util) {
        const n = map.length / 3;
        const pixels = new Array(n);
        for (let i = 0; i < n; i++) {
            const o = i * 3;
            pixels[i] = util.hsvToRgb(map[o], map[o + 1], map[o + 2]);
        }
        return pixels;
    }

    function fromRgbPixels(pixels, util) {
        const map = new Array(pixels.length * 3);
        for (let i = 0; i < pixels.length; i++) {
            const hsv = util.rgbToHsv(pixels[i][0], pixels[i][1], pixels[i][2]);
            const o = i * 3;
            map[o] = hsv.h;
            map[o + 1] = hsv.s;
            map[o + 2] = hsv.v;
        }
        return map;
    }

    function manualApply(map, width, height, opts, util) {
        const n = width * height;
        let pixels = toRgbPixels(map, util);
        if (opts.flip === 'On') pixels.reverse();
        if (opts.mirror === 'On') {
            const mirrored = new Array(n * 2);
            for (let i = 0; i < n; i++) {
                mirrored[i] = pixels[n - 1 - i];
                mirrored[n + i] = pixels[i];
            }
            const folded = new Array(n);
            for (let i = 0; i < n; i++) {
                const a = mirrored[i * 2];
                const b = mirrored[i * 2 + 1];
                folded[i] = [
                    Math.max(a[0], b[0]),
                    Math.max(a[1], b[1]),
                    Math.max(a[2], b[2])
                ];
            }
            pixels = folded;
        }
        if (opts.backgroundMode === 'Additive') {
            const bg = util.parseHexRgb(opts.backgroundColor || '#000000');
            const bgScale = Math.max(0, Math.min(1, Number(opts.backgroundBrightness)));
            for (let i = 0; i < n; i++) {
                pixels[i][0] += bg[0] * bgScale;
                pixels[i][1] += bg[1] * bgScale;
                pixels[i][2] += bg[2] * bgScale;
            }
        }
        const brightness = Math.max(0, Math.min(1, Number(opts.brightness)));
        for (let i = 0; i < n; i++) {
            pixels[i][0] *= brightness;
            pixels[i][1] *= brightness;
            pixels[i][2] *= brightness;
        }
        const sigma = Math.max(0, Math.min(10, Number(opts.blur)));
        if (sigma > 0 && n > 3) {
            const radius = Math.max(1, Math.min(Math.floor((n - 1) / 2), Math.round(4 * sigma)));
            const kernel = new Array(radius * 2 + 1);
            let sum = 0;
            for (let d = -radius; d <= radius; d++) {
                const w = Math.exp(-d * d / (2 * sigma * sigma));
                kernel[d + radius] = w;
                sum += w;
            }
            for (let k = 0; k < kernel.length; k++) kernel[k] /= sum;
            const blurred = new Array(n);
            for (let i = 0; i < n; i++) {
                const rgb = [0, 0, 0];
                for (let d = -radius; d <= radius; d++) {
                    const j = i + d;
                    if (j < 0 || j >= n) continue;
                    const w = kernel[d + radius];
                    rgb[0] += pixels[j][0] * w;
                    rgb[1] += pixels[j][1] * w;
                    rgb[2] += pixels[j][2] * w;
                }
                blurred[i] = rgb;
            }
            pixels = blurred;
        }
        return fromRgbPixels(pixels, util);
    }

    function manualWrongBrightnessBeforeBackground(map, width, height, opts, util) {
        const n = width * height;
        const pixels = toRgbPixels(map, util);
        const brightness = Math.max(0, Math.min(1, Number(opts.brightness)));
        for (let i = 0; i < n; i++) {
            pixels[i][0] *= brightness;
            pixels[i][1] *= brightness;
            pixels[i][2] *= brightness;
        }
        const bg = util.parseHexRgb(opts.backgroundColor || '#000000');
        const bgScale = Math.max(0, Math.min(1, Number(opts.backgroundBrightness)));
        for (let i = 0; i < n; i++) {
            pixels[i][0] += bg[0] * bgScale;
            pixels[i][1] += bg[1] * bgScale;
            pixels[i][2] += bg[2] * bgScale;
        }
        return fromRgbPixels(pixels, util);
    }

    function scriptControlDescriptors(scriptFile) {
        return loadScript(scriptFile).properties.map(parseProperty);
    }

    function assertOutputMetadataAndDefaults(scriptFile) {
        const descriptors = scriptControlDescriptors(scriptFile);
        const names = descriptors.map(p => p.name);
        const controls = [
            ['flip', 'Flip'],
            ['mirror', 'Mirror'],
            ['backgroundMode', 'Background Mode'],
            ['backgroundColor', 'Background Color'],
            ['backgroundBrightness', 'Background Brightness'],
            ['brightness', 'Brightness'],
            ['blur', 'Blur']
        ];
        for (const [name, display] of controls) {
            const entries = descriptors.filter(p => p.name === name);
            assert.strictEqual(entries.length, 1, `${scriptFile}: expected single ${name} descriptor`);
            assert.strictEqual(entries[0].display, display, `${scriptFile}: ${name} display mismatch`);
        }
        assert.strictEqual(new Set(names).size, names.length, `${scriptFile}: duplicate descriptor names`);

        const algo = loadScript(scriptFile);
        assert.strictEqual(algo.getFlip(), 'Off', `${scriptFile}: default flip`);
        assert.strictEqual(algo.getMirror(), 'Off', `${scriptFile}: default mirror`);
        assert.strictEqual(algo.getBackgroundMode(), 'Off', `${scriptFile}: default backgroundMode`);
        assert.strictEqual(algo.getBackgroundColor(), '#000000', `${scriptFile}: default backgroundColor`);
        approx(Number(algo.getBackgroundBrightness()), 1, EPS, `${scriptFile}: default backgroundBrightness`);
        approx(Number(algo.getBrightness()), 1, EPS, `${scriptFile}: default brightness`);
        approx(Number(algo.getBlur()), 0, EPS, `${scriptFile}: default blur`);

        algo.setFlip('Yes');
        assert.strictEqual(algo.getFlip(), 'On', `${scriptFile}: Yes should normalize to On`);
        algo.setFlip('No');
        assert.strictEqual(algo.getFlip(), 'Off', `${scriptFile}: No should normalize to Off`);
    }

    function configureIdentity(algo) {
        algo.setFlip('Off');
        algo.setMirror('Off');
        algo.setBackgroundMode('Off');
        algo.setBackgroundColor('#000000');
        algo.setBackgroundBrightness(1);
        algo.setBrightness(1);
        algo.setBlur(0);
    }

    function caseMetadataAndIdentity(util) {
        assertOutputMetadataAndDefaults('audionoise.js');
        assertOutputMetadataAndDefaults('audiosmoke.js');

        for (const scriptFile of ['audionoise.js', 'audiosmoke.js']) {
            const algo = loadScript(scriptFile, 0xc0ffee);
            algo.setSpeed(0);
            algo.setMultiplier(0);
            const frame = makeFrame();
            const base = render(algo, scriptFile, 8, 1, frame, `${scriptFile} base`);
            configureIdentity(algo);
            const identity = render(algo, scriptFile, 8, 1, frame, `${scriptFile} identity`);
            assert.deepStrictEqual(identity, base, `${scriptFile}: identity controls changed output`);

            const expected = util.applyStripTransforms(
                Float32Array.from(base),
                8,
                1,
                {
                    flip: 'Off',
                    mirror: 'Off',
                    backgroundMode: 'Off',
                    backgroundColor: '#000000',
                    backgroundBrightness: 1,
                    brightness: 1,
                    blur: 0
                }
            );
            assertArrayClose(identity, Array.from(expected), EPS, `${scriptFile}: identity oracle mismatch`);
        }
    }

    function caseFlipMirror(util) {
        const scriptFile = 'audionoise.js';
        const algo = loadScript(scriptFile, 0xc36055);
        algo.setSpeed(0);
        algo.setMultiplier(0);
        configureIdentity(algo);
        const frame = makeFrame();
        const width = 7;
        const base = render(algo, scriptFile, width, 1, frame, 'noise base');

        algo.setFlip('On');
        algo.setMirror('On');
        const actual = render(algo, scriptFile, width, 1, frame, 'noise flip mirror');
        const opts = {
            flip: 'On',
            mirror: 'On',
            backgroundMode: 'Off',
            backgroundColor: '#000000',
            backgroundBrightness: 1,
            brightness: 1,
            blur: 0
        };
        const expected = manualApply(base, width, 1, opts, util);
        assertArrayClose(actual, expected, EPS, 'C36 output flip+mirror oracle');
        assert(hasDifference(actual, base, 1e-5),
            'C36 output flip+mirror: transformed map should differ from base');
    }

    function caseAdditiveBeforeBrightness(util) {
        const scriptFile = 'audiosmoke.js';
        const algo = loadScript(scriptFile, 0xc54055);
        algo.setSpeed(0);
        algo.setMultiplier(0);
        configureIdentity(algo);
        const frame = makeFrame();
        const width = 6;
        const base = render(algo, scriptFile, width, 1, frame, 'smoke base');

        algo.setBackgroundMode('Additive');
        algo.setBackgroundColor('#ff0000');
        algo.setBackgroundBrightness(0.8);
        algo.setBrightness(0.5);
        const actual = render(algo, scriptFile, width, 1, frame, 'smoke additive+brightness');
        const opts = {
            flip: 'Off',
            mirror: 'Off',
            backgroundMode: 'Additive',
            backgroundColor: '#ff0000',
            backgroundBrightness: 0.8,
            brightness: 0.5,
            blur: 0
        };
        const expected = manualApply(base, width, 1, opts, util);
        const wrongOrder = manualWrongBrightnessBeforeBackground(base, width, 1, opts, util);
        assertArrayClose(actual, expected, EPS, 'C54 output additive-before-brightness oracle');
        assert(meanAbsDiff(actual, wrongOrder) > 1e-3,
            'C54 output additive-before-brightness: output matched wrong order');
    }

    function caseBlur(util) {
        const scriptFile = 'audionoise.js';
        const algo = loadScript(scriptFile, 0xc36056);
        algo.setSpeed(0);
        algo.setMultiplier(0);
        configureIdentity(algo);
        const frame = makeFrame();
        const width = 9;
        const base = render(algo, scriptFile, width, 1, frame, 'noise blur base');

        algo.setBlur(1.25);
        const actual = render(algo, scriptFile, width, 1, frame, 'noise blur 1.25');
        const opts = {
            flip: 'Off',
            mirror: 'Off',
            backgroundMode: 'Off',
            backgroundColor: '#000000',
            backgroundBrightness: 1,
            brightness: 1,
            blur: 1.25
        };
        const expected = manualApply(base, width, 1, opts, util);
        assertArrayClose(actual, expected, EPS, 'C36 output blur oracle');
        assert(hasDifference(actual, base, 1e-5), 'C36 output blur: blur did not alter map');
    }

    function caseOverdrivePreserved(util) {
        const scriptFile = 'audionoise.js';
        const algo = loadScript(scriptFile, 0xc36057);
        algo.setSpeed(0);
        algo.setMultiplier(0);
        algo.colors = [{ h: 0, s: 0, v: 1 }];
        configureIdentity(algo);
        algo.setBackgroundMode('Additive');
        algo.setBackgroundColor('#ff0000');
        algo.setBackgroundBrightness(1);
        algo.setBrightness(0.5);
        const frame = makeFrame();
        const out = render(algo, scriptFile, 1, 1, frame, 'noise overdrive');
        const rgb = util.hsvToRgb(out[0], out[1], out[2]);
        assertArrayClose(rgb, [1, 0.5, 0.5], EPS, 'C36 output overdrive-preserved rgb');
    }

    function caseAudioIdentityKeyRetention() {
        function frame(overrides) {
            return audio(Object.assign({
                version: 6,
                sourceId: 'stable-source',
                profileId: 11,
                sourceEpoch: 4,
                configRevision: 3,
                frameSequence: 1,
                available: true,
                status: 'ok',
                dt: 0.2,
                timing: { deltaSeconds: 0.2 },
                powers: null,
                events: { delta: { kick: 0, beat: 0 } }
            }, overrides || {}));
        }

        for (const [scriptFile, seed] of [['audionoise.js', 0xc36058], ['audiosmoke.js', 0xc54058]]) {
            const retained = loadScript(scriptFile, seed);
            retained.setSpeed(1.1);
            retained.setMultiplier(0);
            configureIdentity(retained);
            render(retained, scriptFile, 8, 3, frame({ frameSequence: 1 }), `${scriptFile} retained pre`);
            const kept = render(retained, scriptFile, 8, 3, frame({
                frameSequence: 2,
                sourceId: '',
                available: false,
                status: 'reset'
            }), `${scriptFile} retained unavailable`);

            const control = loadScript(scriptFile, seed);
            control.setSpeed(1.1);
            control.setMultiplier(0);
            configureIdentity(control);
            render(control, scriptFile, 8, 3, frame({ frameSequence: 1 }), `${scriptFile} control pre`);
            const continued = render(control, scriptFile, 8, 3, frame({
                frameSequence: 2,
                sourceId: 'stable-source',
                available: true,
                status: 'ok'
            }), `${scriptFile} control continued`);

            assertArrayClose(kept, continued, EPS,
                `${scriptFile}: unavailable same-profile frame should retain prior audio identity`);

            const changed = loadScript(scriptFile, seed);
            changed.setSpeed(1.1);
            changed.setMultiplier(0);
            configureIdentity(changed);
            render(changed, scriptFile, 8, 3, frame({ frameSequence: 1 }), `${scriptFile} changed pre`);
            const resetOnEpoch = render(changed, scriptFile, 8, 3, frame({
                frameSequence: 2,
                sourceEpoch: 5
            }), `${scriptFile} changed epoch`);
            assert(hasDifference(resetOnEpoch, continued, 1e-6),
                `${scriptFile}: new epoch should change audio identity and reset state`);
        }
    }

    const util = loadUtilOracle();
    const cases = [
        ['C36+C54/output/x-metadata-defaults-identity', () => caseMetadataAndIdentity(util)],
        ['C36+C54/identity/audio-key-retain-unavailable-reset-on-epoch', () => caseAudioIdentityKeyRetention()],
        ['C36/output/flip-then-paired-max-mirror', () => caseFlipMirror(util)],
        ['C54/output/additive-before-brightness', () => caseAdditiveBeforeBrightness(util)],
        ['C36/output/nonzero-blur-gaussian', () => caseBlur(util)],
        ['C36/output/overdrive-preserved-under-brightness', () => caseOverdrivePreserved(util)]
    ];

    for (const [id, fn] of cases) {
        fn();
        markCase(id);
        console.log('PASS ' + id);
    }
}

function assertWorkerDOutputRows() {
    const approxDOut = (a, b, eps) => Math.abs(a - b) <= eps;

    function hsvToRgbDOut(h, s, v) {
        h = ((h % 1) + 1) % 1;
        s = Math.max(0, Math.min(1, s));
        v = Math.max(0, Math.min(1, v));
        const i = Math.floor(h * 6);
        const f = h * 6 - i;
        const p = v * (1 - s);
        const q = v * (1 - f * s);
        const t = v * (1 - (1 - f) * s);
        switch (i % 6) {
        case 0: return [v, t, p];
        case 1: return [q, v, p];
        case 2: return [p, v, t];
        case 3: return [p, q, v];
        case 4: return [t, p, v];
        default: return [v, p, q];
        }
    }

    function makeFrameDOut(sourceId, profileId, overrides) {
        return audio(Object.assign({
            version: 6,
            dt: 0,
            timing: { deltaSeconds: 0 },
            sourceId: sourceId,
            profileId: profileId,
            sourceEpoch: 1,
            configRevision: 1,
            pitch: { valid: false, hz: 0, midi: 0, confidence: 0 },
            banks: { full: { count: 6, processed: [0, 0, 0, 0, 0, 0], novelty: [0, 0, 0, 0, 0, 0] } },
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
            powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } }
        }, overrides || {}));
    }

    function rgbAtDOut(map, x, y, width) {
        const i = (y * width + x) * 3;
        return hsvToRgbDOut(map[i], map[i + 1], map[i + 2]);
    }

    function assertRgbDOut(actual, expected, msg, eps) {
        assert(
            approxDOut(actual[0], expected[0], eps) &&
            approxDOut(actual[1], expected[1], eps) &&
            approxDOut(actual[2], expected[2], eps),
            `${msg}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`
        );
    }

    function runCaseDOut(caseId, fn) {
        try {
            fn();
            markCase(caseId);
            console.log(`PASS ${caseId}`);
            return { caseId: caseId, ok: true };
        } catch (err) {
            console.log(`FAIL ${caseId} :: ${err.message}`);
            return { caseId: caseId, ok: false, error: err.message };
        }
    }

    function caseMetadataUnique() {
        const scripts = [
            ['audiofilter.js', 0x4d01],
            ['audiomagnitude.js', 0x4d02],
            ['audiovumeter.js', 0x4d03],
            ['audiopitchspectrum.js', 0x4d04]
        ];
        const required = [
            'flip',
            'mirror',
            'backgroundMode',
            'backgroundColor',
            'backgroundBrightness',
            'brightness',
            'blur'
        ];
        for (const [file, seed] of scripts) {
            const algo = loadScript(file, seed);
            const names = algo.properties.map(prop => prop.split('|')[0].slice(5));
            const counts = {};
            for (const name of names) counts[name] = (counts[name] || 0) + 1;
            for (const name of required) {
                assert.strictEqual(counts[name], 1, `${file} expected one "${name}" property, got ${counts[name] || 0}`);
            }
        }
    }

    function caseMagnitudeFlipMirror() {
        const algo = loadScript('audiomagnitude.js', 0x4d11);
        algo.setFrequencyRange('Beat');
        algo.setFlip('Off');
        algo.setMirror('Off');
        algo.setBackgroundMode('Off');
        algo.setBackgroundColor('#000000');
        algo.setBackgroundBrightness(1);
        algo.setBrightness(1);
        algo.setBlur(0);
        algo.colors = [
            { h: 0 / 3, s: 1, v: 1 },
            { h: 1 / 3, s: 1, v: 1 },
            { h: 2 / 3, s: 1, v: 1 }
        ];
        const frame = makeFrameDOut('mag-x', 30, { beat: 1 });
        const base = render(algo, 'audiomagnitude.js', 3, 1, frame, 'd-output base');
        assertRgbDOut(rgbAtDOut(base, 0, 0, 3), [1, 0, 0], 'magnitude base x0', 1e-6);
        assertRgbDOut(rgbAtDOut(base, 1, 0, 3), [0, 1, 0], 'magnitude base x1', 1e-6);
        assertRgbDOut(rgbAtDOut(base, 2, 0, 3), [0, 0, 1], 'magnitude base x2', 1e-6);

        algo.setFlip('On');
        const flipped = render(algo, 'audiomagnitude.js', 3, 1, frame, 'd-output flipped');
        assertRgbDOut(rgbAtDOut(flipped, 0, 0, 3), [0, 0, 1], 'magnitude flip x0', 1e-6);
        assertRgbDOut(rgbAtDOut(flipped, 1, 0, 3), [0, 1, 0], 'magnitude flip x1', 1e-6);
        assertRgbDOut(rgbAtDOut(flipped, 2, 0, 3), [1, 0, 0], 'magnitude flip x2', 1e-6);

        algo.setMirror('On');
        const mirrored = render(algo, 'audiomagnitude.js', 3, 1, frame, 'd-output flipped+mirrored');
        assertRgbDOut(rgbAtDOut(mirrored, 0, 0, 3), [1, 1, 0], 'magnitude mirror x0', 1e-6);
        assertRgbDOut(rgbAtDOut(mirrored, 1, 0, 3), [0, 0, 1], 'magnitude mirror x1', 1e-6);
        assertRgbDOut(rgbAtDOut(mirrored, 2, 0, 3), [1, 1, 0], 'magnitude mirror x2', 1e-6);
    }

    function caseFilterAdditiveBeforeBrightness() {
        const algo = loadScript('audiofilter.js', 0x4d21);
        algo.setUseGradient('No');
        algo.setColor('#ff0000');
        algo.setFrequencyRange('Beat');
        algo.setBoost(0);
        algo.setRollSpeed(0);
        algo.setFlip('Off');
        algo.setMirror('Off');
        algo.setBlur(0);
        algo.setBackgroundMode('Additive');
        algo.setBackgroundColor('#0000ff');
        algo.setBackgroundBrightness(0.5);
        algo.setBrightness(0.5);
        const map = render(
            algo,
            'audiofilter.js',
            4,
            1,
            makeFrameDOut('filter-x', 18, { beat: 1 }),
            'd-output filter additive-before-brightness'
        );
        for (let x = 0; x < 4; x++) {
            assertRgbDOut(rgbAtDOut(map, x, 0, 4), [0.5, 0, 0.25], `filter additive order x${x}`, 1e-6);
        }
    }

    function casePitchBlurNonzero() {
        const algo = loadScript('audiopitchspectrum.js', 0x4d31);
        algo.setFadeRate(0);
        algo.setResponsiveness(1);
        algo.setMirror('No');
        algo.setFlip('Off');
        algo.setBackgroundMode('Off');
        algo.setBrightness(1);
        algo.setBlur(0);
        algo.colors = [
            { h: 0.00, s: 1, v: 1 },
            { h: 0.66, s: 1, v: 1 }
        ];
        const frame = makeFrameDOut('pitch-x', 38, {
            dt: 0.04,
            timing: { deltaSeconds: 0.04 },
            pitch: { valid: true, hz: 440, midi: 69, confidence: 0.9 },
            banks: { full: { count: 5, processed: [1, 0, 0, 0, 0], novelty: [0, 0, 0, 0, 0] } }
        });
        const sharp = render(algo, 'audiopitchspectrum.js', 5, 1, frame, 'd-output pitch sharp');
        const sharpMid = rgbAtDOut(sharp, 1, 0, 5);
        assertRgbDOut(sharpMid, [0, 0, 0], 'pitch sharp middle', 1e-6);

        algo.setBlur(1);
        const blurred = render(algo, 'audiopitchspectrum.js', 5, 1, frame, 'd-output pitch blurred');
        const blurMid = rgbAtDOut(blurred, 1, 0, 5);
        assert(blurMid[0] > 0 || blurMid[1] > 0 || blurMid[2] > 0, `pitch blur should spread energy to middle, got ${JSON.stringify(blurMid)}`);
    }

    function caseIdentityDefaults() {
        function configureIdentity(algo) {
            algo.setFlip('Off');
            algo.setMirror('Off');
            algo.setBackgroundMode('Off');
            algo.setBackgroundColor('#000000');
            algo.setBackgroundBrightness(1);
            algo.setBrightness(1);
            algo.setBlur(0);
        }

        const a = loadScript('audiovumeter.js', 0x4d41);
        const b = loadScript('audiovumeter.js', 0x4d42);
        configureIdentity(b);
        const frame = makeFrameDOut('vu-x', 60, {
            dt: 0.04,
            timing: { deltaSeconds: 0.04 },
            volume: { rawRms: Math.pow(10, ((0.7 - 1) * 100) / 20), normalized: 0.01 }
        });
        const mapA = render(a, 'audiovumeter.js', 25, 1, frame, 'd-output vu default');
        const mapB = render(b, 'audiovumeter.js', 25, 1, frame, 'd-output vu explicit-identity');
        assert.deepStrictEqual(mapA, mapB, 'identity defaults must preserve valid HSV map');
    }

    function caseOverdriveBrightness() {
        const algo = loadScript('audiofilter.js', 0x4d51);
        algo.setUseGradient('No');
        algo.setColor('#ff0000');
        algo.setFrequencyRange('Beat');
        algo.setBoost(0);
        algo.setRollSpeed(0);
        algo.setFlip('Off');
        algo.setMirror('Off');
        algo.setBackgroundMode('Additive');
        algo.setBackgroundColor('#ff0000');
        algo.setBackgroundBrightness(1);
        algo.setBlur(0);
        algo.setBrightness(0.5);
        const map = render(
            algo,
            'audiofilter.js',
            1,
            1,
            makeFrameDOut('filter-overdrive', 18, { beat: 1 }),
            'd-output overdrive-before-final-clip'
        );
        assertRgbDOut(rgbAtDOut(map, 0, 0, 1), [1, 0, 0], 'overdrive should survive until brightness stage', 1e-6);
    }

    const results = [
        runCaseDOut('D-output-metadata-unique', caseMetadataUnique),
        runCaseDOut('D-output-magnitude-flip-mirror', caseMagnitudeFlipMirror),
        runCaseDOut('D-output-filter-additive-before-brightness', caseFilterAdditiveBeforeBrightness),
        runCaseDOut('D-output-pitch-blur-nonzero', casePitchBlurNonzero),
        runCaseDOut('D-output-identity-defaults', caseIdentityDefaults),
        runCaseDOut('D-output-overdrive-brightness', caseOverdriveBrightness)
    ];
    const failed = results.filter(r => !r.ok);
    if (failed.length)
        throw new Error(`Worker D output cases failed: ${failed.map(r => r.caseId).join(', ')}`);
    console.log(`Worker D output cases passed (${results.length}/${results.length})`);
}

function assertWorkerIRegressions20260916() {
    function frame(overrides) {
        return audio(Object.assign({
            version: 6,
            bpm: 120,
            dt: 0.2,
            timing: { deltaSeconds: 0.1 },
            sourceId: "worker-i-regressions",
            profileId: 191,
            sourceEpoch: 1,
            configRevision: 1,
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
            banks: { full: { count: 24, processed: new Array(24).fill(0), novelty: new Array(24).fill(0) } }
        }, overrides || {}));
    }

    function constantRandom(value) {
        return function() { return value; };
    }

    function assertMapClose(a, b, epsilon, message) {
        assert.strictEqual(a.length, b.length, message + " (length)");
        for (var i = 0; i < a.length; i++) {
            assert(Math.abs(a[i] - b[i]) <= epsilon, message + " at index " + i);
        }
    }

    {
        var decay2x = loadScript("audiopower.js", 0x4201, SCRIPTS_DIR, { random: constantRandom(0.25) });
        var decay1x = loadScript("audiopower.js", 0x4201, SCRIPTS_DIR, { random: constantRandom(0.25) });
        decay2x.setMode("LedFx Power");
        decay1x.setMode("LedFx Power");
        decay2x.setFrequencyRange("Lows (beat+bass)");
        decay1x.setFrequencyRange("Lows (beat+bass)");
        decay2x.setSparksDecayRate(0.6);
        decay1x.setSparksDecayRate(0.6);
        decay2x.setBassDecayRate(0.5);
        decay1x.setBassDecayRate(0.5);

        var seed = frame({
            low: 1,
            timing: { deltaSeconds: 0.02 },
            dt: 0.04,
            events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } }
        });
        render(decay2x, "audiopower.js", 24, 1, seed, "workerI regression seed decay2x");
        render(decay1x, "audiopower.js", 24, 1, seed, "workerI regression seed decay1x");

        var decayStep = frame({
            low: 0,
            timing: { deltaSeconds: 0.02 },
            dt: 0.04,
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
        });
        render(decay2x, "audiopower.js", 24, 1, decayStep, "workerI regression decay step A");
        var out2x = render(decay2x, "audiopower.js", 24, 1, decayStep, "workerI regression decay step B");
        var out1x = render(decay1x, "audiopower.js", 24, 1, frame({
            low: 0,
            timing: { deltaSeconds: 0.04 },
            dt: 0.08,
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
        }), "workerI regression decay single");

        assertMapClose(out2x, out1x, 1e-6, "regression row42: 2x20ms decay should match 1x40ms");
        markCase("workerI.row42-regression-decay-2x20ms-vs-1x40ms");
    }

    {
        var filter2x = loadScript("audiopower.js", 0x4202, SCRIPTS_DIR, { random: constantRandom(0.13) });
        var filter1x = loadScript("audiopower.js", 0x4202, SCRIPTS_DIR, { random: constantRandom(0.13) });
        filter2x.setMode("LedFx Power");
        filter1x.setMode("LedFx Power");
        filter2x.setFrequencyRange("Lows (beat+bass)");
        filter1x.setFrequencyRange("Lows (beat+bass)");
        filter2x.setSparksDecayRate(0);
        filter1x.setSparksDecayRate(0);
        filter2x.setBassDecayRate(0);
        filter1x.setBassDecayRate(0);

        var step20 = frame({
            low: 0.8,
            timing: { deltaSeconds: 0.02 },
            dt: 0.04,
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
        });
        render(filter2x, "audiopower.js", 24, 1, step20, "workerI regression filter step A");
        var filterOut2x = render(filter2x, "audiopower.js", 24, 1, step20, "workerI regression filter step B");
        var filterOut1x = render(filter1x, "audiopower.js", 24, 1, frame({
            low: 0.8,
            timing: { deltaSeconds: 0.04 },
            dt: 0.08,
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
        }), "workerI regression filter single");

        assertMapClose(filterOut2x, filterOut1x, 1e-6, "regression row42: 2x20ms filter should match 1x40ms");
        markCase("workerI.row42-regression-filter-2x20ms-vs-1x40ms");
    }

    {
        var zero = loadScript("audiopower.js", 0x4203, SCRIPTS_DIR, { random: constantRandom(0.77) });
        zero.setMode("LedFx Power");
        zero.setFrequencyRange("Lows (beat+bass)");
        render(zero, "audiopower.js", 24, 1, frame({
            low: 1,
            timing: { deltaSeconds: 0.02 },
            dt: 0.04,
            events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } }
        }), "workerI regression zero seed");
        var zeroA = render(zero, "audiopower.js", 24, 1, frame({
            low: 0,
            timing: { deltaSeconds: 0 },
            dt: 0,
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
        }), "workerI regression zero A");
        var zeroB = render(zero, "audiopower.js", 24, 1, frame({
            low: 0,
            timing: { deltaSeconds: 0 },
            dt: 0,
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
        }), "workerI regression zero B");
        assert.deepStrictEqual(zeroA, zeroB, "regression row42: zero-time duplicates must be stable without new events");
        markCase("workerI.row42-regression-zero-time-duplicate");
    }

    {
        var rain = loadScript("audiopuddles.js", 0x4401);
        rain.setMode("Rain Pulse");
        rain.setRainLowsSensitivity(0.03);
        rain.setRainMidsSensitivity(0.12);
        rain.setRainHighsSensitivity(0.27);
        assert(Math.abs(rain.getRainLowsSensitivity() - 0.03) < 1e-9, "regression row44: lows sensitivity setter mismatch");
        assert(Math.abs(rain.getRainMidsSensitivity() - 0.12) < 1e-9, "regression row44: mids sensitivity setter mismatch");
        assert(Math.abs(rain.getRainHighsSensitivity() - 0.27) < 1e-9, "regression row44: highs sensitivity setter mismatch");
        markCase("workerI.row44-regression-independent-band-threshold-controls");
    }
}

function assertWorkerAOutputRows() {
    const EPS = 1e-6;
    const SCRIPTS = [
        "audiobands.js",
        "audiobandsmatrix.js",
        "audiospectralblocks.js",
        "audiobladepower.js"
    ];
    const REQUIRED = [
        "flip",
        "mirror",
        "backgroundMode",
        "backgroundColor",
        "backgroundBrightness",
        "brightness",
        "blur"
    ];

    function approx(actual, expected, label) {
        assert(Math.abs(actual - expected) <= EPS, `${label}: expected ${expected}, got ${actual}`);
    }

    function hsvAt(map, pixel) {
        const o = pixel * 3;
        return { h: map[o], s: map[o + 1], v: map[o + 2] };
    }

    function hsvToRgb(h, s, v) {
        const hue = ((h % 1) + 1) % 1;
        const sat = Math.max(0, Math.min(1, s));
        const val = Math.max(0, v);
        const i = Math.floor(hue * 6);
        const f = hue * 6 - i;
        const p = val * (1 - sat);
        const q = val * (1 - f * sat);
        const t = val * (1 - (1 - f) * sat);
        switch (i % 6) {
            case 0: return [val, t, p];
            case 1: return [q, val, p];
            case 2: return [p, val, t];
            case 3: return [p, q, val];
            case 4: return [t, p, val];
            default: return [val, p, q];
        }
    }

    function rgbAt(map, pixel) {
        const hsv = hsvAt(map, pixel);
        return hsvToRgb(hsv.h, hsv.s, hsv.v);
    }

    function values(map, length) {
        const out = [];
        for (let i = 0; i < length; i++)
            out.push(map[i * 3 + 2]);
        return out;
    }

    function parseName(descriptor) {
        const part = descriptor.split("|").find(field => field.indexOf("name:") === 0);
        return part ? part.slice(5) : "";
    }

    function maxValue(map) {
        let peak = 0;
        for (let i = 2; i < map.length; i += 3)
            peak = Math.max(peak, map[i]);
        return peak;
    }

    function assertAllBounded(map, label) {
        for (let i = 0; i < map.length; i++) {
            assert(Number.isFinite(map[i]), `${label}: non-finite at ${i}`);
            assert(map[i] >= 0 && map[i] <= 1, `${label}: outside [0,1] at ${i} => ${map[i]}`);
        }
    }

    {
        for (const file of SCRIPTS) {
            const algo = loadScript(file);
            const names = algo.properties.map(parseName).filter(Boolean);
            const unique = new Set(names);
            assert.strictEqual(unique.size, names.length, `${file}: duplicate property names`);
            for (const control of REQUIRED)
                assert(unique.has(control), `${file}: missing X control '${control}'`);
        }
        markCase("worker-a.output-metadata");
        console.log("PASS worker-a.output-metadata");
    }

    {
        const algo = loadScript("audiospectralblocks.js");
        algo.colors = [{ h: 0, s: 0, v: 1 }];
        algo.color = algo.colors[0];
        algo.setBlockCount(1);
        algo.setFlip("On");
        algo.setMirror("On");
        algo.setBackgroundMode("Off");
        algo.setBackgroundBrightness(1);
        algo.setBrightness(1);
        algo.setBlur(0);
        const map = render(
            algo,
            "audiospectralblocks.js",
            4,
            1,
            audio({
                banks: { full: { count: 4, novelty: [1, 0.8, 0.2, 0.4], processed: [0, 0, 0, 0] } }
            }),
            "worker-a flip-then-mirror"
        );
        const expected = [1, 0.4, 0.4, 1];
        const actual = values(map, 4);
        for (let i = 0; i < expected.length; i++)
            approx(actual[i], expected[i], `flip/mirror index ${i}`);
        markCase("worker-a.output-flip-mirror-order");
        console.log("PASS worker-a.output-flip-mirror-order");
    }

    {
        const algo = loadScript("audiospectralblocks.js");
        algo.colors = [{ h: 0, s: 0, v: 1 }];
        algo.color = algo.colors[0];
        algo.setBlockCount(1);
        algo.setFlip("Off");
        algo.setMirror("Off");
        algo.setBackgroundMode("Additive");
        algo.setBackgroundColor("#ff0000");
        algo.setBackgroundBrightness(1);
        algo.setBrightness(0.5);
        algo.setBlur(0);
        const map = render(
            algo,
            "audiospectralblocks.js",
            1,
            1,
            audio({
                banks: { full: { count: 1, novelty: [0], processed: [0] } }
            }),
            "worker-a additive-before-brightness"
        );
        const pixel = hsvAt(map, 0);
        approx(pixel.h, 0, "additive hue");
        approx(pixel.s, 1, "additive saturation");
        approx(pixel.v, 0.5, "additive then brightness");
        markCase("worker-a.output-additive-before-brightness");
        console.log("PASS worker-a.output-additive-before-brightness");
    }

    {
        const algo = loadScript("audiospectralblocks.js");
        algo.colors = [{ h: 1 / 12, s: 1, v: 1 }, { h: 1 / 12, s: 1, v: 1 }];
        algo.color = algo.colors[0];
        algo.setBlockCount(1);
        algo.setFlip("Off");
        algo.setMirror("Off");
        algo.setBackgroundMode("Off");
        algo.setBackgroundBrightness(1);
        algo.setBlur(0);
        algo.setBrightness(1);
        const full = render(
            algo,
            "audiospectralblocks.js",
            1,
            1,
            audio({
                banks: {
                    full: {
                        count: 1,
                        novelty: [Math.sqrt(2)],
                        processed: [0]
                    }
                }
            }),
            "worker-a overdrive brightness full"
        );
        const fullRgb = rgbAt(full, 0);
        approx(fullRgb[0], 1, "overdrive full rgb.r");
        approx(fullRgb[1], 1, "overdrive full rgb.g");
        approx(fullRgb[2], 0, "overdrive full rgb.b");
        assertAllBounded(full, "overdrive full bounded");

        algo.setBrightness(0.5);
        const half = render(
            algo,
            "audiospectralblocks.js",
            1,
            1,
            audio({
                banks: {
                    full: {
                        count: 1,
                        novelty: [Math.sqrt(2)],
                        processed: [0]
                    }
                }
            }),
            "worker-a overdrive brightness half"
        );
        const halfRgb = rgbAt(half, 0);
        approx(halfRgb[0], 1, "overdrive half rgb.r");
        approx(halfRgb[1], 0.5, "overdrive half rgb.g");
        approx(halfRgb[2], 0, "overdrive half rgb.b");
        assertAllBounded(half, "overdrive half bounded");
        markCase("worker-a.output-overdrive");
        console.log("PASS worker-a.output-overdrive");
    }

    {
        const algo = loadScript("audiobands.js");
        algo.setBandCount(4);
        algo.setAlign("left");
        algo.setFlip("Off");
        algo.setMirror("Off");
        algo.setBackgroundMode("Off");
        algo.setBackgroundBrightness(1);
        algo.setBrightness(1);
        algo.setBlur(0);
        const base = render(
            algo,
            "audiobands.js",
            4,
            1,
            audio({
                banks: { full: { count: 4, novelty: [1, 0, 0, 0], processed: [0, 0, 0, 0] } }
            }),
            "worker-a blur base"
        );
        algo.setBlur(1.25);
        const blurred = render(
            algo,
            "audiobands.js",
            4,
            1,
            audio({
                banks: { full: { count: 4, novelty: [1, 0, 0, 0], processed: [0, 0, 0, 0] } }
            }),
            "worker-a blur active"
        );
        assert(blurred[5] > base[5], "blur should spread energy to second pixel");
        assert(blurred[2] < base[2], "blur should reduce first-pixel peak");
        markCase("worker-a.output-blur");
        console.log("PASS worker-a.output-blur");
    }

    {
        const activeFrame = audio({
            beat: 1,
            bass: 1,
            low: 1,
            mid: 1,
            high: 1,
            timing: { deltaSeconds: 1 / 60 },
            dt: 0.04,
            banks: { full: { count: 8, novelty: new Array(8).fill(1), processed: new Array(8).fill(0) } }
        });
        for (const file of SCRIPTS) {
            const algo = loadScript(file);
            if (file === "audiobands.js") {
                algo.setBandCount(4);
                algo.setAlign("left");
            } else if (file === "audiobandsmatrix.js") {
                algo.setBandCount(4);
                algo.setFlipGradient("No");
                algo.setFlipBandOrder("No");
            } else if (file === "audiospectralblocks.js") {
                algo.setBlockCount(4);
            } else if (file === "audiobladepower.js") {
                algo.setMultiplier(0.5);
                algo.setDecay(0.7);
                algo.setFrequencyRange("Lows (beat+bass)");
                algo.setFixHues("No");
                algo.setMirror("Off");
                algo.setBlur(0);
            }
            algo.setFlip("Off");
            algo.setMirror("Off");
            algo.setBackgroundMode("Off");
            algo.setBackgroundBrightness(1);
            algo.setBlur(0);
            algo.setBrightness(1);
            const base = render(algo, file, 8, 1, activeFrame, `worker-a brightness base ${file}`);
            const basePeak = maxValue(base);
            algo.setBrightness(0.5);
            const half = render(algo, file, 8, 1, activeFrame, `worker-a brightness half ${file}`);
            const halfPeak = maxValue(half);
            assert(halfPeak > 0, `${file}: brightness 0.5 should keep non-black output`);
            if (file === "audiobladepower.js")
                assert(halfPeak < basePeak * 0.6, `${file}: expected source double-brightness behavior`);
            else
                assert(halfPeak < basePeak, `${file}: output brightness must reduce peak`);
            assertAllBounded(half, `${file} brightness-bounded`);
        }
        markCase("worker-a.output-brightness");
        console.log("PASS worker-a.output-brightness");
    }
}

function assertWorkerARegressionRows() {
    const EPS = 1e-6;

    function approx(actual, expected, label) {
        assert(Math.abs(actual - expected) <= EPS, `${label}: expected ${expected}, got ${actual}`);
    }

    function hsvAt(map, pixel) {
        const o = pixel * 3;
        return { h: map[o], s: map[o + 1], v: map[o + 2] };
    }

    function hsvToRgb(h, s, v) {
        const hue = ((h % 1) + 1) % 1;
        const sat = Math.max(0, Math.min(1, s));
        const val = Math.max(0, v);
        const i = Math.floor(hue * 6);
        const f = hue * 6 - i;
        const p = val * (1 - sat);
        const q = val * (1 - f * sat);
        const t = val * (1 - (1 - f) * sat);
        switch (i % 6) {
            case 0: return [val, t, p];
            case 1: return [q, val, p];
            case 2: return [p, val, t];
            case 3: return [p, q, val];
            case 4: return [t, p, val];
            default: return [val, p, q];
        }
    }

    function rgbAt(map, pixel) {
        const hsv = hsvAt(map, pixel);
        return hsvToRgb(hsv.h, hsv.s, hsv.v);
    }

    {
        const algo = loadScript("audiobladepower.js");
        algo.colors = [{ h: 0, s: 1, v: 1 }, { h: 2 / 3, s: 1, v: 1 }];
        algo.color = algo.colors[0];
        algo.setMirror("No");
        algo.setFlip("Off");
        algo.setBackgroundMode("Off");
        algo.setBackgroundBrightness(1);
        algo.setBlur(0);
        algo.setBrightness(1);
        algo.setMultiplier(0.5);
        algo.setDecay(0.7);
        algo.setFrequencyRange("Lows (beat+bass)");

        const lit = render(
            algo,
            "audiobladepower.js",
            10,
            1,
            audio({ beat: 0.3, bass: 0.3, low: 0.3, timing: { deltaSeconds: 1 / 60 } }),
            "workerA blade light 3 cells"
        );
        assert.strictEqual(lit.filter((_, i) => i % 3 === 2 && lit[i] > 1e-6).length, 3,
            "blade should light 3 cells at power 0.3");

        algo.setDecay(0.2);
        const decayed = render(
            algo,
            "audiobladepower.js",
            10,
            1,
            audio({ beat: 0, bass: 0, low: 0, timing: { deltaSeconds: 1 / 60 } }),
            "workerA blade decay after knob change"
        );
        assert(hsvAt(decayed, 0).v > 0, "blade state must persist after changing decay");
        markCase("worker-a.regression.blade-state-key");
        console.log("PASS worker-a.regression.blade-state-key");
    }

    {
        const algo = loadScript("audiobladepower.js");
        algo.colors = [{ h: 0, s: 1, v: 1 }, { h: 2 / 3, s: 1, v: 1 }];
        algo.color = algo.colors[0];
        algo.setMirror("No");
        algo.setFlip("Off");
        algo.setBackgroundMode("Off");
        algo.setBackgroundBrightness(1);
        algo.setBlur(0);
        algo.setBrightness(1);
        algo.setMultiplier(0.5);
        algo.setDecay(0.7);
        algo.setFrequencyRange("Lows (beat+bass)");

        render(
            algo,
            "audiobladepower.js",
            10,
            1,
            audio({
                available: true,
                profileId: "profile-a",
                sourceId: "mic",
                sourceEpoch: 3,
                configRevision: 12,
                beat: 0.3,
                bass: 0.3,
                low: 0.3,
                timing: { deltaSeconds: 1 / 60 }
            }),
            "workerA blade identity seed"
        );

        const held = render(
            algo,
            "audiobladepower.js",
            10,
            1,
            audio({
                available: false,
                status: "reset",
                profileId: "profile-a",
                sourceId: "",
                beat: 0,
                bass: 0,
                low: 0,
                timing: { deltaSeconds: 1 / 60 }
            }),
            "workerA blade identity hold through empty frame"
        );
        assert(hsvAt(held, 0).v > 0, "blade should retain state through unavailable frame on same profile");
        markCase("worker-a.regression.blade-identity-retain-empty");
        console.log("PASS worker-a.regression.blade-identity-retain-empty");
    }

    {
        const algo = loadScript("audiobladepower.js");
        algo.colors = [{ h: 0, s: 1, v: 1 }, { h: 2 / 3, s: 1, v: 1 }];
        algo.color = algo.colors[0];
        algo.setMirror("No");
        algo.setFlip("Off");
        algo.setBackgroundMode("Off");
        algo.setBackgroundBrightness(1);
        algo.setBlur(0);
        algo.setBrightness(1);
        algo.setMultiplier(0.5);
        algo.setDecay(0.7);
        algo.setFrequencyRange("Lows (beat+bass)");

        render(
            algo,
            "audiobladepower.js",
            10,
            1,
            audio({
                available: true,
                profileId: "profile-a",
                sourceId: "mic",
                sourceEpoch: 3,
                configRevision: 12,
                beat: 0.3,
                bass: 0.3,
                low: 0.3,
                timing: { deltaSeconds: 1 / 60 }
            }),
            "workerA blade identity reset seed"
        );

        const reset = render(
            algo,
            "audiobladepower.js",
            10,
            1,
            audio({
                available: true,
                profileId: "profile-a",
                sourceId: "mic",
                sourceEpoch: 4,
                configRevision: 12,
                beat: 0,
                bass: 0,
                low: 0,
                timing: { deltaSeconds: 1 / 60 }
            }),
            "workerA blade identity reset new epoch"
        );
        approx(hsvAt(reset, 0).v, 0, "blade should clear state on new source epoch");
        markCase("worker-a.regression.blade-identity-reset-new-epoch");
        console.log("PASS worker-a.regression.blade-identity-reset-new-epoch");
    }

    {
        const algo = loadScript("audiobladepower.js");
        algo.colors = [{ h: 0.3, s: 1, v: 1 }, { h: 0.3, s: 1, v: 1 }];
        algo.color = algo.colors[0];
        algo.setMirror("No");
        algo.setFlip("Off");
        algo.setBackgroundMode("Off");
        algo.setBackgroundBrightness(1);
        algo.setBlur(0);
        algo.setMultiplier(0.5);
        algo.setDecay(0.7);
        algo.setBrightness(0.5);
        const map = render(
            algo,
            "audiobladepower.js",
            10,
            1,
            audio({ beat: 1, bass: 1, low: 1, timing: { deltaSeconds: 1 / 60 } }),
            "workerA blade source double-brightness fresh"
        );
        approx(hsvAt(map, 0).v, 0.25, "blade fresh cells should follow source double-brightness");
        const tail = render(
            algo,
            "audiobladepower.js",
            10,
            1,
            audio({ beat: 0, bass: 0, low: 0, timing: { deltaSeconds: 1 / 60 } }),
            "workerA blade source double-brightness tail"
        );
        approx(hsvAt(tail, 0).v, 0.2, "blade retained tail should decay from source-scaled fill");
        markCase("worker-a.regression.blade-source-double-tail");
        console.log("PASS worker-a.regression.blade-source-double-tail");
    }

    {
        const algo = loadScript("audiobandsmatrix.js");
        algo.colors = [{ h: 0, s: 1, v: 1 }, { h: 2 / 3, s: 1, v: 1 }];
        algo.color = algo.colors[0];
        algo.setBandCount(2);
        algo.setFlipGradient("No");
        algo.setFlipBandOrder("No");
        algo.setFlip("Off");
        algo.setBackgroundMode("Off");
        algo.setBackgroundBrightness(1);
        algo.setBrightness(1);
        algo.setBlur(0);
        algo.setMirror("Yes");
        const map = render(
            algo,
            "audiobandsmatrix.js",
            4,
            1,
            audio({
                banks: { full: { count: 4, novelty: [1, 0, 0.5, 0], processed: [0, 0, 0, 0] } }
            }),
            "workerA bandsmatrix mirror fold"
        );
        const expected = [
            { h: 0, s: 1, v: 1 },
            { h: 11 / 12, s: 1, v: 1 },
            { h: 11 / 12, s: 1, v: 1 },
            { h: 0, s: 1, v: 1 }
        ];
        for (let i = 0; i < expected.length; i++) {
            approx(hsvAt(map, i).h, expected[i].h, `bandsmatrix mirror pixel${i}.h`);
            approx(hsvAt(map, i).s, expected[i].s, `bandsmatrix mirror pixel${i}.s`);
            approx(hsvAt(map, i).v, expected[i].v, `bandsmatrix mirror pixel${i}.v`);
        }
        markCase("worker-a.regression.bandsmatrix-mirror");
        console.log("PASS worker-a.regression.bandsmatrix-mirror");
    }

    {
        const algo = loadScript("audiospectralblocks.js");
        algo.colors = [{ h: 1 / 12, s: 1, v: 1 }, { h: 1 / 12, s: 1, v: 1 }];
        algo.color = algo.colors[0];
        algo.setBlockCount(1);
        algo.setFlip("Off");
        algo.setMirror("Off");
        algo.setBackgroundMode("Off");
        algo.setBackgroundBrightness(1);
        algo.setBlur(0);
        algo.setBrightness(1);
        const full = render(
            algo,
            "audiospectralblocks.js",
            1,
            1,
            audio({
                banks: { full: { count: 1, novelty: [Math.sqrt(2)], processed: [0] } }
            }),
            "workerA spectral overdrive full brightness"
        );
        const fullRgb = rgbAt(full, 0);
        approx(fullRgb[0], 1, "spectral overdrive full rgb.r");
        approx(fullRgb[1], 1, "spectral overdrive full rgb.g");
        approx(fullRgb[2], 0, "spectral overdrive full rgb.b");

        algo.setBrightness(0.5);
        const half = render(
            algo,
            "audiospectralblocks.js",
            1,
            1,
            audio({
                banks: { full: { count: 1, novelty: [Math.sqrt(2)], processed: [0] } }
            }),
            "workerA spectral overdrive half brightness"
        );
        const halfRgb = rgbAt(half, 0);
        approx(halfRgb[0], 1, "spectral overdrive half rgb.r");
        approx(halfRgb[1], 0.5, "spectral overdrive half rgb.g");
        approx(halfRgb[2], 0, "spectral overdrive half rgb.b");
        markCase("worker-a.regression.spectral-overdrive");
        console.log("PASS worker-a.regression.spectral-overdrive");
    }
}

function assertWorkerHRegressionsRows() {
    function frame(overrides) {
        return audio(Object.assign({
            version: 6,
            dt: 1 / 60,
            timing: { deltaSeconds: 1 / 60 },
            bpm: 120,
            sourceId: 'worker-h-regression-source',
            profileId: 188,
            sourceEpoch: 1,
            configRevision: 1,
            low: 0,
            mid: 0,
            high: 0,
            powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } },
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
            banks: { full: { count: 12, processed: new Array(12).fill(0), novelty: new Array(12).fill(0) } }
        }, overrides || {}));
    }

    function maxAbsDiff(a, b) {
        var d = 0;
        for (var i = 0; i < a.length; i++)
            d = Math.max(d, Math.abs(a[i] - b[i]));
        return d;
    }

    function valuesOnly(map) {
        var out = [];
        for (var i = 2; i < map.length; i += 3) out.push(map[i]);
        return out;
    }

    function peakValue(map) {
        var peak = 0;
        for (var i = 2; i < map.length; i += 3) peak = Math.max(peak, map[i]);
        return peak;
    }

    var lavaA = loadScript('audiolava.js', 0x2910);
    var lavaB = loadScript('audiolava.js', 0x2910);
    var artFrame1 = frame({
        dt: 0.05,
        timing: { deltaSeconds: 0.05 },
        low: 0.35,
        powers: { raw: { low: 0.35 } }
    });
    render(lavaA, 'audiolava.js', 9, 1, artFrame1, 'hreg lava art baseline A');
    render(lavaB, 'audiolava.js', 9, 1, artFrame1, 'hreg lava art baseline B');
    lavaA.setMode('LedFx Lava Lamp');
    render(lavaA, 'audiolava.js', 9, 1, frame({
        dt: 0.08,
        timing: { deltaSeconds: 0.08 },
        low: 0.7,
        powers: { raw: { low: 0.7 } }
    }), 'hreg lava ledfx step1');
    render(lavaA, 'audiolava.js', 9, 1, frame({
        dt: 0.08,
        timing: { deltaSeconds: 0.08 },
        low: 0.7,
        powers: { raw: { low: 0.7 } }
    }), 'hreg lava ledfx step2');
    lavaA.setMode('Artistic');
    var artFrame2 = frame({
        dt: 0.05,
        timing: { deltaSeconds: 0.05 },
        low: 0.35,
        powers: { raw: { low: 0.35 } }
    });
    var lavaOutA = render(lavaA, 'audiolava.js', 9, 1, artFrame2, 'hreg lava art post ledfx A');
    var lavaOutB = render(lavaB, 'audiolava.js', 9, 1, artFrame2, 'hreg lava art post ledfx B');
    assert.deepStrictEqual(lavaOutA, lavaOutB, 'hreg lava: LedFx time/state leaked into Artistic mode');
    markCase('hreg.lava.artistic-state-isolated');

    var crawl = loadScript('audiocrawler.js', 0x2911);
    crawl.setMode('LedFx Crawler');
    crawl.setSpeed(0);
    crawl.setReactivity(1);
    crawl.setChop(30);
    var crawlFrame = frame({
        dt: 0.2,
        timing: { deltaSeconds: 0.2 },
        low: 0.6,
        powers: { raw: { low: 0.6 } }
    });
    var crawlV1 = valuesOnly(render(crawl, 'audiocrawler.js', 10, 1, crawlFrame, 'hreg crawler speed0 first'));
    var crawlV2 = valuesOnly(render(crawl, 'audiocrawler.js', 10, 1, crawlFrame, 'hreg crawler speed0 second'));
    assert.notDeepStrictEqual(crawlV1, crawlV2, 'hreg crawler: wall-field clock t3 is not driving LedFx value field');
    markCase('hreg.crawler.t3-wall-field-clock');

    var blocksZero = loadScript('audioblocks.js', 0x2912);
    blocksZero.setMode('LedFx Block Reflections');
    var zeroFrame = frame({
        dt: 0,
        timing: { deltaSeconds: 0 },
        low: 0.9,
        powers: { raw: { low: 0.9 } }
    });
    var blocksZeroA = render(blocksZero, 'audioblocks.js', 8, 1, zeroFrame, 'hreg blocks zero dt first');
    var blocksZeroB = render(blocksZero, 'audioblocks.js', 8, 1, zeroFrame, 'hreg blocks zero dt second');
    assert.deepStrictEqual(blocksZeroA, blocksZeroB, 'hreg blocks: zero-time frame should not advance LedFx low filter');
    markCase('hreg.blocks.zero-dt-stable');

    var blocksA = loadScript('audioblocks.js', 0x2913);
    var blocksB = loadScript('audioblocks.js', 0x2913);
    blocksA.setMode('LedFx Block Reflections');
    blocksB.setMode('LedFx Block Reflections');
    var blocksLong = render(blocksA, 'audioblocks.js', 8, 1, frame({
        dt: 0.2,
        timing: { deltaSeconds: 0.2 },
        low: 0.8,
        powers: { raw: { low: 0.8 } }
    }), 'hreg blocks dt=0.2');
    render(blocksB, 'audioblocks.js', 8, 1, frame({
        dt: 0.1,
        timing: { deltaSeconds: 0.1 },
        low: 0.8,
        powers: { raw: { low: 0.8 } }
    }), 'hreg blocks dt=0.1 first');
    var blocksSplit = render(blocksB, 'audioblocks.js', 8, 1, frame({
        dt: 0.1,
        timing: { deltaSeconds: 0.1 },
        low: 0.8,
        powers: { raw: { low: 0.8 } }
    }), 'hreg blocks dt=0.1 second');
    assert(maxAbsDiff(blocksLong, blocksSplit) < 1e-9, 'hreg blocks: elapsed alpha cadence mismatch for equal total seconds');
    markCase('hreg.blocks.elapsed-alpha-cadence');

    var waterA = loadScript('audiowater.js', 0x2914);
    var waterB = loadScript('audiowater.js', 0x2914);
    waterA.setMode('LedFx Water');
    waterB.setMode('LedFx Water');
    var noTime = frame({
        dt: 0,
        timing: { deltaSeconds: 0 },
        low: 0,
        mid: 0,
        high: 0,
        banks: { full: { count: 12, processed: new Array(12).fill(0), novelty: new Array(12).fill(0) } }
    });
    render(waterA, 'audiowater.js', 10, 1, noTime, 'hreg water no-time init A');
    render(waterB, 'audiowater.js', 10, 1, noTime, 'hreg water no-time init B');
    render(waterA, 'audiowater.js', 10, 1, noTime, 'hreg water no-time extra A');
    var active = frame({
        dt: 1 / 60,
        timing: { deltaSeconds: 1 / 60 },
        low: 0.5,
        mid: 0.4,
        high: 0.3,
        banks: { full: { count: 12, processed: [1,1,0.8,0.8,0,0,0,0,0,0,0.6,0.6], novelty: new Array(12).fill(0) } }
    });
    var waterOutA = render(waterA, 'audiowater.js', 10, 1, active, 'hreg water active A');
    var waterOutB = render(waterB, 'audiowater.js', 10, 1, active, 'hreg water active B');
    assert.deepStrictEqual(waterOutA, waterOutB, 'hreg water: zero-time frame advanced LedFx emitter drift');
    markCase('hreg.water.no-drift-at-zero-time');

    var waterTiny = loadScript('audiowater.js', 0x2915);
    waterTiny.setMode('LedFx Water');
    waterTiny.setLedFxBackgroundMode('Additive');
    waterTiny.setLedFxBackgroundColor('#ff0000');
    waterTiny.setLedFxBackgroundBrightness(1);
    var tinyOut = render(waterTiny, 'audiowater.js', 2, 2, frame({
        dt: 0,
        timing: { deltaSeconds: 0 },
        low: 0,
        mid: 0,
        high: 0,
        banks: { full: { count: 12, processed: new Array(12).fill(0), novelty: new Array(12).fill(0) } }
    }), 'hreg water tiny additive transform');
    assert(peakValue(tinyOut) > 0.99, 'hreg water: tiny-N LedFx path skipped output/background controls');
    markCase('hreg.water.tiny-n-transforms-applied');
}

function assertWorkerDIdentityRows() {
    function mkFrame(sourceId, profileId, overrides) {
        return audio(Object.assign({
            version: 6,
            dt: 0,
            timing: { deltaSeconds: 0 },
            sourceId: sourceId,
            profileId: profileId,
            sourceEpoch: 1,
            configRevision: 1,
            available: true,
            status: "ok",
            pitch: { valid: false, hz: 0, midi: 0, confidence: 0 },
            banks: { full: { count: 6, processed: [0, 0, 0, 0, 0, 0], novelty: [0, 0, 0, 0, 0, 0] } },
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
            powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } }
        }, overrides || {}));
    }

    function runCase(caseId, fn) {
        try {
            if (typeof markCase === "function") markCase(caseId);
            fn();
            console.log(`PASS ${caseId}`);
            return { caseId: caseId, ok: true };
        } catch (err) {
            console.log(`FAIL ${caseId} :: ${err.message}`);
            return { caseId: caseId, ok: false, error: err.message };
        }
    }

    function caseRow18FilterIdentity() {
        const width = 7;
        const height = 1;
        const palette = [
            { h: 0.00, s: 1, v: 1 },
            { h: 0.33, s: 1, v: 1 },
            { h: 0.66, s: 1, v: 1 }
        ];

        function make(seed) {
            const algo = loadScript("audiofilter.js", seed);
            algo.setUseGradient("Yes");
            algo.setFrequencyRange("Beat");
            algo.setRollSpeed(0.8);
            algo.setBoost(0);
            algo.colors = palette;
            return algo;
        }

        const prime = mkFrame("src-A", 18, { beat: 1, dt: 0.1, timing: { deltaSeconds: 0.1 } });
        const middle = mkFrame("src-A", 18, { beat: 1, dt: 0.2, timing: { deltaSeconds: 0.2 } });
        const transient = mkFrame("", 18, {
            beat: 1,
            dt: 0.2,
            timing: { deltaSeconds: 0.2 },
            available: false,
            status: "reset",
            sourceEpoch: 1
        });
        const tail = mkFrame("src-A", 18, { beat: 1, dt: 0.15, timing: { deltaSeconds: 0.15 } });

        const keepA = make(0x4d61);
        render(keepA, "audiofilter.js", width, height, prime, "row18 identity prime keep");
        render(keepA, "audiofilter.js", width, height, transient, "row18 identity transient");
        const kept = render(keepA, "audiofilter.js", width, height, tail, "row18 identity kept");

        const keepB = make(0x4d61);
        render(keepB, "audiofilter.js", width, height, prime, "row18 identity prime ref");
        render(keepB, "audiofilter.js", width, height, middle, "row18 identity middle ref");
        const keptRef = render(keepB, "audiofilter.js", width, height, tail, "row18 identity kept ref");
        assert.deepStrictEqual(kept, keptRef, "row18 transient unavailable frame should preserve identity and roll history");

        const resetA = make(0x4d62);
        render(resetA, "audiofilter.js", width, height, prime, "row18 reset prime");
        render(resetA, "audiofilter.js", width, height, middle, "row18 reset middle");
        const switched = render(
            resetA,
            "audiofilter.js",
            width,
            height,
            mkFrame("src-B", 18, { beat: 1, dt: 0.15, timing: { deltaSeconds: 0.15 }, sourceEpoch: 2 }),
            "row18 reset switched"
        );
        const fresh = make(0x4d63);
        const freshSwitched = render(
            fresh,
            "audiofilter.js",
            width,
            height,
            mkFrame("src-B", 18, { beat: 1, dt: 0.15, timing: { deltaSeconds: 0.15 }, sourceEpoch: 2 }),
            "row18 reset fresh"
        );
        assert.deepStrictEqual(switched, freshSwitched, "row18 real source/epoch change should reset roll state");
    }

    function caseRow38PitchIdentity() {
        const width = 6;
        const height = 1;
        const palette = [
            { h: 0.00, s: 1, v: 1 },
            { h: 0.33, s: 1, v: 1 },
            { h: 0.66, s: 1, v: 1 }
        ];

        function make(seed) {
            const algo = loadScript("audiopitchspectrum.js", seed);
            algo.setFadeRate(0.25);
            algo.setResponsiveness(1);
            algo.setMirror("No");
            algo.setBlur(0);
            algo.colors = palette;
            return algo;
        }

        const prime = mkFrame("pitch-A", 38, {
            dt: 0.04,
            timing: { deltaSeconds: 0.04 },
            pitch: { valid: true, hz: 440, midi: 69, confidence: 0.9 },
            banks: { full: { count: 6, processed: [1, 1, 1, 1, 1, 1], novelty: [0, 0, 0, 0, 0, 0] } }
        });
        const fadeStep = mkFrame("pitch-A", 38, {
            dt: 0.04,
            timing: { deltaSeconds: 0.04 },
            pitch: { valid: false, hz: 0, midi: 0, confidence: 0 },
            banks: { full: { count: 6, processed: [0, 0, 0, 0, 0, 0], novelty: [0, 0, 0, 0, 0, 0] } }
        });
        const transientFadeStep = mkFrame("", 38, {
            dt: 0.04,
            timing: { deltaSeconds: 0.04 },
            profileId: 38,
            sourceEpoch: 1,
            available: false,
            status: "reset",
            pitch: { valid: false, hz: 0, midi: 0, confidence: 0 },
            banks: { full: { count: 6, processed: [0, 0, 0, 0, 0, 0], novelty: [0, 0, 0, 0, 0, 0] } }
        });

        const keepA = make(0x4d71);
        render(keepA, "audiopitchspectrum.js", width, height, prime, "row38 identity prime keep");
        const transientOut = render(keepA, "audiopitchspectrum.js", width, height, transientFadeStep, "row38 identity transient");

        const keepB = make(0x4d71);
        render(keepB, "audiopitchspectrum.js", width, height, prime, "row38 identity prime ref");
        const transientRef = render(keepB, "audiopitchspectrum.js", width, height, fadeStep, "row38 identity ref");
        assert.deepStrictEqual(transientOut, transientRef, "row38 transient unavailable frame should preserve stale-pitch fading history");

        const resetA = make(0x4d72);
        render(resetA, "audiopitchspectrum.js", width, height, prime, "row38 reset prime");
        const switched = render(
            resetA,
            "audiopitchspectrum.js",
            width,
            height,
            mkFrame("pitch-B", 38, {
                dt: 0.04,
                timing: { deltaSeconds: 0.04 },
                sourceEpoch: 2,
                pitch: { valid: false, hz: 0, midi: 0, confidence: 0 },
                banks: { full: { count: 6, processed: [0, 0, 0, 0, 0, 0], novelty: [0, 0, 0, 0, 0, 0] } }
            }),
            "row38 reset switched"
        );

        const fresh = make(0x4d73);
        const switchedFresh = render(
            fresh,
            "audiopitchspectrum.js",
            width,
            height,
            mkFrame("pitch-B", 38, {
                dt: 0.04,
                timing: { deltaSeconds: 0.04 },
                sourceEpoch: 2,
                pitch: { valid: false, hz: 0, midi: 0, confidence: 0 },
                banks: { full: { count: 6, processed: [0, 0, 0, 0, 0, 0], novelty: [0, 0, 0, 0, 0, 0] } }
            }),
            "row38 reset fresh"
        );
        assert.deepStrictEqual(switched, switchedFresh, "row38 real source/epoch change should reset pitch history");
    }

    const results = [
        runCase("row18-filter-identity-transient-vs-epoch-reset", caseRow18FilterIdentity),
        runCase("row38-pitch-identity-transient-vs-epoch-reset", caseRow38PitchIdentity)
    ];
    const failed = results.filter(r => !r.ok);
    console.log(`IDENTITY SUMMARY pass=${results.length - failed.length} fail=${failed.length}`);
    if (failed.length)
        throw new Error(`Identity cases failed: ${failed.map(r => r.caseId).join(", ")}`);
}

function assertWorkerJPlasmaWledBounds() {
    const script = 'audioplasma.js';

    function frame(overrides = {}) {
        const lows = overrides.low === undefined ? 0.8 : overrides.low;
        return audio(Object.assign({
            version: 6,
            sourceId: 'worker-j-wled-bounds',
            profileId: 41,
            sourceEpoch: 3,
            configRevision: 9,
            dt: 0.2,
            timing: { deltaSeconds: 0.2 },
            beat: lows,
            bass: lows,
            low: lows,
            mid: 0,
            high: 0
        }, overrides));
    }

    function assertFiniteMap(map, label) {
        for (let i = 0; i < map.length; i++) {
            const v = map[i];
            assert(Number.isFinite(v), `${label}: non-finite at ${i}`);
            assert(v >= 0 && v <= 1, `${label}: out of bounds ${v} at ${i}`);
        }
    }

    {
        const zero = loadScript(script, 0x9111);
        zero.setMode('PlasmaWled2d');
        zero.setFrequencyRange('Lows (beat+bass)');
        zero.setSpeedMultiplier(1);
        zero.setSizeMultiplier(1);
        zero.setSpeedDivisor(0);
        zero.setHorizontalStretch(0);
        zero.setVerticalStretch(0);
        assert.strictEqual(zero.getSpeedDivisor(), 0, 'speed divisor zero must persist');
        assert.strictEqual(zero.getHorizontalStretch(), 0, 'horizontal stretch zero must persist');
        assert.strictEqual(zero.getVerticalStretch(), 0, 'vertical stretch zero must persist');
        const zeroMap = render(zero, script, 8, 4, frame(), 'worker-j plasmawled zero-controls');
        assertFiniteMap(zeroMap, 'zero-controls render');

        const maxed = loadScript(script, 0x9111);
        maxed.setMode('PlasmaWled2d');
        maxed.setFrequencyRange('Lows (beat+bass)');
        maxed.setSpeedMultiplier(1);
        maxed.setSizeMultiplier(1);
        maxed.setSpeedDivisor(255);
        maxed.setHorizontalStretch(255);
        maxed.setVerticalStretch(255);
        assert.strictEqual(maxed.getSpeedDivisor(), 255, 'speed divisor 255 must persist');
        assert.strictEqual(maxed.getHorizontalStretch(), 255, 'horizontal stretch 255 must persist');
        assert.strictEqual(maxed.getVerticalStretch(), 255, 'vertical stretch 255 must persist');
        const maxMap = render(maxed, script, 8, 4, frame(), 'worker-j plasmawled max-controls');
        assertFiniteMap(maxMap, 'max-controls render');
        assert.notDeepStrictEqual(Array.from(zeroMap), Array.from(maxMap), '0..255 controls must change rendered geometry');
    }

    {
        const invalid = loadScript(script, 0x9222);
        invalid.setMode('PlasmaWled2d');
        invalid.setFrequencyRange('Lows (beat+bass)');
        invalid.setSpeedMultiplier(1);
        invalid.setSizeMultiplier(0);
        invalid.setSpeedDivisor('nope');
        invalid.setHorizontalStretch(undefined);
        invalid.setVerticalStretch({});
        assert.strictEqual(invalid.getSpeedDivisor(), 32, 'invalid speed divisor should restore default');
        assert.strictEqual(invalid.getHorizontalStretch(), 100, 'invalid horizontal stretch should restore default');
        assert.strictEqual(invalid.getVerticalStretch(), 100, 'invalid vertical stretch should restore default');

        invalid.setSpeedDivisor(-4);
        invalid.setHorizontalStretch(999);
        invalid.setVerticalStretch(260);
        assert.strictEqual(invalid.getSpeedDivisor(), 0, 'speed divisor must clamp at 0');
        assert.strictEqual(invalid.getHorizontalStretch(), 255, 'horizontal stretch must clamp at 255');
        assert.strictEqual(invalid.getVerticalStretch(), 255, 'vertical stretch must clamp at 255');

        invalid.setSpeedDivisor('nope');
        invalid.setHorizontalStretch(undefined);
        invalid.setVerticalStretch({});
        const invalidMap = render(invalid, script, 8, 4, frame({ low: 0.6 }), 'worker-j plasmawled invalid-input');
        assertFiniteMap(invalidMap, 'invalid-input render');

        const defaults = loadScript(script, 0x9222);
        defaults.setMode('PlasmaWled2d');
        defaults.setFrequencyRange('Lows (beat+bass)');
        defaults.setSpeedMultiplier(1);
        defaults.setSizeMultiplier(0);
        const defaultMap = render(defaults, script, 8, 4, frame({ low: 0.6 }), 'worker-j plasmawled default-controls');
        assert.deepStrictEqual(Array.from(invalidMap), Array.from(defaultMap), 'invalid input must match default-control rendering');
    }
    markCase('row41-plasmawled-bounds');
}

function assertWorkerGRegressions() {
function wgFrame(overrides) {
    return audio(Object.assign({
        version: 6,
        dt: 1 / 30,
        timing: { deltaSeconds: 1 / 60 },
        sourceId: 'worker-g-regression-source',
        profileId: 730,
        sourceEpoch: 1,
        configRevision: 1,
        banks: { full: { count: 10, processed: new Array(10).fill(0), novelty: new Array(10).fill(0) } },
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
    }, overrides || {}));
}

function peakV(map) {
    var peak = 0;
    for (var i = 2; i < map.length; i += 3) peak = Math.max(peak, map[i]);
    return peak;
}

function pixelV(map, pixelIndex) {
    return map[pixelIndex * 3 + 2];
}

function approx(actual, expected, eps, label) {
    assert(Math.abs(actual - expected) <= eps, `${label}: expected ${expected}, got ${actual}`);
}

function assertFiniteMap(map, label) {
    for (var i = 0; i < map.length; i++)
        assert(Number.isFinite(map[i]), `${label}: non-finite at index ${i} (${map[i]})`);
}

function row52SplitVsSingleAndNaNRecovery() {
    var processed = [0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var silent = new Array(10).fill(0);

    var split = loadScript('audiobarcode.js', 0x7701);
    split.setMode('Scroll+');
    split.setReferenceBlur(0);
    split.setReferenceMirror('Off');
    split.setReferenceBrightness(1);
    split.setReferencePalette('#ff0000,#00ff00,#0000ff');
    split.setScrollPlusSpeed(0.5);
    split.setScrollPlusDecay(0.5);
    split.setScrollPlusThreshold(0.1);
    render(split, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: 10, processed: processed, novelty: silent } }
    }), 'worker-g regression row52 split half');
    var splitFull = render(split, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: 10, processed: processed, novelty: silent } }
    }), 'worker-g regression row52 split full');
    render(split, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 3,
        timing: { deltaSeconds: 0.05 },
        banks: { full: { count: 10, processed: silent, novelty: silent } }
    }), 'worker-g regression row52 split decay A');
    var splitDecayB = render(split, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 4,
        timing: { deltaSeconds: 0.05 },
        banks: { full: { count: 10, processed: silent, novelty: silent } }
    }), 'worker-g regression row52 split decay B');
    var splitRatio = pixelV(splitDecayB, 0) / pixelV(splitFull, 0);
    approx(splitRatio, 0.855625, 1e-6, 'row52 split ratio');

    var single = loadScript('audiobarcode.js', 0x7702);
    single.setMode('Scroll+');
    single.setReferenceBlur(0);
    single.setReferenceMirror('Off');
    single.setReferenceBrightness(1);
    single.setReferencePalette('#ff0000,#00ff00,#0000ff');
    single.setScrollPlusSpeed(0.5);
    single.setScrollPlusDecay(0.5);
    single.setScrollPlusThreshold(0.1);
    render(single, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: 10, processed: processed, novelty: silent } }
    }), 'worker-g regression row52 single half');
    var singleFull = render(single, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: 10, processed: processed, novelty: silent } }
    }), 'worker-g regression row52 single full');
    var singleDecay = render(single, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 3,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: 10, processed: silent, novelty: silent } }
    }), 'worker-g regression row52 single decay');
    var singleRatio = pixelV(singleDecay, 0) / pixelV(singleFull, 0);
    approx(singleRatio, 0.85, 1e-6, 'row52 single ratio');
    assert(Math.abs(splitRatio - singleRatio) > 0.005, 'row52 split ratio must differ from single-step ratio');

    var recovery = loadScript('audiobarcode.js', 0x7703);
    recovery.setMode('Scroll+');
    recovery.setReferenceBlur(0);
    recovery.setReferenceMirror('Off');
    recovery.setReferenceBrightness(1);
    recovery.setReferencePalette('#ff0000,#00ff00,#0000ff');
    recovery.setScrollPlusSpeed(0.5);
    recovery.setScrollPlusDecay(0.5);
    recovery.setScrollPlusThreshold(0.1);
    var invalid = render(recovery, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 1,
        dt: Number.NaN,
        timing: { deltaSeconds: Number.NaN },
        banks: { full: { count: 10, processed: processed, novelty: silent } }
    }), 'worker-g regression row52 invalid timing');
    assertFiniteMap(invalid, 'row52 invalid timing');
    var recHalf = render(recovery, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: 10, processed: processed, novelty: silent } }
    }), 'worker-g regression row52 recover half');
    var recFull = render(recovery, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 3,
        timing: { deltaSeconds: 0.1 },
        banks: { full: { count: 10, processed: processed, novelty: silent } }
    }), 'worker-g regression row52 recover full');
    assert(peakV(recHalf) < 1e-9, 'row52 invalid timing recovery first valid frame must stay half-step dark');
    assert(peakV(recFull) > 0.4, 'row52 invalid timing recovery second valid frame must inject');

    markCase('worker-g-reg-row52-split-vs-single');
    markCase('worker-g-reg-row52-invalid-timing-recovery');
}

function row47PercussiveIdentityGeometry() {
    var algo = loadScript('audiostrobe.js', 0x7704);
    algo.setMode('Percussive RGB');
    algo.setStrobeColor('#00ff00');
    algo.setStrobeWidth(2);
    algo.setStrobeDecayRate(0);
    algo.setBassStrobeDecayRate(1);

    var lit = render(algo, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        sourceId: 'src-A',
        profileId: 501,
        sourceEpoch: 7,
        configRevision: 11,
        events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g regression row47 lit');
    assert(peakV(lit) > 0.9, 'row47 baseline onset must light strip');

    var transient = render(algo, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        sourceId: 'src-A',
        profileId: 501,
        sourceEpoch: 7,
        configRevision: 11,
        banks: { full: { count: 0, processed: [], novelty: [] } },
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g regression row47 transient');
    assert(peakV(transient) > 0.9, 'row47 transient same-source loss should retain state');

    var rebind = render(algo, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        sourceId: 'src-A',
        profileId: 501,
        sourceEpoch: 8,
        configRevision: 11,
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g regression row47 rebind');
    assert(peakV(rebind) < 1e-9, 'row47 source epoch rebind should clear state');

    var heightRebind = loadScript('audiostrobe.js', 0x7705);
    heightRebind.setMode('Percussive RGB');
    heightRebind.setStrobeColor('#00ff00');
    heightRebind.setStrobeWidth(2);
    heightRebind.setStrobeDecayRate(0);
    heightRebind.setBassStrobeDecayRate(1);
    render(heightRebind, 'audiostrobe.js', 8, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0.1 },
        sourceId: 'src-B',
        profileId: 502,
        sourceEpoch: 3,
        configRevision: 2,
        events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g regression row47 height lit');
    var heightChanged = render(heightRebind, 'audiostrobe.js', 8, 2, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0.1 },
        sourceId: 'src-B',
        profileId: 502,
        sourceEpoch: 3,
        configRevision: 2,
        events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
    }), 'worker-g regression row47 height changed');
    assert(peakV(heightChanged) < 1e-9, 'row47 geometry height change should clear state');

    markCase('worker-g-reg-row47-identity-transient-vs-rebind');
    markCase('worker-g-reg-row47-height-reset');
}

function row51ReferenceSeedStability() {
    var algo = loadScript('audiobarcode.js', 0x7706);
    algo.setMode('Reference');
    algo.setReferenceBlur(0);
    algo.setReferenceMirror('Off');
    algo.setReferenceBrightness(1);
    algo.setReferencePalette('#ff0000,#00ff00,#0000ff');
    algo.setReferenceSpeed(3);
    algo.setReferenceDecay(0.97);
    var processed = [0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var frameA = render(algo, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 1,
        timing: { deltaSeconds: 0 },
        banks: { full: { count: 10, processed: processed, novelty: processed } }
    }), 'worker-g regression row51 seed init');
    var frameB = render(algo, 'audiobarcode.js', 10, 1, wgFrame({
        frameSequence: 2,
        timing: { deltaSeconds: 0 },
        banks: { full: { count: 10, processed: processed, novelty: processed } }
    }), 'worker-g regression row51 seed repeat');
    assert(peakV(frameA) > 0.05, 'row51 initial zero-dt frame should seed current color');
    assert.deepStrictEqual(frameB, frameA, 'row51 repeated zero-dt frame should not shift or age');
    markCase('worker-g-reg-row51-seed-stable-at-zero-dt');
}

row52SplitVsSingleAndNaNRecovery();
row47PercussiveIdentityGeometry();
row51ReferenceSeedStability();
}

function assertWorkerLPitchTimingRows() {
    var EPS = 1e-6;
    var caseIds = [];
    var utilSandbox = { Math: Math, Float32Array: Float32Array };
    vm.createContext(utilSandbox);
    vm.runInContext(HSV_UTIL, utilSandbox, { filename: "hsvutil.js" });
    var HSV = utilSandbox.HSVUtil;

    function frame(overrides) {
        return audio(Object.assign({
            version: 6,
            dt: 1 / 60,
            timing: { deltaSeconds: 1 / 60 },
            sourceId: "worker-l-source",
            profileId: 38,
            sourceEpoch: 1,
            configRevision: 1,
            available: true,
            status: "ok",
            pitch: { valid: true, hz: 440, midi: 69, confidence: 0.92 },
            banks: {
                full: {
                    count: 5,
                    processed: [1, 0.75, 0.5, 0.25, 0],
                    novelty: [1, 0.75, 0.5, 0.25, 0]
                }
            },
            events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } },
            powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } }
        }, overrides || {}));
    }

    function makeAlgo(seed) {
        var algo = loadScript("audiopitchspectrum.js", seed);
        algo.setMirror("No");
        algo.setFlip("Off");
        algo.setBackgroundMode("Off");
        algo.setBackgroundColor("#000000");
        algo.setBackgroundBrightness(1);
        algo.setBrightness(1);
        algo.setBlur(0);
        algo.colors = [
            { h: 0.01, s: 1, v: 1 },
            { h: 0.13, s: 1, v: 1 },
            { h: 0.35, s: 0.95, v: 0.9 },
            { h: 0.62, s: 0.9, v: 0.95 },
            { h: 0.82, s: 1, v: 1 }
        ];
        return algo;
    }

    function rgbAt(map, index) {
        var o = index * 3;
        return HSV.hsvToRgb(map[o], map[o + 1], map[o + 2]);
    }

    function noteColorForMidi(colors, midi) {
        var scaled = HSV.clamp01((midi - 21) / (108 - 21));
        var hsv = HSV.gradientLedfxAt(colors, scaled);
        return HSV.hsvToRgb(hsv.h, hsv.s, hsv.v);
    }

    function maxV(map) {
        var peak = 0;
        for (var i = 2; i < map.length; i += 3) {
            if (map[i] > peak) peak = map[i];
        }
        return peak;
    }

    function maxDiff(a, b) {
        var peak = 0;
        for (var i = 0; i < a.length; i++) {
            var d = Math.abs(a[i] - b[i]);
            if (d > peak) peak = d;
        }
        return peak;
    }

    function approxRgb(actual, expected, label, eps) {
        var limit = eps === undefined ? EPS : eps;
        for (var i = 0; i < 3; i++) {
            assert(Math.abs(actual[i] - expected[i]) <= limit,
                label + ": channel " + i + " expected " + expected[i] + ", got " + actual[i]);
        }
    }

    function affineStep(prev, color, w, fade, steps) {
        var retain = 1 - fade;
        var a = retain * (1 - w);
        var b = retain * w;
        var decay = Math.pow(a, steps);
        var gain = 0;
        if (b > 0) {
            var denom = 1 - a;
            gain = Math.abs(denom) < 1e-12 ? b * steps : b * (1 - decay) / denom;
        }
        return [
            prev[0] * decay + color[0] * gain,
            prev[1] * decay + color[1] * gain,
            prev[2] * decay + color[2] * gain
        ];
    }

    function recordCase(caseId) {
        markCase(caseId);
        caseIds.push(caseId);
        console.log("PASS " + caseId);
    }

    (function caseZeroDtStableWithInitialCapture() {
        var algo = makeAlgo(0x4c01);
        algo.setFadeRate(0.5);
        algo.setResponsiveness(1);
        var first = render(algo, "audiopitchspectrum.js", 5, 1, frame({
            dt: 0,
            timing: { deltaSeconds: 0 },
            pitch: { valid: true, hz: 330, midi: 64, confidence: 0.8 },
            banks: { full: { count: 5, processed: [1, 1, 1, 1, 1], novelty: [0, 0, 0, 0, 0] } }
        }), "worker-l zero-dt first");
        var repeated = render(algo, "audiopitchspectrum.js", 5, 1, frame({
            dt: 0,
            timing: { deltaSeconds: 0 },
            pitch: { valid: true, hz: 330, midi: 64, confidence: 0.8 },
            banks: { full: { count: 5, processed: [1, 1, 1, 1, 1], novelty: [0, 0, 0, 0, 0] } }
        }), "worker-l zero-dt repeat");
        assert(maxV(first) > 0.01, "row38: initial zero-dt frame should capture current input");
        assert.deepStrictEqual(repeated, first, "row38: repeated zero-dt frame must not age state");
        recordCase("row38-workerL-zero-dt-stable");
    })();

    (function caseCadenceInvariantConstantInput() {
        function runAtHz(hz) {
            var algo = makeAlgo(0x4c02);
            algo.setFadeRate(0.5);
            algo.setResponsiveness(1);
            var ticks = Math.round(hz * 0.2);
            var maps = [];
            for (var i = 0; i < ticks; i++) {
                maps.push(render(algo, "audiopitchspectrum.js", 1, 1, frame({
                    dt: 2 / hz,
                    timing: { deltaSeconds: 1 / hz },
                    pitch: { valid: true, hz: 440, midi: 69, confidence: 0.9 },
                    banks: { full: { count: 1, processed: [1], novelty: [0] } }
                }), "worker-l cadence " + hz + "Hz step " + i));
            }
            return maps;
        }

        var hz25 = runAtHz(25).pop();
        var hz50 = runAtHz(50).pop();
        var hz60 = runAtHz(60).pop();
        var expectedColor = noteColorForMidi(makeAlgo(0x4c02).colors, 69);
        var expected = [expectedColor[0] * 0.5, expectedColor[1] * 0.5, expectedColor[2] * 0.5];
        approxRgb(rgbAt(hz25, 0), expected, "row38 cadence 25Hz steady value");
        approxRgb(rgbAt(hz50, 0), expected, "row38 cadence 50Hz steady value");
        approxRgb(rgbAt(hz60, 0), expected, "row38 cadence 60Hz steady value");
        assert(maxDiff(hz25, hz50) <= 1e-6, "row38: 25Hz and 50Hz equal-time output diverged");
        assert(maxDiff(hz25, hz60) <= 1e-6, "row38: 25Hz and 60Hz equal-time output diverged");
        console.log("WORKER-L CADENCE r25=" + rgbAt(hz25, 0)[0] + " r50=" + rgbAt(hz50, 0)[0] + " r60=" + rgbAt(hz60, 0)[0] + " expected=" + expected[0]);
        recordCase("row38-workerL-constant-input-cadence-25-50-60");
    })();

    (function caseNominalOneStepFormula() {
        var algo = makeAlgo(0x4c03);
        algo.setFadeRate(0.25);
        algo.setResponsiveness(1);
        algo.colors = [{ h: 0, s: 1, v: 1 }, { h: 2 / 3, s: 1, v: 1 }];
        var map = render(algo, "audiopitchspectrum.js", 1, 1, frame({
            dt: 2 / 60,
            timing: { deltaSeconds: 1 / 60 },
            pitch: { valid: true, hz: 622.25, midi: 75, confidence: 0.97 },
            banks: { full: { count: 1, processed: [0.25], novelty: [0] } }
        }), "worker-l nominal step");
        var noteColor = noteColorForMidi(algo.colors, 75);
        var expected = affineStep([0, 0, 0], noteColor, 0.25, 0.25, 1);
        approxRgb(rgbAt(map, 0), expected, "row38 one-step affine at nominal 60Hz");
        recordCase("row38-workerL-nominal-one-step-formula");
    })();

    (function caseBlendThenFadeOrder() {
        var algo = makeAlgo(0x4c04);
        algo.setResponsiveness(1);
        algo.colors = [{ h: 0.0, s: 1, v: 1 }, { h: 0.66, s: 1, v: 1 }];

        algo.setFadeRate(0);
        render(algo, "audiopitchspectrum.js", 1, 1, frame({
            dt: 2 / 60,
            timing: { deltaSeconds: 1 / 60 },
            pitch: { valid: true, hz: 261.63, midi: 60, confidence: 0.9 },
            banks: { full: { count: 1, processed: [1], novelty: [0] } }
        }), "worker-l blend prime");

        algo.setFadeRate(0.5);
        var stepped = render(algo, "audiopitchspectrum.js", 1, 1, frame({
            dt: 2 / 60,
            timing: { deltaSeconds: 1 / 60 },
            pitch: { valid: true, hz: 1479.98, midi: 90, confidence: 0.9 },
            banks: { full: { count: 1, processed: [0.4], novelty: [0] } }
        }), "worker-l blend test");

        var prev = noteColorForMidi(algo.colors, 60);
        var nextColor = noteColorForMidi(algo.colors, 90);
        var expected = affineStep(prev, nextColor, 0.4, 0.5, 1);
        var wrongOrder = [
            prev[0] * 0.5 * (1 - 0.4) + nextColor[0] * 0.4,
            prev[1] * 0.5 * (1 - 0.4) + nextColor[1] * 0.4,
            prev[2] * 0.5 * (1 - 0.4) + nextColor[2] * 0.4
        ];
        var actual = rgbAt(stepped, 0);
        approxRgb(actual, expected, "row38 blend-then-fade expected");
        assert(maxDiff(actual, wrongOrder) > 1e-3,
            "row38: output matched fade-then-blend instead of blend-then-fade");
        recordCase("row38-workerL-blend-then-fade-order");
    })();

    (function caseFadeOneAlwaysBlack() {
        var algo = makeAlgo(0x4c05);
        algo.setFadeRate(1);
        algo.setResponsiveness(1);
        var map = render(algo, "audiopitchspectrum.js", 3, 1, frame({
            dt: 0,
            timing: { deltaSeconds: 0 },
            pitch: { valid: true, hz: 523.25, midi: 72, confidence: 0.9 },
            banks: { full: { count: 3, processed: [1, 1, 1], novelty: [0, 0, 0] } }
        }), "worker-l fade one");
        assert(map.every(function(v, i) { return i % 3 !== 2 || v <= 1e-9; }),
            "row38: fadeRate=1 must remain black even with fresh input");
        recordCase("row38-workerL-fade1-black");
    })();

    (function caseFreshInputAndStaleP0Decay() {
        var algo = makeAlgo(0x4c06);
        algo.setFadeRate(0.5);
        algo.setResponsiveness(1);
        algo.colors = [{ h: 0, s: 1, v: 1 }, { h: 2 / 3, s: 1, v: 1 }];
        var freshA = render(algo, "audiopitchspectrum.js", 1, 1, frame({
            dt: 2 / 60,
            timing: { deltaSeconds: 1 / 60 },
            pitch: { valid: true, hz: 27.5, midi: 21, confidence: 0.95 },
            banks: { full: { count: 1, processed: [1], novelty: [0] } }
        }), "worker-l fresh first");
        var freshB = render(algo, "audiopitchspectrum.js", 1, 1, frame({
            dt: 2 / 60,
            timing: { deltaSeconds: 1 / 60 },
            pitch: { valid: true, hz: 27.5, midi: 21, confidence: 0.95 },
            banks: { full: { count: 1, processed: [1], novelty: [0] } }
        }), "worker-l fresh steady");
        var expectedFresh = [0.5, 0, 0];
        approxRgb(rgbAt(freshA, 0), expectedFresh, "row38 fresh frame at fadeRate 0.5");
        approxRgb(rgbAt(freshB, 0), expectedFresh, "row38 steady fresh frame at fadeRate 0.5");
        var stale = render(algo, "audiopitchspectrum.js", 1, 1, frame({
            dt: 0.6,
            timing: { deltaSeconds: 0.3 },
            pitch: { valid: false, hz: NaN, midi: 999, confidence: 0 },
            banks: { full: { count: 1, processed: [0], novelty: [0] } }
        }), "worker-l stale 0.3s decay");
        var staleRgb = rgbAt(stale, 0);
        var expectedScale = 0.5 * Math.pow(0.5, 18);
        var freshColor = [1, 0, 0];
        var expectedStale = freshColor.map(function(x) { return x * expectedScale; });
        approxRgb(staleRgb, expectedStale, "row38 stale P0 decays by 0.5^18 after 0.3s", 1e-8);
        var packedChannels = staleRgb.map(function(x) { return Math.round(x * 255); });
        assert(packedChannels[0] === 0 && packedChannels[1] === 0 && packedChannels[2] === 0,
            "row38: stale 0.3s output should quantize to packed 0");
        console.log("WORKER-L VALUES freshR=" + rgbAt(freshA, 0)[0] + " staleR=" + staleRgb[0] + " expectedStaleScale=" + expectedScale);
        recordCase("row38-workerL-fresh-positive-and-stale-p0-decay");
    })();

    (function caseEqualPDifferentPitchWithNonuniformGradient() {
        var lowAlgo = makeAlgo(0x4c07);
        var highAlgo = makeAlgo(0x4c07);
        lowAlgo.setFadeRate(0.3);
        highAlgo.setFadeRate(0.3);
        lowAlgo.setResponsiveness(1);
        highAlgo.setResponsiveness(1);
        var powers = [0.0, 0.25, 0.75, 0.1, 1.0];
        var low = render(lowAlgo, "audiopitchspectrum.js", 5, 1, frame({
            pitch: { valid: true, hz: 220, midi: 57, confidence: 0.88 },
            banks: { full: { count: 5, processed: powers, novelty: [0, 0, 0, 0, 0] } }
        }), "worker-l low pitch");
        var high = render(highAlgo, "audiopitchspectrum.js", 5, 1, frame({
            pitch: { valid: true, hz: 880, midi: 81, confidence: 0.88 },
            banks: { full: { count: 5, processed: powers, novelty: [0, 0, 0, 0, 0] } }
        }), "worker-l high pitch");
        assert.notDeepStrictEqual(low, high, "row38: equal P with different valid pitch must change color output");
        assert(maxDiff(rgbAt(low, 2), rgbAt(high, 2)) > 1e-3,
            "row38: nonuniform gradient center bin should differ across pitch");
        recordCase("row38-workerL-equal-p-different-valid-pitch-nonuniform-g");
    })();

    (function caseStaleSourceRetainAndTrueIdentityReset() {
        var width = 4;
        var keepA = makeAlgo(0x4c08);
        var keepB = makeAlgo(0x4c08);
        keepA.setFadeRate(0.25);
        keepB.setFadeRate(0.25);
        keepA.setResponsiveness(1);
        keepB.setResponsiveness(1);

        var prime = frame({
            dt: 2 / 60,
            timing: { deltaSeconds: 1 / 60 },
            sourceId: "pitch-A",
            sourceEpoch: 1,
            configRevision: 1,
            pitch: { valid: true, hz: 440, midi: 69, confidence: 0.9 },
            banks: { full: { count: width, processed: [1, 1, 1, 1], novelty: [0, 0, 0, 0] } }
        });
        render(keepA, "audiopitchspectrum.js", width, 1, prime, "worker-l identity prime A");
        render(keepB, "audiopitchspectrum.js", width, 1, prime, "worker-l identity prime B");

        var transient = render(keepA, "audiopitchspectrum.js", width, 1, frame({
            dt: 2 / 60,
            timing: { deltaSeconds: 1 / 60 },
            sourceId: "",
            sourceEpoch: 1,
            configRevision: 1,
            available: false,
            status: "reset",
            pitch: { valid: false, hz: 0, midi: 0, confidence: 0 },
            banks: { full: { count: width, processed: [0, 0, 0, 0], novelty: [0, 0, 0, 0] } }
        }), "worker-l identity transient");
        var reference = render(keepB, "audiopitchspectrum.js", width, 1, frame({
            dt: 2 / 60,
            timing: { deltaSeconds: 1 / 60 },
            sourceId: "pitch-A",
            sourceEpoch: 1,
            configRevision: 1,
            pitch: { valid: false, hz: 0, midi: 0, confidence: 0 },
            banks: { full: { count: width, processed: [0, 0, 0, 0], novelty: [0, 0, 0, 0] } }
        }), "worker-l identity reference");
        assert.deepStrictEqual(transient, reference,
            "row38: transient unavailable frame should preserve stale source identity");

        var switchedAlgo = makeAlgo(0x4c09);
        switchedAlgo.setFadeRate(0.25);
        switchedAlgo.setResponsiveness(1);
        render(switchedAlgo, "audiopitchspectrum.js", width, 1, prime, "worker-l identity switched prime");
        var switched = render(switchedAlgo, "audiopitchspectrum.js", width, 1, frame({
            dt: 2 / 60,
            timing: { deltaSeconds: 1 / 60 },
            sourceId: "pitch-B",
            sourceEpoch: 2,
            configRevision: 2,
            pitch: { valid: false, hz: 0, midi: 0, confidence: 0 },
            banks: { full: { count: width, processed: [0, 0, 0, 0], novelty: [0, 0, 0, 0] } }
        }), "worker-l identity switched");

        var fresh = makeAlgo(0x4c0a);
        fresh.setFadeRate(0.25);
        fresh.setResponsiveness(1);
        var freshMap = render(fresh, "audiopitchspectrum.js", width, 1, frame({
            dt: 2 / 60,
            timing: { deltaSeconds: 1 / 60 },
            sourceId: "pitch-B",
            sourceEpoch: 2,
            configRevision: 2,
            pitch: { valid: false, hz: 0, midi: 0, confidence: 0 },
            banks: { full: { count: width, processed: [0, 0, 0, 0], novelty: [0, 0, 0, 0] } }
        }), "worker-l identity fresh");
        assert.deepStrictEqual(switched, freshMap,
            "row38: true source identity change should reset pitch history");
        recordCase("row38-workerL-stale-source-and-true-identity-reset");
    })();

    (function caseMetadataAndBounds() {
        var algo = makeAlgo(0x4c0b);
        var byName = {};
        for (var i = 0; i < algo.properties.length; i++) {
            var fields = algo.properties[i].split("|");
            var parsed = {};
            for (var j = 0; j < fields.length; j++) {
                var cut = fields[j].indexOf(":");
                if (cut > 0) parsed[fields[j].slice(0, cut)] = fields[j].slice(cut + 1);
            }
            byName[parsed.name] = parsed;
        }
        assert(byName.fadeRate, "row38: fadeRate metadata missing");
        assert(byName.responsiveness, "row38: responsiveness metadata missing");
        assert.strictEqual(byName.fadeRate.values, "0,1", "row38: fadeRate bounds must stay 0,1");
        assert.strictEqual(byName.responsiveness.values, "0,1", "row38: responsiveness bounds must stay 0,1");
        assert.strictEqual(byName.fadeRate.type, "float", "row38: fadeRate type changed");
        assert.strictEqual(byName.responsiveness.type, "float", "row38: responsiveness type changed");

        algo.setFadeRate(-2);
        assert.strictEqual(algo.getFadeRate(), 0, "row38: fadeRate lower clamp broken");
        algo.setFadeRate(5);
        assert.strictEqual(algo.getFadeRate(), 1, "row38: fadeRate upper clamp broken");
        algo.setResponsiveness(-2);
        assert.strictEqual(algo.getResponsiveness(), 0, "row38: responsiveness lower clamp broken");
        algo.setResponsiveness(5);
        assert.strictEqual(algo.getResponsiveness(), 1, "row38: responsiveness upper clamp broken");
        recordCase("row38-workerL-metadata-and-bounds");
    })();

    var expectedCaseIds = [
        "row38-workerL-zero-dt-stable",
        "row38-workerL-constant-input-cadence-25-50-60",
        "row38-workerL-nominal-one-step-formula",
        "row38-workerL-blend-then-fade-order",
        "row38-workerL-fade1-black",
        "row38-workerL-fresh-positive-and-stale-p0-decay",
        "row38-workerL-equal-p-different-valid-pitch-nonuniform-g",
        "row38-workerL-stale-source-and-true-identity-reset",
        "row38-workerL-metadata-and-bounds"
    ];
    assert.deepStrictEqual(caseIds, expectedCaseIds, "worker-l case execution mismatch");
    console.log("WORKER-L CASE IDS " + caseIds.join(","));
}

function assertPitchNontrivialComposition(directory = SCRIPTS_DIR) {
    const file = 'audiopitchspectrum.js';
    const make = (fade, response, colors) => {
        const algo = loadScript(file, 0x3812, directory);
        algo.setBlur(0);
        algo.setMirror('No');
        algo.setFadeRate(fade);
        algo.setResponsiveness(response);
        algo.colors = colors;
        return algo;
    };
    const frame = (seconds, magnitude, midi) => audio({
        version: 6, available: true, sourceId: 'pitch-composition',
        profileId: 38, sourceEpoch: 1, configRevision: 1,
        timing: { deltaSeconds: seconds }, dt: seconds * 2,
        pitch: { valid: true, midi, hz: 440 * Math.pow(2, (midi - 69) / 12), confidence: 1 },
        banks: { full: { count: 1, processed: [magnitude], novelty: [0] } }
    });
    for (const hz of [25, 50, 60]) {
        const algo = make(0.5, 0.2, [{ h: 0, s: 0, v: 1 }]);
        render(algo, file, 1, 1, frame(0, 0, 69), 'empty composition initialization');
        let map;
        for (let i = 0; i < hz / 5; i++)
            map = render(algo, file, 1, 1, frame(1 / hz, 0.25, 69), `fractional injection ${hz}Hz`);
        assert(Math.abs(map[2] - 0.19999845330312382) < 1e-6,
            `nontrivial affine recurrence at ${hz}Hz: ${map[2]}`);
    }
    markCase('row38-nontrivial-affine-cadence');

    for (const hz of [25, 50, 60]) {
        const algo = make(0, 0.2, [{ h: 0, s: 1, v: 1 }, { h: 1 / 3, s: 1, v: 1 }]);
        render(algo, file, 1, 1, frame(0, 1, 21), 'pitch-filter initialization');
        let map;
        for (let i = 0; i < hz / 5; i++)
            map = render(algo, file, 1, 1, frame(1 / hz, 1, 108), `pitch-filter step ${hz}Hz`);
        const pixel = hsvPixels(map)[0];
        const expected = [0.019650779981437605, 0.9803492200185624, 0];
        for (let channel = 0; channel < 3; channel++)
            assert(Math.abs(pixel[channel] - expected[channel]) < 1e-6,
                `nontrivial pitch filter at ${hz}Hz channel ${channel}: ${pixel[channel]}`);
    }
    markCase('row38-nontrivial-midi-ema-cadence');
    console.log('PASS nontrivial affine injection and MIDI EMA at 25/50/60Hz');
}

function assertWorkerMCrawlerRows() {
    const SCRIPT = "audiocrawler.js";
    const caseIds = [];
    const utilSandbox = { Math, Float32Array };
    vm.createContext(utilSandbox);
    vm.runInContext(HSV_UTIL, utilSandbox, { filename: "hsvutil.js" });
    const HSV = utilSandbox.HSVUtil;

    function recordCase(caseId) {
        caseIds.push(caseId);
        markCase(caseId);
    }

    function setNeutralLedFxOutputControls(algo) {
        if (typeof algo.setLedFxFlip === "function") algo.setLedFxFlip("Off");
        if (typeof algo.setLedFxMirror === "function") algo.setLedFxMirror("No");
        if (typeof algo.setLedFxBackgroundMode === "function") algo.setLedFxBackgroundMode("Off");
        if (typeof algo.setLedFxBackgroundColor === "function") algo.setLedFxBackgroundColor("#000000");
        if (typeof algo.setLedFxBackgroundBrightness === "function") algo.setLedFxBackgroundBrightness(1);
        if (typeof algo.setLedFxBrightness === "function") algo.setLedFxBrightness(1);
        if (typeof algo.setLedFxBlur === "function") algo.setLedFxBlur(0);
    }

    function deepMerge(target, source) {
        if (!source || typeof source !== "object") return target;
        Object.keys(source).forEach(key => {
            const value = source[key];
            if (value && typeof value === "object" && !Array.isArray(value)) {
                target[key] = deepMerge(target[key] ? Object.assign({}, target[key]) : {}, value);
            } else {
                target[key] = value;
            }
        });
        return target;
    }

    function frame(overrides = {}) {
        return deepMerge(audio({
            version: 6,
            profileId: 11,
            sourceId: "worker-m-crawler",
            sourceEpoch: 1,
            configRevision: 1,
            available: true,
            status: "ok",
            dt: 1 / 60,
            timing: { deltaSeconds: 1 / 60 },
            powers: { raw: { beat: 0, bass: 0, low: 0, mid: 0, high: 0 } }
        }), overrides);
    }

    function maxAbsDiff(a, b) {
        let max = 0;
        for (let i = 0; i < a.length; i++) max = Math.max(max, Math.abs(a[i] - b[i]));
        return max;
    }

    function valuesOnly(map) {
        const out = [];
        for (let i = 2; i < map.length; i += 3) out.push(map[i]);
        return out;
    }

    function sourceRawLow(audioFrame) {
        if (audioFrame && audioFrame.powers && audioFrame.powers.raw && Number.isFinite(audioFrame.powers.raw.low))
            return HSV.clamp01(audioFrame.powers.raw.low);
        return HSV.clamp01(audioFrame && Number.isFinite(audioFrame.low) ? audioFrame.low : 0);
    }

    function nominalAlpha60(dtSeconds, nominal) {
        if (!(dtSeconds > 0) || !(nominal > 0)) return 0;
        return 1 - Math.pow(1 - nominal, dtSeconds * 60);
    }

    function smoothedStepSum(previous, target, nominalAlpha, stepCount) {
        if (!(stepCount > 0)) return 0;
        if (!(nominalAlpha > 0)) return previous * stepCount;
        if (nominalAlpha >= 1) return target * stepCount;
        const decay = 1 - nominalAlpha;
        return (stepCount * target) +
            ((previous - target) * decay * (1 - Math.pow(decay, stepCount)) / nominalAlpha);
    }

    function sourceCrawlerModelStep(state, algo, width, height, audioFrame) {
        const audioKey = HSV.audioIdentityKey(audioFrame, state.audioKey);
        const geometryKey = `${width}x${height}`;
        if (audioKey !== state.audioKey || geometryKey !== state.geometryKey) {
            state.audioKey = audioKey;
            state.geometryKey = geometryKey;
            state.filteredLow = 0;
            state.fieldMs = 0;
            state.timestepMs = 0;
        }

        const dtSeconds = HSV.audioSeconds(audioFrame);
        const dtMs = dtSeconds * 1000;
        const rawLow = sourceRawLow(audioFrame);
        const nominalSteps = dtSeconds * 60;
        const previousLow = state.filteredLow;
        state.filteredLow += (rawLow - state.filteredLow) * nominalAlpha60(dtSeconds, 0.1);
        const filteredStepSum = smoothedStepSum(previousLow, rawLow, 0.1, nominalSteps);

        state.fieldMs += dtMs;
        state.timestepMs += dtMs + (filteredStepSum * algo.reactivity * algo.speed * 1000);

        const t1 = HSV.time01(algo.speed * algo.sway, state.timestepMs);
        const t3 = HSV.time01(algo.speed * algo.chop + state.filteredLow * algo.reactivity, state.fieldMs);
        const sinT1 = 0.5 + 0.5 * Math.sin(t1 * 2 * Math.PI);

        const map = HSV.createMap(width, height);
        const pixelCount = Math.max(1, width * height);
        const denominator = Math.max(1, pixelCount - 1);
        const stretch = Math.max(0.00001, algo.stretch);
        const stretchSpan = stretch / 10;
        const gradientStops = (algo.hasUserColors && Array.isArray(algo.colors) && algo.colors.length >= 2)
            ? algo.colors
            : [
                { h: 0 / 3, s: 1, v: 1 },
                { h: 1 / 3, s: 1, v: 1 },
                { h: 2 / 3, s: 1, v: 1 }
            ];

        for (let i = 0; i < pixelCount; i++) {
            const x = i % width;
            const y = Math.floor(i / width);
            const i1 = i / denominator;

            let h = (i + t3 * pixelCount) / pixelCount;
            h *= stretch;
            h = ((h % stretchSpan) + stretchSpan) % stretchSpan;
            h += i1;
            h += sinT1;

            let v = 0.5 + 0.5 * Math.sin(h * 2 * Math.PI);
            v *= v;

            const hMod = ((h % 1) + 1) % 1;
            const base = HSV.gradientRgbAt(gradientStops, hMod);
            const rgbOut = [base[0] * v, base[1] * v, base[2] * v];
            const hsvOut = HSV.rgbToHsvUnclipped(rgbOut[0], rgbOut[1], rgbOut[2]);
            HSV.setPixel(map, width, x, y, hsvOut.h, hsvOut.s, hsvOut.v);
        }

        return Array.from(map);
    }

    (function caseSourceGoldensSmallGrid() {
        const width = 4;
        const height = 2;
        const algo = loadScript(SCRIPT, 0x11aa);
        setNeutralLedFxOutputControls(algo);
        algo.setMode("LedFx Crawler");
        algo.setSpeed(0.61);
        algo.setReactivity(0.42);
        algo.setSway(17);
        algo.setChop(23);
        algo.setStretch(2.9);
        const model = { filteredLow: 0, fieldMs: 0, timestepMs: 0, audioKey: "", geometryKey: "" };
        const step1 = frame({ dt: 0.2, timing: { deltaSeconds: 0.1 }, low: 0.05, powers: { raw: { low: 0.8 } } });
        const step2 = frame({ dt: 0.2, timing: { deltaSeconds: 0.1 }, low: 0.95, powers: { raw: { low: 0.3 } } });

        const expected1 = sourceCrawlerModelStep(model, algo, width, height, step1);
        const actual1 = render(algo, SCRIPT, width, height, step1, "worker-m row11 source-golden step1");
        const expected2 = sourceCrawlerModelStep(model, algo, width, height, step2);
        const actual2 = render(algo, SCRIPT, width, height, step2, "worker-m row11 source-golden step2");
        assert(maxAbsDiff(actual1, expected1) < 1e-9, `row11 source golden step1 mismatch, max diff ${maxAbsDiff(actual1, expected1)}`);
        assert(maxAbsDiff(actual2, expected2) < 1e-9, `row11 source golden step2 mismatch, max diff ${maxAbsDiff(actual2, expected2)}`);
        recordCase("row11.workerM.source-golden-small-grid");
    })();

    (function caseRawLowVsFilteredDecoy() {
        const width = 7;
        const height = 2;

        const sameRawA = loadScript(SCRIPT, 0x11ab);
        const sameRawB = loadScript(SCRIPT, 0x11ab);
        setNeutralLedFxOutputControls(sameRawA);
        setNeutralLedFxOutputControls(sameRawB);
        sameRawA.setMode("LedFx Crawler");
        sameRawB.setMode("LedFx Crawler");
        const decoyLowA = render(sameRawA, SCRIPT, width, height, frame({ low: 0.1, powers: { raw: { low: 0.7 } } }), "worker-m row11 decoy lowA");
        const decoyLowB = render(sameRawB, SCRIPT, width, height, frame({ low: 0.95, powers: { raw: { low: 0.7 } } }), "worker-m row11 decoy lowB");
        assert.deepStrictEqual(decoyLowA, decoyLowB, "row11 decoy: filtered low should not replace raw low input");

        const rawLo = loadScript(SCRIPT, 0x11ac);
        const rawHi = loadScript(SCRIPT, 0x11ac);
        setNeutralLedFxOutputControls(rawLo);
        setNeutralLedFxOutputControls(rawHi);
        rawLo.setMode("LedFx Crawler");
        rawHi.setMode("LedFx Crawler");
        const mapLo = render(rawLo, SCRIPT, width, height, frame({ low: 0.4, powers: { raw: { low: 0.2 } } }), "worker-m row11 raw lo");
        const mapHi = render(rawHi, SCRIPT, width, height, frame({ low: 0.4, powers: { raw: { low: 0.8 } } }), "worker-m row11 raw hi");
        assert.notDeepStrictEqual(mapLo, mapHi, "row11 raw low must affect LedFx Crawler output");
        recordCase("row11.workerM.raw-low-vs-decoy");
    })();

    (function caseZeroDtDuplicateStable() {
        const width = 9;
        const height = 1;
        const algo = loadScript(SCRIPT, 0x11ad);
        setNeutralLedFxOutputControls(algo);
        algo.setMode("LedFx Crawler");
        render(algo, SCRIPT, width, height, frame({ timing: { deltaSeconds: 0.05 }, powers: { raw: { low: 0.6 } } }), "worker-m row11 zero-dt prime");
        const zero = frame({ dt: 0, timing: { deltaSeconds: 0 }, low: 0.2, powers: { raw: { low: 0.9 } } });
        const a = render(algo, SCRIPT, width, height, zero, "worker-m row11 zero-dt A");
        const b = render(algo, SCRIPT, width, height, zero, "worker-m row11 zero-dt B");
        assert.deepStrictEqual(a, b, "row11 zero-dt duplicate changed output");
        recordCase("row11.workerM.zero-dt-duplicate-stable");
    })();

    (function caseMatchedTimeConstantInputFilter() {
        const width = 8;
        const height = 1;
        const oneShot = loadScript(SCRIPT, 0x11ae);
        const split = loadScript(SCRIPT, 0x11ae);
        setNeutralLedFxOutputControls(oneShot);
        setNeutralLedFxOutputControls(split);
        oneShot.setMode("LedFx Crawler");
        split.setMode("LedFx Crawler");

        const longStep = render(oneShot, SCRIPT, width, height, frame({
            dt: 0.4,
            timing: { deltaSeconds: 0.2 },
            low: 0.1,
            powers: { raw: { low: 0.75 } }
        }), "worker-m row11 cadence long");
        render(split, SCRIPT, width, height, frame({
            dt: 0.2,
            timing: { deltaSeconds: 0.1 },
            low: 0.1,
            powers: { raw: { low: 0.75 } }
        }), "worker-m row11 cadence split step1");
        const splitStep = render(split, SCRIPT, width, height, frame({
            dt: 0.2,
            timing: { deltaSeconds: 0.1 },
            low: 0.1,
            powers: { raw: { low: 0.75 } }
        }), "worker-m row11 cadence split step2");

        assert(maxAbsDiff(longStep, splitStep) < 1e-9, `row11 matched cadence mismatch, max diff ${maxAbsDiff(longStep, splitStep)}`);
        recordCase("row11.workerM.matched-timeconstant-inputfilter");
    })();

    (function caseModeIsolationArtisticControl() {
        const width = 7;
        const height = 2;
        const test = loadScript(SCRIPT, 0x11af);
        const control = loadScript(SCRIPT, 0x11af);
        setNeutralLedFxOutputControls(test);
        setNeutralLedFxOutputControls(control);

        const artisticBase = frame({ dt: 0.1, timing: { deltaSeconds: 0.05 }, low: 0.4, powers: { raw: { low: 0.4 } } });
        const artisticFinal = frame({ dt: 0.1, timing: { deltaSeconds: 0.05 }, low: 0.25, powers: { raw: { low: 0.25 } } });

        render(test, SCRIPT, width, height, artisticBase, "worker-m row11 mode-isolation artistic base test");
        render(control, SCRIPT, width, height, artisticBase, "worker-m row11 mode-isolation artistic base control");

        test.setMode("LedFx Crawler");
        render(test, SCRIPT, width, height, frame({ dt: 0.2, timing: { deltaSeconds: 0.1 }, low: 0.9, powers: { raw: { low: 0.9 } } }), "worker-m row11 mode-isolation ledfx 1");
        render(test, SCRIPT, width, height, frame({ dt: 0.2, timing: { deltaSeconds: 0.1 }, low: 0.8, powers: { raw: { low: 0.8 } } }), "worker-m row11 mode-isolation ledfx 2");

        test.setMode("Artistic");
        const testFinal = render(test, SCRIPT, width, height, artisticFinal, "worker-m row11 mode-isolation artistic final test");
        const controlFinal = render(control, SCRIPT, width, height, artisticFinal, "worker-m row11 mode-isolation artistic final control");
        assert.deepStrictEqual(testFinal, controlFinal, "row11 mode isolation failed: LedFx state leaked into Artistic evolution");
        recordCase("row11.workerM.mode-isolation-art-ledfx-art");
    })();

    (function caseControlsAndGuards() {
        const width = 7;
        const height = 2;
        const base = loadScript(SCRIPT, 0x11b0);
        const chop = loadScript(SCRIPT, 0x11b0);
        const sway = loadScript(SCRIPT, 0x11b0);
        setNeutralLedFxOutputControls(base);
        setNeutralLedFxOutputControls(chop);
        setNeutralLedFxOutputControls(sway);
        base.setMode("LedFx Crawler");
        chop.setMode("LedFx Crawler");
        sway.setMode("LedFx Crawler");
        chop.setChop(80);
        sway.setSway(45);

        const input = frame({ dt: 0.24, timing: { deltaSeconds: 0.12 }, low: 0.6, powers: { raw: { low: 0.6 } } });
        const baseMap = render(base, SCRIPT, width, height, input, "worker-m row11 controls base");
        const chopMap = render(chop, SCRIPT, width, height, input, "worker-m row11 controls chop");
        const swayMap = render(sway, SCRIPT, width, height, input, "worker-m row11 controls sway");

        assert.notDeepStrictEqual(valuesOnly(baseMap), valuesOnly(chopMap), "row11 control: chop did not change value field");
        assert.notDeepStrictEqual(valuesOnly(baseMap), valuesOnly(swayMap), "row11 control: sway did not change value field");

        const safe = loadScript(SCRIPT, 0x11b1);
        setNeutralLedFxOutputControls(safe);
        safe.setMode("LedFx Crawler");
        safe.setSpeed(0);
        safe.setStretch(0);
        const safeMap = render(safe, SCRIPT, width, height, frame({
            dt: 0.5,
            timing: { deltaSeconds: 0.25 },
            low: 0.7,
            powers: { raw: { low: 0.7 } }
        }), "worker-m row11 controls safe");
        assert(safeMap.every(Number.isFinite), "row11 guard: speed=0/stretch=0 produced non-finite values");
        recordCase("row11.workerM.controls-and-guards");
    })();

    const expectedCaseIds = [
        "row11.workerM.source-golden-small-grid",
        "row11.workerM.raw-low-vs-decoy",
        "row11.workerM.zero-dt-duplicate-stable",
        "row11.workerM.matched-timeconstant-inputfilter",
        "row11.workerM.mode-isolation-art-ledfx-art",
        "row11.workerM.controls-and-guards"
    ];
    assert.deepStrictEqual(caseIds, expectedCaseIds, "worker-m case execution mismatch");
    console.log("WORKER-M CASE IDS " + caseIds.join(","));
}

function assertMatrixResponseGeometry() {
    const approx = (actual, expected, eps, label) =>
        assert(Math.abs(actual - expected) <= eps, `${label}: expected ${expected}, got ${actual}`);
    const scanFile = 'audioscanmulti.js';
    const scanFrame = audio({
        version: 6, low: 0.1, mid: 0.25, high: 0.4,
        timing: { deltaSeconds: 0 }, dt: 0
    });
    const scanCases = [
        { width: 4, rows: [[0.2, 0, 0], [0.2, 0, 0], [0, 0.5, 0],
            [0, 0.5, 0], [0, 0, 0.8], [0, 0, 0.8]], coverage: [1, 0, 0, 0] },
        { width: 6, rows: [[0.2, 0, 0], [0, 0.5, 0], [0, 0, 0.8]],
            coverage: [1, 2 / 3, 0, 0, 0, 0] },
        { width: 3, rows: [[0.2 * 2 / 3, 0.5 / 3, 0], [0, 0.5 / 3, 0.8 * 2 / 3]],
            coverage: [1, 0, 0] }
    ];
    for (const test of scanCases) {
        for (const axis of ['Horizontal', 'Vertical']) {
            const algo = loadScript(scanFile);
            algo.setMode('LedFx Scan Multi');
            algo.setAxis(axis);
            algo.setFilter('Off');
            algo.setWidth(25);
            algo.setSpeed(0);
            algo.setBlur(0);
            algo.colors = [{ h: 0, s: 1, v: 1 }, { h: 1 / 3, s: 1, v: 1 },
                { h: 2 / 3, s: 1, v: 1 }];
            const width = axis === 'Horizontal' ? test.width : test.rows.length;
            const height = axis === 'Horizontal' ? test.rows.length : test.width;
            const pixels = hsvPixels(render(algo, scanFile, width, height, scanFrame, 'matrix lanes'));
            for (let y = 0; y < test.rows.length; y++) {
                for (let x = 0; x < test.width; x++) {
                    const index = axis === 'Horizontal' ? y * width + x : x * width + y;
                    for (let channel = 0; channel < 3; channel++)
                        approx(pixels[index][channel], test.rows[y][channel] * test.coverage[x],
                            1e-6, `${axis} ${width}x${height} lane pixel ${x},${y},${channel}`);
                }
            }
        }
    }

    const bandsFile = 'audiobandsmatrix.js';
    const bandsCases = [
        { count: 3, width: 5, samples: [...Array(7).fill(0.1), ...Array(7).fill(0.5), ...Array(6).fill(0.75)],
            rows: [[0, 1, 1, 0, 0], [0, 1, 1, 1, 1], [0, 0, 0, 1, 1], [0.4, 0, 0, 1, 1]] },
        { count: 4, width: 4, flipOrder: true,
            samples: [...Array(4).fill(0.1), ...Array(4).fill(0.5), ...Array(4).fill(0.75), ...Array(4).fill(0.25)],
            rows: [[1, 0, 1, 0], [0, 1, 1, 0], [0, 1, 0, 0], [0, 1, 0, 0.4]] },
        { count: 16, width: 4, samples: [...Array(15).fill(0), 1],
            rows: Array.from({ length: 4 }, () => [0, 0, 0, 1]) },
        { count: 3, width: 7, samples: [...Array(7).fill(0.5), ...Array(7).fill(0.25), ...Array(7).fill(1)],
            rows: [[0, 0, 0.75, 0.75, 1, 1, 1], [0.5, 0.5, 0, 0, 1, 1, 1], [1, 1, 0, 0, 1, 1, 1]] }
    ];
    for (const test of bandsCases) {
        const algo = loadScript(bandsFile);
        algo.setBandCount(test.count);
        algo.setFlipBandOrder(test.flipOrder ? 'Yes' : 'No');
        algo.colors = [{ h: 0, s: 0, v: 1 }];
        const frame = audio({ banks: { full: { count: test.samples.length,
            novelty: test.samples, processed: Array(test.samples.length).fill(1) } } });
        const map = render(algo, bandsFile, test.width, test.rows.length, frame, 'matrix band heights');
        test.rows.flat().forEach((expected, pixel) =>
            approx(map[pixel * 3 + 2], expected, 1e-6, `matrix band pixel ${pixel}`));
    }
    for (const flipGradient of ['No', 'Yes']) {
        const algo = loadScript(bandsFile);
        algo.setBandCount(2);
        algo.setFlipGradient(flipGradient);
        algo.colors = [{ h: 0, s: 1, v: 1 }, { h: 2 / 3, s: 1, v: 1 }];
        const frame = audio({ banks: { full: { count: 16,
            novelty: Array(16).fill(1), processed: Array(16).fill(0) } } });
        const pixels = hsvPixels(render(algo, bandsFile, 4, 4, frame, 'matrix alternating gradients'));
        const expected = flipGradient === 'No' ? [1, 0, 0] : [0, 0, 1];
        for (const pixel of [0, 14])
            pixels[pixel].forEach((value, channel) =>
                approx(value, expected[channel], 1e-6, `matrix gradient anchor ${pixel}`));
        assert.deepStrictEqual(pixels[12], pixels[2], 'Odd bands reverse gradient and coverage together');
    }
    console.log('PASS matrix response geometry: literal scan lanes, thin-axis overlap, full-spectrum bars and gradient anchors');
}

function assertMatrixLayoutChoices() {
    function assertLayoutDescriptor(file) {
        const algo = loadScript(file);
        const layout = algo.properties.map(parseProperty).filter(property => property.name === 'matrixLayout');
        assert.strictEqual(layout.length, 1, `${file}: expected one matrixLayout descriptor`);
        const descriptor = layout[0];
        assert.strictEqual(descriptor.type, 'list', `${file}: matrixLayout must be a list`);
        assert.strictEqual(descriptor.display, 'Layout', `${file}: matrixLayout display label`);
        assert.strictEqual(descriptor.values, 'Matrix,Repeated rows', `${file}: matrixLayout values`);
        assert.strictEqual(typeof algo[descriptor.write], 'function', `${file}: matrixLayout write accessor`);
        assert.strictEqual(typeof algo[descriptor.read], 'function', `${file}: matrixLayout read accessor`);
        assert.strictEqual(algo[descriptor.read](), 'Matrix', `${file}: matrixLayout default`);
        algo[descriptor.write]('Repeated rows');
        assert.strictEqual(algo[descriptor.read](), 'Repeated rows', `${file}: matrixLayout repeated rows setter`);
        algo[descriptor.write]('Matrix');
        assert.strictEqual(algo[descriptor.read](), 'Matrix', `${file}: matrixLayout matrix setter`);
    }

    function layoutFrame(tick) {
        const noveltyBase = [0.15, 0.82, 0.37, 0.66, 0.28, 0.74, 0.41, 0.93];
        const processedBase = [0.71, 0.19, 0.59, 0.31, 0.88, 0.23, 0.47, 0.54];
        const rotate = (values, amount) =>
            values.map((_, index) => values[(index + amount) % values.length]);
        return audio({
            version: 6,
            low: [0.12, 0.58, 0.24, 0.83, 0.41, 0.67][tick % 6],
            mid: [0.21, 0.36, 0.62, 0.27, 0.53, 0.44][tick % 6],
            high: [0.74, 0.29, 0.61, 0.47, 0.35, 0.78][tick % 6],
            dt: tick === 2 ? 0 : 0.16,
            timing: { deltaSeconds: tick === 2 ? 0 : 0.08 },
            sourceEpoch: tick < 4 ? 1 : 2,
            banks: {
                full: {
                    count: noveltyBase.length,
                    novelty: rotate(noveltyBase, tick % noveltyBase.length),
                    processed: rotate(processedBase, (tick * 2) % processedBase.length)
                }
            },
            events: { delta: { onset: tick === 3 ? 1 : 0, beat: tick === 4 ? 1 : 0, kick: 0, bar: 0 } }
        });
    }

    function configure(file, algo) {
        if (file === 'audioscanmulti.js') {
            algo.setMode('LedFx Scan Multi');
            algo.setAxis('Vertical');
            algo.setSpeed(27);
            algo.setWidth(34);
            algo.setBounce('Yes');
            algo.setSourceMode('Melbank');
            algo.setMelbank('Processed');
            algo.setFilter('On');
            algo.setAttack(0.35);
            algo.setDecay(0.14);
            algo.setGradient('Yes');
        } else {
            algo.setBandCount(6);
            algo.setFlipGradient('Yes');
            algo.setFlipBandOrder('No');
        }
        algo.setFlip('On');
        algo.setMirror('On');
        algo.setBackgroundMode('Additive');
        algo.setBackgroundColor('#224466');
        algo.setBackgroundBrightness(0.4);
        algo.setBrightness(0.85);
        algo.setBlur(1.15);
    }

    function applyTickControls(file, algo, tick) {
        if (file === 'audioscanmulti.js') {
            if (tick === 1) algo.setSourceMode('Power');
            if (tick === 2) {
                algo.setSourceMode('Melbank');
                algo.setMelbank('Novelty');
            }
            if (tick === 3) algo.setBounce('No');
            if (tick === 4) algo.setAxis('Horizontal');
            if (tick === 5) algo.setFilter('Off');
        } else {
            if (tick === 2) algo.setFlipBandOrder('Yes');
            if (tick === 3) algo.setFlipGradient('No');
            if (tick === 4) algo.setBandCount(3);
            if (tick === 5) algo.setMirror('No');
        }
    }

    function assertRowsEqualToStrip(matrixMap, stripMap, width, height, label) {
        const rowSize = width * 3;
        assert.strictEqual(stripMap.length, rowSize, `${label}: strip map width mismatch`);
        for (let row = 0; row < height; row++) {
            const start = row * rowSize;
            assert.deepStrictEqual(
                matrixMap.slice(start, start + rowSize),
                stripMap,
                `${label}: row ${row} mismatch`
            );
        }
    }

    const files = ['audioscanmulti.js', 'audiobandsmatrix.js'];
    for (const file of files)
        assertLayoutDescriptor(file);

    const layoutSequence = ['Matrix', 'Repeated rows', 'Repeated rows', 'Matrix', 'Repeated rows', 'Repeated rows'];
    for (const file of files) {
        for (const [width, height] of [[7, 11], [80, 4]]) {
            const matrixAlgo = loadScript(file, 0x7a11);
            const oracleAlgo = loadScript(file, 0x7a11);
            configure(file, matrixAlgo);
            configure(file, oracleAlgo);
            for (let tick = 0; tick < layoutSequence.length; tick++) {
                const layout = layoutSequence[tick];
                applyTickControls(file, matrixAlgo, tick);
                applyTickControls(file, oracleAlgo, tick);
                matrixAlgo.setMatrixLayout(layout);
                oracleAlgo.setMatrixLayout(layout);
                const frame = layoutFrame(tick);
                const matrixMap = render(matrixAlgo, file, width, height, frame,
                    `${file} ${width}x${height} layout ${layout} tick ${tick}`);
                if (layout === 'Repeated rows') {
                    const stripMap = render(oracleAlgo, file, width, 1, frame,
                        `${file} ${width}x1 strip oracle tick ${tick}`);
                    assertRowsEqualToStrip(matrixMap, stripMap, width, height,
                        `${file} ${width}x${height} repeated rows tick ${tick}`);
                } else {
                    const expected = render(oracleAlgo, file, width, height, frame,
                        `${file} ${width}x${height} matrix oracle tick ${tick}`);
                    assert.deepStrictEqual(matrixMap, expected,
                        `${file} ${width}x${height} matrix layout tick ${tick}`);
                }
            }
        }
        for (const [width, height] of [[1, 1], [1, 11], [11, 1]]) {
            const matrixAlgo = loadScript(file, 0x5e11);
            const rowsAlgo = loadScript(file, 0x5e11);
            configure(file, matrixAlgo);
            configure(file, rowsAlgo);
            for (let tick = 0; tick < 6; tick++) {
                applyTickControls(file, matrixAlgo, tick);
                applyTickControls(file, rowsAlgo, tick);
                matrixAlgo.setMatrixLayout('Matrix');
                rowsAlgo.setMatrixLayout('Repeated rows');
                const frame = layoutFrame(tick);
                const matrixMap = render(matrixAlgo, file, width, height, frame,
                    `${file} strip matrix ${width}x${height} tick ${tick}`);
                const rowsMap = render(rowsAlgo, file, width, height, frame,
                    `${file} strip rows ${width}x${height} tick ${tick}`);
                assert.deepStrictEqual(rowsMap, matrixMap,
                    `${file} strip geometry must ignore layout ${width}x${height} tick ${tick}`);
            }
        }
    }
    for (const axis of ['Horizontal', 'Vertical']) {
        const file = 'audioscanmulti.js';
        const pair = [loadScript(file), loadScript(file)];
        pair.forEach(algo => configure(file, algo));
        for (let tick = 0; tick < 6; tick++) {
            const maps = pair.map((algo, index) => {
                applyTickControls(file, algo, tick);
                algo.setMode('Artistic');
                algo.setAxis(axis);
                algo.setMatrixLayout(index ? 'Repeated rows' : 'Matrix');
                return render(algo, file, 7, 11, layoutFrame(tick), 'Artistic ignores Layout');
            });
            assert.deepStrictEqual(maps[0], maps[1], `Artistic ignores Layout: ${axis} tick ${tick}`);
        }
    }
    console.log('PASS matrix layout choices: descriptors, repeated-row parity, strip invariance and Artistic exclusion');
}

function optionalCliValue(flag) {
    const index = process.argv.indexOf(flag);
    if (index < 0) return null;
    const value = process.argv[index + 1];
    assert(value && !value.startsWith('--'), `${flag} requires a directory path`);
    return value;
}

function assertMatrixResponsePreservation() {
    const layoutDirectory = optionalCliValue('--matrix-layout-before');
    const baselineDirectory = optionalCliValue('--matrix-baseline') || optionalCliValue('--matrix-oracle');
    if (!layoutDirectory && !baselineDirectory) return;
    const files = ['audioscanmulti.js', 'audiobandsmatrix.js'];
    function assertOnlyMatrixLayoutMetadataDelta(file, oracleDirectory, label) {
        const current = loadScript(file);
        const oracle = loadScript(file, 0x5eed, oracleDirectory);
        const parsedCurrent = current.properties.map(parseProperty);
        const layoutDescriptors = parsedCurrent.filter(property => property.name === 'matrixLayout');
        assert.strictEqual(layoutDescriptors.length, 1, `${file} ${label}: missing matrixLayout descriptor`);
        const layout = layoutDescriptors[0];
        assert.strictEqual(layout.type, 'list', `${file} ${label}: matrixLayout type`);
        assert.strictEqual(layout.display, 'Layout', `${file} ${label}: matrixLayout display`);
        assert.strictEqual(layout.values, 'Matrix,Repeated rows', `${file} ${label}: matrixLayout values`);
        assert.strictEqual(current[layout.read](), 'Matrix', `${file} ${label}: matrixLayout default`);
        const stripped = current.properties.filter(descriptor => parseProperty(descriptor).name !== 'matrixLayout');
        assert.strictEqual(JSON.stringify(stripped), JSON.stringify(oracle.properties),
            `${file} ${label}: only matrixLayout descriptor may differ`);
        for (const property of oracle.properties.map(parseProperty))
            assert.strictEqual(JSON.stringify(current[property.read]()), JSON.stringify(oracle[property.read]()),
                `${file} ${label}: default ${property.name}`);
    }

    function compareAgainstOracle(oracleDirectory, label, comparePredicate, layoutChooser) {
        assert.strictEqual(fs.readFileSync(path.join(oracleDirectory, 'hsvutil.js'), 'utf8'), HSV_UTIL,
            `${label}: frozen helper must match current hsvutil.js`);
        for (const file of files)
            assertOnlyMatrixLayoutMetadataDelta(file, oracleDirectory, label);
        let comparisons = 0;
        for (const file of files) {
            const layoutChoices = layoutChooser(file);
            for (const layoutChoice of layoutChoices) {
                for (const [width, height] of [[1, 1], [1, 11], [11, 1], [7, 11], [11, 7], [32, 32], [80, 4]]) {
                    const pair = [loadScript(file), loadScript(file, 0x5eed, oracleDirectory)];
                    for (const axis of file === 'audioscanmulti.js' ? ['Horizontal', 'Vertical'] : ['Horizontal']) {
                        for (const algo of pair) {
                            algo.setMirror('No');
                            if (file === 'audioscanmulti.js') {
                                algo.setAxis(axis);
                                algo.setSpeed(23);
                                algo.setWidth(27);
                                algo.setAttack(0.37);
                                algo.setDecay(0.13);
                                algo.setGradient('Yes');
                            } else {
                                algo.setBandCount(5);
                                algo.setFlipGradient('Yes');
                                algo.setFlipBandOrder('Yes');
                            }
                        }
                        pair[0].setMatrixLayout(layoutChoice);
                        for (let tick = 0; tick < 45; tick++) {
                            const mode = tick < 8 || tick >= 32 ? 'Artistic' : 'LedFx Scan Multi';
                            const seconds = tick % 9 ? 0.2 : 0;
                            const frame = audio({
                                version: 6, low: (tick % 7) / 8, mid: 0.31, high: 0.67,
                                dt: seconds * 2, timing: { deltaSeconds: seconds },
                                sourceEpoch: tick < 24 ? 1 : 2,
                                banks: { full: { count: 6, novelty: [0.1, 0.9, 0.2, 0.7, 0.3, 0.5],
                                    processed: [0.8, 0.1, 0.4, 0.2, 0.9, 0.3] } }
                            });
                            const maps = pair.map(algo => {
                                if (file === 'audioscanmulti.js') {
                                    algo.setMode(mode);
                                    algo.setBounce(tick < 20 ? 'Yes' : 'No');
                                    algo.setSourceMode(tick < 16 ? 'Power' : 'Melbank');
                                    algo.setMelbank(tick < 28 ? 'Processed' : 'Novelty');
                                }
                                return render(algo, file, width, height, frame, `${label} oracle`);
                            });
                            if (comparePredicate(file, width, height, mode)) {
                                assert.deepStrictEqual(
                                    maps[0], maps[1],
                                    `${file} ${label} ${layoutChoice} ${width}x${height} ${axis} ${mode} tick ${tick}`
                                );
                                comparisons++;
                            }
                            if (file === 'audioscanmulti.js' &&
                                (layoutChoice === 'Matrix' || comparePredicate(file, width, height, mode)))
                                assert.strictEqual(JSON.stringify(pair[0].scans), JSON.stringify(pair[1].scans),
                                    `${file} ${label}: source position state changed`);
                        }
                    }
                }
            }
        }
        return comparisons;
    }

    const summaries = [];
    if (layoutDirectory) {
        const matrixComparisons = compareAgainstOracle(
            layoutDirectory,
            'layout-before',
            () => true,
            () => ['Matrix']
        );
        summaries.push(`layout-before=${matrixComparisons}`);
    }
    if (baselineDirectory) {
        const baselineComparisons = compareAgainstOracle(
            baselineDirectory,
            'baseline',
            (file, width, height, mode) =>
                width === 1 || height === 1 || (file === 'audioscanmulti.js' && mode === 'Artistic'),
            () => ['Matrix', 'Repeated rows']
        );
        summaries.push(`baseline=${baselineComparisons}`);
    }
    console.log(`PASS matrix preservation: ${summaries.join(', ')} exact HSV checks; only matrixLayout metadata delta allowed`);
}

if (process.argv.includes('--matrix-layout-only')) {
    assertMatrixResponseGeometry();
    assertMatrixLayoutChoices();
    assertMatrixResponsePreservation();
} else if (process.argv.includes('--reference-effects')) {
    compareReferenceEffects().catch(error => { console.error(error); process.exitCode = 1; });
} else {
assertScriptIdentityBijection();
assertExistingModeManifest();
assert.throws(
    () => assertMapContract(
        'synthetic', new Uint32Array(3), 1, 1, 'wrong type'
    ),
    /Float32Array/
);
assertNonAudioScripts();
assertWaterBandCoverage();

function assertPublishedAudio() {
    for (const [width, height] of [[24, 1], [37, 1], [1, 9], [7, 11]]) {
        const maps = [3, 18].map(peak => {
            const values = Array(24).fill(0);
            values[peak] = 0.8;
            return render(loadScript('audiospectrum.js'), 'audiospectrum.js',
                width, height, audio({
                    version: 6, dt: 1 / 30,
                    timing: { deltaSeconds: 1 / 60 },
                    banks: { full: { count: 24, processed: values,
                        novelty: values.map(x => x * 0.3) } }
                }), 'real full-bank sweep');
        });
        const brightest = map => {
            let peak = 0;
            for (let i = 1; i < map.length / 3; ++i)
                if (map[i * 3 + 2] > map[peak * 3 + 2]) peak = i;
            return peak;
        };
        assert(brightest(maps[1]) > brightest(maps[0]),
            `spectrum ${width}x${height}: must read actual bank bins, not scalar powers`);
        assert(Math.max(...maps[0].filter((_, i) => i % 3 === 2)) > 0.1);
    }
    // The same physical interval, not the same number of JS invocations.
    const powers = [25, 30, 50, 60].map(hz => {
        const algo = loadScript('audioenergy.js');
        let map;
        for (let n = 0; n < hz; ++n)
            map = render(algo, 'audioenergy.js', 1000, 1,
                audio({ low: n < hz / 5 ? 0.8 : 0, dt: 2 / hz,
                    timing: { deltaSeconds: 1 / hz } }), 'physical decay', n);
        return map.filter((v, i) => i % 3 === 2 && v > 0).length;
    });
    assert(Math.max(...powers) - Math.min(...powers) <= 2,
        `energy decay depends on render rate: ${powers}`);

    const particles = [25, 30, 50, 60].map(hz => {
        const algo = loadScript('audiofireworks.js');
        render(algo, 'audiofireworks.js', 24, 1, audio(), 'initialize particles');
        algo.particles.push({ x: 5, y: 2, vx: 0.2, vy: -0.1,
            color: PALETTE[0], life: 100, maxLife: 100 });
        for (let n = 0; n < hz * 0.4; ++n)
            render(algo, 'audiofireworks.js', 24, 1,
                audio({ dt: 2 / hz, timing: { deltaSeconds: 1 / hz } }),
                'physical particle travel');
        return algo.particles[0];
    });
    for (const key of ['x', 'y', 'vy', 'life'])
        assert(Math.max(...particles.map(p => p[key])) - Math.min(...particles.map(p => p[key])) < 1e-8,
            `fireworks ${key} depends on render rate: ${particles.map(p => p[key])}`);

    const burst = count => {
        const algo = loadScript('audiofireworks.js');
        algo.setTriggerMode('Beat');
        algo.setMaxParticles(500);
        render(algo, 'audiofireworks.js', 24, 1, audio({
            beatFired: true, events: { delta: { beat: count, onset: 0, kick: 0, bar: 0 } },
            timing: { deltaSeconds: 0 }
        }), 'counter catch-up');
        return algo.particles.length;
    };
    assert(burst(3) > burst(1) * 2, 'fireworks drops coalesced beat events');

    for (const [file, state] of [['audiopuddles.js', 'ripples'],
        ['audiobarcode.js', 'lines'], ['audioshockwave.js', 'waves']]) {
        const algo = loadScript(file);
        if (algo.setMinSpawnMs) algo.setMinSpawnMs(0);
        if (algo.setTrigger) algo.setTrigger('Onset');
        const frame = audio({ version: 6, onset: true, onsetIntensity: 0.8,
            events: { delta: { onset: 3, beat: 0, kick: 0, bar: 0 } },
            timing: { deltaSeconds: 0 } });
        render(algo, file, 7, 11, frame, 'three coalesced events');
        assert.strictEqual(algo[state].length, 3, `${file}: coalesced onset count`);
        frame.events.delta.onset = 0;
        render(algo, file, 7, 11, frame, 'repeat publication');
        assert.strictEqual(algo[state].length, 3, `${file}: replayed an old event`);
    }
    const strobe = loadScript('audiostrobe.js');
    strobe.setBassTrigger('Kick');
    const kicked = render(strobe, 'audiostrobe.js', 7, 1,
        audio({ version: 6, dt: 1, kickFired: true,
            timing: { deltaSeconds: 0.5 } }), 'kick without tempo');
    assert(kicked.some((value, i) => i % 3 === 2 && value > 0),
        'strobe Kick option must not require a tempo beat');

    const shotBrightness = [25, 30, 50, 60].map(hz => {
        const algo = loadScript('audioshot.js');
        render(algo, 'audioshot.js', 24, 9, audio({
            onset: true, onsetIntensity: 0.8, timing: { deltaSeconds: 0 },
            events: { delta: { onset: 1, beat: 0, kick: 0, bar: 0 } }
        }), 'initial shot');
        let map;
        for (let n = 0; n < hz * 0.4; ++n)
            map = render(algo, 'audioshot.js', 24, 9, audio({
                timing: { deltaSeconds: 1 / hz }, dt: 2 / hz,
                events: { delta: { onset: 0, beat: 0, kick: 0, bar: 0 } }
            }), 'physical shot decay');
        return Math.max(...map.filter((_, i) => i % 3 === 2));
    });
    assert(Math.max(...shotBrightness) - Math.min(...shotBrightness) < 1e-6,
        `shot decay depends on render rate: ${shotBrightness}`);
    console.log('AUDIO_API6: real-bank sweep=4 layouts; physical decay/travel=25/30/50/60Hz; ' +
        'coalesced onsets=3 consumers; burst catch-up=3 beats; kick independent of tempo');
}

assertPublishedAudio();
assertReferenceCadence();
assertMeltSpeedZeroStability();
assertMeltSparkleBehavior();
assertPitchSpectrumUsesPitch();
assertWaterfallHistory();
assertDigitalRainMotion();
assertRainPulseSelection();
assertMatrixResponseGeometry();
assertMatrixLayoutChoices();
assertMatrixResponsePreservation();
assertScrollPlusFractionalShift();
assertSharedHueHelperBoundaries();
assertWorkerMCrawlerRows();
assertPitchNontrivialComposition();
assertWorkerLPitchTimingRows();
assertWorkerGRegressions();
assertWorkerJPlasmaWledBounds();
assertWorkerHRegressionsRows();
assertWorkerDIdentityRows();
assertWorkerAOutputRows();
assertWorkerARegressionRows();
assertWorkerBOutputRows();
assertWorkerCOutputRows();
assertWorkerDOutputRows();
assertWorkerIRegressions20260916();
assertWorkerEOutputRows();
assertWorkerERegressionRows();
assertWorkerASpectralRows();
assertWorkerBHistoryRows();
assertWorkerCSimulationRows();
assertWorkerDInputRows();
assertWorkerEBeatRows();
assertWorkerGReferenceRows();
assertWorkerHAtmosphereRows();
assertWorkerIScanRows();
assertWorkerJMatrixRows();

let passed = 0;
for (const scriptFile of EXPECTED_SCRIPTS) {
    try {
        const result = assertScript(scriptFile);
        passed++;
        console.log(
            `PASS ${scriptFile} hook=${result.activeStimulus} ` +
            `contrast=${result.contrast.toFixed(3)} peak=${result.peak.toFixed(3)}`
        );
    } catch (error) {
        console.error(`FAIL ${scriptFile}: ${error.message}`);
        process.exitCode = 1;
    }
}
validatePinnedManifest(PINNED_REGISTRATION_MANIFEST, executedCases);
const brokenManifest = PINNED_REGISTRATION_MANIFEST.map(entry =>
    entry.id === 'bands' ? Object.assign({}, entry, { caseIds: [] }) : entry
);
assert.throws(
    () => validatePinnedManifest(brokenManifest, executedCases),
    /missing executable case ids/
);
markCase('manifest-coverage');
assertExecutedCaseCoverage(REQUIRED_EXECUTED_CASE_IDS, executedCases);
const withoutOne = new Set(executedCases);
withoutOne.delete(REQUIRED_EXECUTED_CASE_IDS[0]);
assert.throws(
    () => assertExecutedCaseCoverage(REQUIRED_EXECUTED_CASE_IDS, withoutOne),
    /missing executed behavior cases/
);
console.log(
    `RESULTS: ${passed} passed, ${EXPECTED_SCRIPTS.length - passed} failed, ` +
    `${EXPECTED_SCRIPTS.length} total`
);
console.log(
    `COVERAGE: contract=${totals.contractCases} parameters=${totals.parameterWrites} ` +
    `responses=${totals.responseChecks} contrast=${totals.contrastChecks} ` +
    `deterministic=${totals.deterministicChecks} resize=${totals.resizeChecks} ` +
    `waterBands=${totals.waterBandCases}`
);
}
