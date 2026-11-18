'use strict';

const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

test('planner imports its exports and preserves notes on invalid imports', async () => {
    const elements = new Map();
    const listeners = {};
    const storage = new Map();
    const document = {
        getElementById(id) {
            if (!elements.has(id)) elements.set(id, { addEventListener() {} });
            return elements.get(id);
        },
        addEventListener(type, fn) { listeners[type] = fn; },
        querySelector() { return null; },
    };
    const ctx = vm.createContext({
        window: {}, document, HTMLDialogElement: class {}, setTimeout: fn => fn(),
        localStorage: { getItem: key => storage.get(key), setItem: (key, value) => storage.set(key, value) },
    });
    const plan = path.join(__dirname, '../bossilighty/plan');
    vm.runInContext(readFileSync(path.join(plan, 'plan-data.js'), 'utf8'), ctx);
    const html = readFileSync(path.join(plan, 'index.html'), 'utf8');
    vm.runInContext(html.match(/<script>([\s\S]*?)<\/script>/)[1], ctx);
    const doImport = async data => {
        listeners.change({ target: { id: 'file', files: [{ text: async () => JSON.stringify(data) }] } });
        await new Promise(resolve => setImmediate(resolve));
        return elements.get('live').textContent;
    };
    const key = 'bossilighty-plan-v1';
    const current = JSON.parse(vm.runInContext('JSON.stringify(exportData())', ctx));
    current.state.notes['f-programmer'] = 'Keep this note';
    assert.equal(await doImport(current), 'Priorities imported');
    const saved = storage.get(key);
    for (const bad of [{}, { ...current, version: 2 }, { ...current, state: null },
        { ...current, state: { ...current.state, rank: {} } },
        { ...current, state: { ...current.state, view: 'unknown' } }]) {
        assert.equal(await doImport(bad), 'That file is not a priorities export');
        assert.equal(storage.get(key), saved);
        assert.equal(vm.runInContext("S.notes['f-programmer']", ctx), 'Keep this note');
    }
    current.state.notes['f-programmer'] = 'Imported raw state';
    assert.equal(await doImport(current.state), 'Priorities imported');
    assert.deepEqual(JSON.parse(storage.get(key)), current.state);
});
