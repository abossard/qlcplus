#!/usr/bin/env node
/*
  Test harness for QLC+ audio-reactive RGB scripts.
  Loads ledfx_compat.js shim + each audio script, then calls rgbMap()
  with simulated audio data to verify they don't crash.
*/

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const SCRIPTS_DIR = path.join(__dirname, '..', 'resources', 'rgbscripts');
const WIDTH = 16;
const HEIGHT = 8;
const FRAMES = 10;

// Simulated audio data with varying spectrum
function makeAudioData(frame) {
    const spectrum = [];
    for (let i = 0; i < 32; i++) {
        // Sine-based fake spectrum that varies per frame
        spectrum.push(Math.abs(Math.sin(i * 0.3 + frame * 0.5)) * 0.8);
    }
    return {
        spectrum: spectrum,
        volume: 0.5 + Math.sin(frame * 0.3) * 0.3,
        beat: (frame % 4 === 0),
        bpm: 120,
        maxMagnitude: 10.0
    };
}

// Load the shim
const shimPath = path.join(SCRIPTS_DIR, 'ledfx_compat.js');
const shimCode = fs.readFileSync(shimPath, 'utf8');

// Find all audio*.js files
const audioScripts = fs.readdirSync(SCRIPTS_DIR)
    .filter(f => f.startsWith('audio') && f.endsWith('.js'))
    .sort();

let passed = 0;
let failed = 0;
const results = [];

for (const scriptFile of audioScripts) {
    const scriptPath = path.join(SCRIPTS_DIR, scriptFile);
    const scriptCode = fs.readFileSync(scriptPath, 'utf8');

    let status = 'PASS';
    let error = null;
    let details = [];

    try {
        // Create isolated sandbox with Date, Math, console
        const sandbox = {
            Date: Date,
            Math: Math,
            console: { log: () => {}, warn: () => {}, error: () => {} },
            Array: Array,
            testAlgo: null
        };
        vm.createContext(sandbox);

        // Load shim first
        vm.runInContext(shimCode, sandbox, { filename: 'ledfx_compat.js' });

        // Verify LedFx is available
        if (!sandbox.LedFx) {
            throw new Error('LedFx shim not loaded into context');
        }

        // Load script
        vm.runInContext(scriptCode, sandbox, { filename: scriptFile });

        const algo = sandbox.testAlgo;
        if (!algo) {
            throw new Error('testAlgo not set — script IIFE failed');
        }

        // Check required properties
        if (!algo.name) throw new Error('Missing algo.name');
        if (!algo.usesAudio) throw new Error('Missing algo.usesAudio');
        if (algo.apiVersion !== 3) throw new Error('apiVersion is not 3: ' + algo.apiVersion);
        if (typeof algo.rgbMap !== 'function') throw new Error('Missing rgbMap function');
        if (typeof algo.rgbMapStepCount !== 'function') throw new Error('Missing rgbMapStepCount');
        if (typeof algo.rgbMapSetColors !== 'function') throw new Error('Missing rgbMapSetColors');
        if (typeof algo.rgbMapGetColors !== 'function') throw new Error('Missing rgbMapGetColors');

        details.push(`name="${algo.name}", acceptColors=${algo.acceptColors}`);

        // Test stepCount
        const steps = algo.rgbMapStepCount(WIDTH, HEIGHT);
        if (steps !== 1) details.push(`WARNING: stepCount=${steps} (expected 1)`);

        // Test color set/get
        algo.rgbMapSetColors([0xFF0000, 0x00FF00, 0x0000FF]);
        const colors = algo.rgbMapGetColors();
        if (!Array.isArray(colors)) throw new Error('rgbMapGetColors did not return array');

        // Test properties
        const propCount = algo.properties ? algo.properties.length : 0;
        details.push(`${propCount} properties`);

        // Run multiple frames with audio data
        let nonBlackPixels = 0;
        for (let frame = 0; frame < FRAMES; frame++) {
            const audio = makeAudioData(frame);
            const map = algo.rgbMap(WIDTH, HEIGHT, 0xFF0000, 0, audio);

            if (!Array.isArray(map)) {
                throw new Error(`Frame ${frame}: rgbMap returned non-array: ${typeof map}`);
            }
            if (map.length !== HEIGHT) {
                throw new Error(`Frame ${frame}: map height ${map.length} !== ${HEIGHT}`);
            }
            for (let y = 0; y < map.length; y++) {
                if (!Array.isArray(map[y])) {
                    throw new Error(`Frame ${frame}: row ${y} is not array`);
                }
                if (map[y].length !== WIDTH) {
                    throw new Error(`Frame ${frame}: row ${y} width ${map[y].length} !== ${WIDTH}`);
                }
                for (let x = 0; x < map[y].length; x++) {
                    const val = map[y][x];
                    if (typeof val !== 'number' || isNaN(val)) {
                        throw new Error(`Frame ${frame}: pixel [${y}][${x}] = ${val} (not a number)`);
                    }
                    if (val !== 0) nonBlackPixels++;
                }
            }
        }

        const totalPixels = WIDTH * HEIGHT * FRAMES;
        const pctActive = ((nonBlackPixels / totalPixels) * 100).toFixed(1);
        details.push(`${pctActive}% non-black pixels across ${FRAMES} frames`);

        if (nonBlackPixels === 0) {
            details.push('WARNING: all pixels black — effect may not be rendering');
        }

        // Test with empty audio (should not crash)
        const emptyMap = algo.rgbMap(WIDTH, HEIGHT, 0xFF0000, 0, { spectrum: [], volume: 0, beat: false, bpm: 120 });
        if (!Array.isArray(emptyMap)) {
            throw new Error('Crashed with empty audio data');
        }

        // Test with null audio (should not crash)
        const nullMap = algo.rgbMap(WIDTH, HEIGHT, 0xFF0000, 0, null);
        if (!Array.isArray(nullMap)) {
            throw new Error('Crashed with null audio data');
        }

    } catch (e) {
        status = 'FAIL';
        error = e.message;
        failed++;
    }

    if (status === 'PASS') passed++;

    const icon = status === 'PASS' ? '✅' : '❌';
    results.push({ file: scriptFile, status, error, details });
    console.log(`${icon} ${scriptFile}: ${status}${error ? ' — ' + error : ''}`);
    if (details.length > 0) {
        console.log(`   ${details.join(', ')}`);
    }
}

console.log(`\n${'='.repeat(60)}`);
console.log(`RESULTS: ${passed} passed, ${failed} failed, ${audioScripts.length} total`);
console.log(`${'='.repeat(60)}`);

process.exit(failed > 0 ? 1 : 0);
