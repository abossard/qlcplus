/*
  Q Light Controller Plus
  huepixels.js

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

var testAlgo;

(
  function () {
    var algo = new Object;
    algo.apiVersion = 3;
    algo.name = "Hue Pixels";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 5;
    algo.usesAudio = false;
    algo.properties = new Array();

    algo.presetPeriod = 0.35;
    algo.presetChunk = 4;
    algo.presetBuildUp = "On";
    algo.lastTimeMs = null;
    algo.state = null;

    algo.properties.push(
      "name:presetPeriod|type:float|display:Period (s)|values:0.01,5|write:setPeriod|read:getPeriod");
    algo.properties.push(
      "name:presetChunk|type:range|display:Chunk|values:1,32|write:setChunk|read:getChunk");
    algo.properties.push(
      "name:presetBuildUp|type:list|display:Build Up|values:Off,On|write:setBuildUp|read:getBuildUp");

    algo.setPeriod = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0.35;
      algo.presetPeriod = Math.max(0.01, Math.min(5, value));
    };
    algo.getPeriod = function() { return algo.presetPeriod; };
    algo.setChunk = function(v) {
      var value = parseInt(v);
      if (!isFinite(value)) value = 4;
      algo.presetChunk = Math.max(1, Math.min(32, value));
    };
    algo.getChunk = function() { return algo.presetChunk; };
    algo.setBuildUp = function(v) { algo.presetBuildUp = v === "Off" ? "Off" : "On"; };
    algo.getBuildUp = function() { return algo.presetBuildUp; };

    function elapsedSeconds() {
      var now = Date.now();
      if (algo.lastTimeMs === null) {
        algo.lastTimeMs = now;
        return 0;
      }
      var seconds = Math.max(0, (now - algo.lastTimeMs) / 1000);
      algo.lastTimeMs = now;
      return seconds;
    }

    function clearCanvas() {
      for (var i = 0; i < algo.state.canvas.length; i++)
        algo.state.canvas[i] = 0;
    }

    function currentColor() {
      var colors = (algo.colors && algo.colors.length) ? algo.colors : [{h: 0, s: 0, v: 1}];
      return colors[algo.state.colorIndex % colors.length];
    }

    function applyChunk(n) {
      if (algo.state.cursor >= n) {
        algo.state.cursor = 0;
        algo.state.colorIndex++;
        if (algo.presetBuildUp === "On") clearCanvas();
      }
      if (algo.presetBuildUp === "Off") clearCanvas();

      var c = currentColor();
      var end = Math.min(n, algo.state.cursor + algo.presetChunk);
      for (var i = algo.state.cursor; i < end; i++) {
        algo.state.canvas[i * 3] = c.h;
        algo.state.canvas[i * 3 + 1] = c.s;
        algo.state.canvas[i * 3 + 2] = c.v;
      }
      algo.state.cursor = end;
    }

    function ensureState(n) {
      if (!algo.state || algo.state.n !== n) {
        algo.state = {
          n: n,
          cursor: 0,
          colorIndex: 0,
          accumulator: 0,
          canvas: HSVUtil.createMap(n, 1)
        };
      }
    }

    algo.rgbMapStepCount = function(width, height) { return 2; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step)
    {
      var n = width * height;
      var map = HSVUtil.createMap(width, height);
      ensureState(n);
      var seconds = elapsedSeconds();
      algo.state.accumulator += seconds;

      if (algo.state.accumulator >= algo.presetPeriod) {
        algo.state.accumulator = algo.state.accumulator % algo.presetPeriod;
        applyChunk(n);
      }
      for (var i = 0; i < map.length; i++) map[i] = algo.state.canvas[i];
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
