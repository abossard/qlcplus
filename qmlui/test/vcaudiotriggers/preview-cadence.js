var testAlgo;
(function () {
    var calls = 0, algo = {};
    algo.name = "Editor audio cadence";
    algo.apiVersion = 3;
    algo.usesAudio = true;
    algo.acceptColors = 0;
    algo.properties = ["name:calls|type:string|display:Calls|write:setCalls|read:getCalls"];
    algo.setCalls = function () {};
    algo.getCalls = function () { return String(calls); };
    algo.rgbMapSetColors = function () {};
    algo.rgbMapGetColors = function () { return []; };
    algo.rgbMapStepCount = function () { return 4; };
    algo.rgbMap = function (w, h, color, step, audio) {
        calls++;
        return new Float32Array([0, 1, audio.low, 1 / 3, 1, audio.mid, 2 / 3, 1, audio.high]);
    };
    testAlgo = algo;
    return algo;
})();
