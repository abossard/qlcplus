var testAlgo;
(function () {
    var algo = {};
    algo.name = "Snapshot boundary";
    algo.apiVersion = 3;
    algo.usesAudio = true;
    algo.acceptColors = 0;
    algo.properties = [];
    algo.rgbMapSetColors = function () {};
    algo.rgbMapGetColors = function () { return []; };
    algo.rgbMapStepCount = function () { return 1; };
    algo.rgbMap = function (w, h, color, step, audio) {
        var raw = audio.powers && audio.powers.raw ? audio.powers.raw : {};
        var pitch = audio.pitch || {};
        var hasRaw = Math.abs((raw.beat || 0) - 0.17) < 1e-12 &&
            Math.abs((raw.bass || 0) - 0.53) < 1e-12 &&
            Math.abs((raw.low || 0) - 0.35) < 1e-12 &&
            Math.abs((raw.mid || 0) - 0.41) < 1e-12 &&
            Math.abs((raw.high || 0) - 0.29) < 1e-12;
        var hasPitch = !!pitch.valid && Math.abs((pitch.hz || 0) - 220) < 1e-12 &&
            Math.abs((pitch.midi || 0) - 57) < 1e-9 &&
            Math.abs((pitch.confidence || 0) - 0.64) < 1e-12;
        if (audio.version !== 6 || audio.banks.full.count !== 5 ||
            audio.sourceId !== "test" || audio.profileId !== 7 ||
            algo.displayWidth !== 11 || algo.displayHeight !== 9 ||
            !hasRaw || !hasPitch)
            throw new Error("wrong audio publication");
        return new Float32Array([
            0, 1, audio.low,
            1 / 3, 1, audio.banks.full.processed[2],
            2 / 3, 1, audio.events.delta.beat / 10,
            0, 0, audio.timing.deltaSeconds
        ]);
    };
    testAlgo = algo;
    return algo;
})();
