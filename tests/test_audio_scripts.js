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
const EXPECTED_SCRIPTS = [
    'audioaurora.js', 'audiobarcode.js', 'audiobasslaser.js',
    'audiobeatcolors.js', 'audioblocks.js', 'audioblurz.js',
    'audiobuildup.js', 'audiocellular.js', 'audiochaser.js',
    'audiocrawler.js', 'audiodjlight.js', 'audioenergy.js',
    'audioenergy2.js', 'audioequalizer.js', 'audiofire.js',
    'audiofireworks.js', 'audioflowfield.js', 'audioglitch.js',
    'audioglitch2.js', 'audiogravimeter.js', 'audiohueshift.js',
    'audiolava.js', 'audiomelt.js', 'audiomeltsparkle.js',
    'audioplasma.js', 'audiopower.js', 'audiopuddles.js',
    'audioreaction.js', 'audioreactor.js', 'audioscan.js',
    'audioscanflare.js', 'audioscanmulti.js', 'audioshockwave.js',
    'audioshot.js', 'audiosoap.js', 'audiospectrum.js',
    'audiosplittower.js', 'audiostrobe.js', 'audiotunnel.js',
    'audiovortex.js', 'audiowater.js'
];
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
    bpm180: { low: 0.52, mid: 0.31, high: 0.17, bpm: 180, dt: 0.06 }
});
const EVENT_STIMULUS = Object.freeze({
    'audiobarcode.js': 'onset',
    'audiobeatcolors.js': 'downbeat',
    'audioblurz.js': 'onset',
    'audiobuildup.js': 'beat',
    'audiofireworks.js': 'beat',
    'audioglitch.js': 'onset',
    'audiopuddles.js': 'onset',
    'audioshockwave.js': 'onset',
    'audioshot.js': 'onset',
    'audiostrobe.js': 'onset'
});
const UNIFORM_BY_DESIGN = new Set([
    'audiobeatcolors.js',
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

function loadScript(scriptFile, seed = 0x5eed, directory = SCRIPTS_DIR) {
    const math = Object.create(Math);
    Object.defineProperty(math, 'random', { value: seededRandom(seed) });
    const sandbox = {
        Math: math,
        Date,
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
    assert.strictEqual(algo.usesAudio, true, `${scriptFile}: usesAudio`);
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
            }
            algo[property.write](initial);
            const actual = algo[property.read]();
            if (property.type === 'float') {
                assert.strictEqual(
                    Number(actual),
                    Number(initial),
                    `${scriptFile} ${property.name}: default round-trip`
                );
            } else {
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
}

if (process.argv.includes('--reference-effects')) {
    compareReferenceEffects().catch(error => { console.error(error); process.exitCode = 1; });
} else {
const installed = fs.readdirSync(SCRIPTS_DIR)
    .filter(file => /^audio.*\.js$/.test(file))
    .sort();
assert.deepStrictEqual(
    installed,
    EXPECTED_SCRIPTS,
    'installed audio script inventory differs from the explicit 41-script contract'
);
assert.throws(
    () => assertMapContract(
        'synthetic', new Uint32Array(3), 1, 1, 'wrong type'
    ),
    /Float32Array/
);
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
