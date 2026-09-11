var testAlgo;
(function () {
    var snapshot = {}, algo = {};
    algo.name = "Native consumer view";
    algo.apiVersion = 3;
    algo.usesAudio = true;
    algo.acceptColors = 0;
    algo.properties = ["name:snapshot|type:string|display:Snapshot|write:setSnapshot|read:getSnapshot"];
    algo.setSnapshot = function () {};
    algo.getSnapshot = function () { return JSON.stringify(snapshot); };
    algo.rgbMapSetColors = function () {};
    algo.rgbMapGetColors = function () { return []; };
    algo.rgbMapStepCount = function () { return 1; };
    algo.rgbMap = function (w, h, color, step, audio) {
        snapshot = audio;
        return new Float32Array([0, 1, audio.low, 1 / 3, 1, audio.mid, 2 / 3, 1, audio.high]);
    };
    testAlgo = algo;
    return algo;
})();
