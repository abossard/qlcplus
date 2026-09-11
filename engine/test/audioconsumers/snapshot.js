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
        if (audio.version !== 6 || audio.banks.full.count !== 5 ||
            audio.sourceId !== "test" || audio.profileId !== 7 ||
            algo.displayWidth !== 11 || algo.displayHeight !== 9)
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
