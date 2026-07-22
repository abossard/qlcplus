#!/usr/bin/env node
/*
  Boundary harness for the installed QLC+ audio-reactive RGB scripts.
  It mirrors the current hsvutil.js, flat audio object, palette, and
  Float32Array HSV contracts used by RGBScript.
*/

'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const SCRIPTS_DIR = path.join(__dirname, '..', 'resources', 'rgbscripts');
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

function loadScript(scriptFile, seed = 0x5eed) {
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
        fs.readFileSync(path.join(SCRIPTS_DIR, scriptFile), 'utf8'),
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
