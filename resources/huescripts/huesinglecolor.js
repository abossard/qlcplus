/*
  Q Light Controller Plus
  huesinglecolor.js

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
    algo.name = "Hue Single Color";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 1;
    algo.usesAudio = false;
    algo.properties = new Array();

    algo.presetModulation = "Off";
    algo.presetModulationSpeed = 0.25;
    algo.presetTemporalSpeed = 1.0;
    algo.phase = 0;
    algo.lastTimeMs = null;

    algo.properties.push(
      "name:presetModulation|type:list|display:Modulation|values:Off,Sine,Breath|write:setModulation|read:getModulation");
    algo.properties.push(
      "name:presetModulationSpeed|type:float|display:Modulation Speed|values:0.01,1|write:setModulationSpeed|read:getModulationSpeed");
    algo.properties.push(
      "name:presetTemporalSpeed|type:float|display:Temporal Speed|values:0.1,10|write:setTemporalSpeed|read:getTemporalSpeed");

    algo.setModulation = function(v) {
      algo.presetModulation = (v === "Sine" || v === "Breath") ? v : "Off";
    };
    algo.getModulation = function() { return algo.presetModulation; };
    algo.setModulationSpeed = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0.25;
      algo.presetModulationSpeed = Math.max(0.01, Math.min(1, value));
    };
    algo.getModulationSpeed = function() { return algo.presetModulationSpeed; };
    algo.setTemporalSpeed = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 1.0;
      algo.presetTemporalSpeed = Math.max(0.1, Math.min(10, value));
    };
    algo.getTemporalSpeed = function() { return algo.presetTemporalSpeed; };

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

    function baseColor() {
      if (algo.colors && algo.colors.length > 0) return algo.colors[0];
      return {h: 0, s: 0, v: 1};
    }

    algo.rgbMapStepCount = function(width, height) { return algo.presetModulation === "Off" ? 1 : 2; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step)
    {
      var map = HSVUtil.createMap(width, height);
      var seconds = elapsedSeconds();
      if (seconds > 0) {
        var rate = 0.1 * algo.presetTemporalSpeed * (0.4 + 1.6 * algo.presetModulationSpeed);
        algo.phase = HSVUtil.mod1(algo.phase + seconds * rate);
      }

      var color = baseColor();
      var n = width * height;
      var breathWave = 0.5 - 0.5 * Math.cos(2 * Math.PI * algo.phase);
      var litCells = Math.floor(0.2 * n + 0.8 * n * breathWave);
      for (var i = 0; i < n; i++) {
        var value = color.v;
        if (algo.presetModulation === "Sine") {
          var u = n <= 1 ? 0 : i / (n - 1);
          value *= 0.4 + 0.3 * Math.max(0, Math.sin(Math.PI * HSVUtil.mod1(u + algo.phase)));
        } else if (algo.presetModulation === "Breath") {
          value *= i < litCells ? 1.0 : 0.0;
        }
        map[i * 3] = color.h;
        map[i * 3 + 1] = color.s;
        map[i * 3 + 2] = HSVUtil.clamp01(value);
      }
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
