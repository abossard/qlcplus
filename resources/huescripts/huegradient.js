/*
  Q Light Controller Plus
  huegradient.js

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
    algo.name = "Hue Gradient";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 5;
    algo.usesAudio = false;
    algo.properties = new Array();

    algo.presetRollSpeed = 0.25;
    algo.presetTemporalSpeed = 1;
    algo.presetModulation = "Off";
    algo.presetModulationSpeed = 0.2;
    algo.presetPositions = "";
    algo.presetFlip = "Off";
    algo.presetMirror = "Off";
    algo.presetBrightness = 1.0;
    algo.presetBlur = 0;
    algo.presetBackgroundMode = "Off";
    algo.presetBackgroundColor = "#000000";
    algo.presetBackgroundBrightness = 1.0;
    algo.phase = 0;
    algo.roll = 0;
    algo.lastTimeMs = null;

    algo.properties.push(
      "name:presetRollSpeed|type:float|display:Roll Speed|values:0,1|write:setRollSpeed|read:getRollSpeed");
    algo.properties.push(
      "name:presetTemporalSpeed|type:float|display:Temporal Speed|values:0.1,10|write:setTemporalSpeed|read:getTemporalSpeed");
    algo.properties.push(
      "name:presetModulation|type:list|display:Modulation|values:Off,Sine,Breath|write:setModulation|read:getModulation");
    algo.properties.push(
      "name:presetModulationSpeed|type:float|display:Mod Speed|values:0.01,1|write:setModulationSpeed|read:getModulationSpeed");
    algo.properties.push("name:presetPositions|type:string|display:Positions (0..1 CSV)|write:setPositions|read:getPositions");
    algo.properties.push("name:presetFlip|type:list|display:Flip|values:Off,On|write:setFlip|read:getFlip");
    algo.properties.push("name:presetMirror|type:list|display:Mirror|values:Off,On|write:setMirror|read:getMirror");
    algo.properties.push("name:presetBrightness|type:float|display:Brightness|values:0,1|write:setBrightness|read:getBrightness");
    algo.properties.push("name:presetBlur|type:float|display:Blur|values:0,10|write:setBlur|read:getBlur");
    algo.properties.push("name:presetBackgroundMode|type:list|display:Background Mode|values:Off,Additive|write:setBackgroundMode|read:getBackgroundMode");
    algo.properties.push("name:presetBackgroundColor|type:string|display:Background Color (#rrggbb)|write:setBackgroundColor|read:getBackgroundColor");
    algo.properties.push("name:presetBackgroundBrightness|type:float|display:Background Brightness|values:0,1|write:setBackgroundBrightness|read:getBackgroundBrightness");

    algo.setRollSpeed = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0.25;
      algo.presetRollSpeed = Math.max(0, Math.min(1, value));
    };
    algo.getRollSpeed = function() { return algo.presetRollSpeed; };
    algo.setTemporalSpeed = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 1;
      algo.presetTemporalSpeed = Math.max(0.1, Math.min(10, value));
    };
    algo.getTemporalSpeed = function() { return algo.presetTemporalSpeed; };
    algo.setModulation = function(v) {
      algo.presetModulation = (v === "Sine" || v === "Breath") ? v : "Off";
    };
    algo.getModulation = function() { return algo.presetModulation; };
    algo.setModulationSpeed = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0.2;
      algo.presetModulationSpeed = Math.max(0.01, Math.min(1, value));
    };
    algo.getModulationSpeed = function() { return algo.presetModulationSpeed; };
    algo.setPositions = function(v) { algo.presetPositions = typeof v === "string" ? v : ""; };
    algo.getPositions = function() { return algo.presetPositions; };
    algo.setFlip = function(v) { algo.presetFlip = v === "On" ? "On" : "Off"; };
    algo.getFlip = function() { return algo.presetFlip; };
    algo.setMirror = function(v) { algo.presetMirror = v === "On" ? "On" : "Off"; };
    algo.getMirror = function() { return algo.presetMirror; };
    algo.setBrightness = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 1;
      algo.presetBrightness = Math.max(0, Math.min(1, value));
    };
    algo.getBrightness = function() { return algo.presetBrightness; };
    algo.setBlur = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0;
      algo.presetBlur = Math.max(0, Math.min(10, value));
    };
    algo.getBlur = function() { return algo.presetBlur; };
    algo.setBackgroundMode = function(v) { algo.presetBackgroundMode = v === "Additive" ? "Additive" : "Off"; };
    algo.getBackgroundMode = function() { return algo.presetBackgroundMode; };
    algo.setBackgroundColor = function(v) {
      if (typeof v === "string" && /^#[0-9a-f]{6}$/i.test(v))
        algo.presetBackgroundColor = v;
    };
    algo.getBackgroundColor = function() { return algo.presetBackgroundColor; };
    algo.setBackgroundBrightness = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 1;
      algo.presetBackgroundBrightness = Math.max(0, Math.min(1, value));
    };
    algo.getBackgroundBrightness = function() { return algo.presetBackgroundBrightness; };

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

    algo.rgbMapStepCount = function(width, height) { return 2; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    function breathRatio(phase) {
      var t = HSVUtil.mod1(phase);
      if (t < (1 / 3))
        return 0.4 * Math.sin(t * 6 * Math.PI - Math.PI / 2) + 0.6;
      return Math.exp(3 - t * 9) + 0.2;
    }

    algo.rgbMap = function(width, height, rgb, step)
    {
      var map = HSVUtil.createMap(width, height);
      var stops = (algo.colors && algo.colors.length > 0) ? algo.colors : [{h: 0, s: 0, v: 1}];
      var seconds = elapsedSeconds();
      if (seconds > 0) {
        algo.roll = HSVUtil.mod1(algo.roll + seconds * algo.presetRollSpeed * 0.2);
        algo.phase = HSVUtil.mod1(algo.phase + seconds * algo.presetTemporalSpeed * algo.presetModulationSpeed * 0.1);
      }

      var n = width * height;
      for (var i = 0; i < n; i++) {
        var u = n <= 1 ? 0 : i / (n - 1);
        var t = u + algo.roll;
        while (t > 1) t -= 1;
        while (t < 0) t += 1;
        var rgbColor = HSVUtil.gradientRgbAt(stops, t, algo.presetPositions);
        var color = HSVUtil.rgbToHsv(rgbColor[0], rgbColor[1], rgbColor[2]);
        var value = color.v;
        if (algo.presetModulation === "Sine") {
          value *= 0.6 + 0.4 * Math.max(0, Math.sin(2 * Math.PI * HSVUtil.mod1(u + algo.phase)));
        } else if (algo.presetModulation === "Breath") {
          var litCells = Math.floor(breathRatio(algo.phase) * n);
          value *= i < litCells ? 1.0 : 0.0;
        }
        map[i * 3] = color.h;
        map[i * 3 + 1] = color.s;
        map[i * 3 + 2] = HSVUtil.clamp01(value);
      }
      return HSVUtil.applyStripTransforms(map, width, height, {
        flip: algo.presetFlip,
        mirror: algo.presetMirror,
        brightness: algo.presetBrightness,
        blur: algo.presetBlur,
        backgroundMode: algo.presetBackgroundMode,
        backgroundColor: algo.presetBackgroundColor,
        backgroundBrightness: algo.presetBackgroundBrightness
      });
    };

    testAlgo = algo;
    return algo;
  }
)();
