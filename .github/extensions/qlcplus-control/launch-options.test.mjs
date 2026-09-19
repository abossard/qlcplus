import test from "node:test";
import assert from "node:assert/strict";
import { timingLaunchOptions } from "./launch-options.mjs";

const cases = [
    { name: "default off overrides inherited enable", input: {}, previous: {}, enabled: false, interval: 1000 },
    { name: "custom enabled launch", input: { timingDiagnostics: true, timingIntervalMs: 2500 }, previous: {}, enabled: true, interval: 2500 },
    { name: "restart retains settings", input: {}, previous: { timingDiagnostics: true, timingIntervalMs: 750 }, enabled: true, interval: 750 },
    { name: "explicit off retains previous interval", input: { timingDiagnostics: false }, previous: { timingDiagnostics: true, timingIntervalMs: 2000 }, enabled: false, interval: 2000 },
    { name: "override interval on restart", input: { timingIntervalMs: 1500 }, previous: { timingDiagnostics: true, timingIntervalMs: 2000 }, enabled: true, interval: 1500 },
    { name: "legacy unknown run", input: {}, previous: { timingDiagnostics: null, timingIntervalMs: null }, enabled: false, interval: 1000 },
];
const parentEnv = Object.freeze({
    QLCPLUS_TIMING_DIAG: "1",
    QLCPLUS_TIMING_DIAG_MS: "5000",
    KEEP: "unchanged",
});

for (const row of cases) {
    test(row.name, () => {
        const result = timingLaunchOptions(Object.freeze(row.input), Object.freeze(row.previous), parentEnv);
        assert.equal(result.timingDiagnostics, row.enabled);
        assert.equal(result.timingIntervalMs, row.interval);
        assert.deepEqual(result.env, {
            QLCPLUS_TIMING_DIAG: row.enabled ? "1" : "0",
            QLCPLUS_TIMING_DIAG_MS: String(row.interval),
            KEEP: "unchanged",
        });
    });
}

for (const value of ["false", 0, null]) {
    test(`reject non-boolean toggle ${JSON.stringify(value)}`, () => {
        assert.throws(() => timingLaunchOptions({ timingDiagnostics: value }), /timingDiagnostics/);
    });
}

for (const value of [0, -1, 0.5, "1000", null, Number.MAX_SAFE_INTEGER + 1]) {
    test(`reject invalid interval ${JSON.stringify(value)}`, () => {
        assert.throws(() => timingLaunchOptions({ timingIntervalMs: value }), /timingIntervalMs/);
    });
}
