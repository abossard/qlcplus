#!/usr/bin/env node
'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const html = fs.readFileSync(path.join(__dirname, '..', 'webaccess/res/keypad.html'), 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const refresh = 'QLC+API|getChannelsValues|1|1|512';
const valid = [
    ['K01', '13 AT 148', [13], [148]],
    ['K02', '13A148', [13], [148]],
    ['K03', ' 3 thru 7 by 2 AT 0 THRU 100 ENTER ', [3, 5, 7], [0, 50, 100]],
    ['K04', '3T7 BY2 A0T100', [3, 5, 7], [0, 50, 100]],
    ['K05', '3 THRU 5 FULL', [3, 4, 5], [255, 255, 255]],
    ['K06', '3 THRU 5 AT FULL', [3, 4, 5], [255, 255, 255]],
    ['K07', '3T5 ZERO', [3, 4, 5], [0, 0, 0]],
    ['K08', '3 THRU 5 AT ZERO', [3, 4, 5], [0, 0, 0]],
    ['K09', '1 THRU 4 AT 0 THRU 100', [1, 2, 3, 4], [0, 33, 67, 100]],
    ['K10', '1T3 A200T20', [1, 2, 3], [200, 110, 20]],
    ['K11', '1 THRU 4 + 20', [1, 2, 3, 4], [30, 120, 255, 255]],
    ['K12', '1 THRU 4 - 60', [1, 2, 3, 4], [0, 40, 180, 195]],
    ['K13', '1 THRU 4 +% 20', [1, 2, 3, 4], [12, 120, 255, 255]],
    ['K14', '1 THRU 4 -% 40', [1, 2, 3, 4], [6, 60, 144, 153]]
];
const invalid = [
    ['K16', 'garbage'],
    ['K17', '2x AT 100'],
    ['K18', '1 THRU 4 BY 0 AT 100'],
    ['K19', '1 THRU 4 BY -1 AT 100'],
    ['K20', '0 AT 100'],
    ['K21', '513 AT 100'],
    ['K22', '4 THRU 2 AT 100'],
    ['K23', '1 THRU frog AT 100']
];

function page() {
    const sends = [], alerts = [], timeouts = [];
    const field = { value: '' };
    const context = vm.createContext({
        window: { location: { host: 'offline.invalid' } },
        document: { getElementById(id) { assert.strictEqual(id, 'commInput'); return field; } },
        WebSocket: function () { this.send = message => sends.push(message); },
        setTimeout: (callback, delay) => timeouts.push({ callback, delay }),
        alert: message => alerts.push(message),
        console: { log() {}, error() {} }
    });
    const run = code => vm.runInContext(code, context, { timeout: 250 });
    run(script);
    run('websocket.onopen(); values = [10, 100, 240, 255];');
    return { context, sends, alerts, timeouts, field, run,
        command: input => run(`chans_vals(${JSON.stringify(input)})`) };
}

function expectWrites(p, channels, values) {
    for (const [i, channel] of channels.entries()) {
        assert(Number.isInteger(channel) && channel >= 1 && channel <= 512);
        assert(Number.isInteger(values[i]) && values[i] >= 0 && values[i] <= 255);
    }
    assert.deepStrictEqual(p.sends, [
        ...channels.map((channel, i) => `CH|${channel}|${values[i]}`), refresh
    ]);
    assert.deepStrictEqual(Array.from(p.context.lastChannels), channels);
    assert.deepStrictEqual(p.alerts, []);
    assert.deepStrictEqual(p.timeouts, []);
}

let passed = 0, failed = 0;
function check(name, test) {
    try {
        test();
        passed++;
        console.log(`PASS ${name}`);
    } catch (error) {
        failed++;
        console.error(`FAIL ${name}: ${error.message}`);
    }
}

for (const [id, input, channels, values] of valid)
    check(id, () => {
        const p = page();
        p.command(input);
        expectWrites(p, channels, values);
    });

check('K15 selection reuse', () => {
    const p = page();
    for (const [input, value] of [
        ['6 THRU 10 BY 2 AT 99', 99], ['AT ZERO', 0], ['AT FULL', 255], ['AT 123', 123]
    ]) {
        p.sends.length = 0;
        p.command(input);
        expectWrites(p, [6, 8, 10], [value, value, value]);
    }
});

for (const [id, input] of invalid)
    check(id, () => {
        const p = page();
        p.command('6 THRU 10 BY 2 AT 99');
        p.sends.length = 0;
        p.command(input);
        assert.deepStrictEqual(p.sends, []);
        assert.deepStrictEqual(Array.from(p.context.lastChannels), [6, 8, 10]);
    });

check('K24 buttons and keyboard', () => {
    const p = page();
    p.run("['2', '5', 'AT', 'FULL'].forEach(composeCommand)");
    assert.strictEqual(p.field.value, '25 AT FULL ');
    p.run("commandKeyDown({ key: 'Enter' })");
    expectWrites(p, [25], [255]);
    assert.strictEqual(p.field.value, '');
});

check('K25 disconnected composition', () => {
    const p = page();
    p.field.value = '13';
    p.run("isConnected = false; composeCommand('AT')");
    assert.strictEqual(p.field.value, '13');
    assert.deepStrictEqual(p.alerts, ['You must connect to QLC+ WebSocket first!']);
    assert.deepStrictEqual(p.sends, []);
});

check('channel 512 and no previous selection', () => {
    const p = page();
    p.command('AT FULL');
    assert.deepStrictEqual(p.sends, []);
    p.command('512 AT 255');
    expectWrites(p, [512], [255]);
});

console.log(`Totals: ${passed} passed, ${failed} failed`);
process.exitCode = failed ? 1 : 0;
