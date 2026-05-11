/*
  Q Light Controller Plus
  audiobarcode.js

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

// Note: when Trigger=Flux, spawning is level-triggered (a new line is spawned
// every frame the flux value remains above the threshold, rate-limited by
// presetMinSpawnMs). Use Trigger=Onset for one-shot per articulation.

var testAlgo;

(
  function () {
    var algo = new Object;
    algo.apiVersion = 3;
    algo.name = "Audio Barcode";
    algo.author = "QLC+ contributors";
    algo.acceptColors = 5;
    algo.usesAudio = true;
    algo.properties = new Array();

    var DOMINANT_TINT = 0.4;
    var DEFAULT_GRADIENT = [0xFF4000, 0xFFFFFF, 0x00C0FF];

    algo.presetLineWidth = 2;
    algo.properties.push(
      "name:presetLineWidth|type:range|display:Line Width (px)|" +
      "values:1,8|write:setLineWidth|read:getLineWidth");
    algo.setLineWidth = function(_v) { algo.presetLineWidth = parseInt(_v); };
    algo.getLineWidth = function() { return algo.presetLineWidth; };

    algo.presetScrollSpeed = 25;
    algo.properties.push(
      "name:presetScrollSpeed|type:range|display:Scroll Speed (px/s)|" +
      "values:1,100|write:setScrollSpeed|read:getScrollSpeed");
    algo.setScrollSpeed = function(_v) { algo.presetScrollSpeed = parseInt(_v); };
    algo.getScrollSpeed = function() { return algo.presetScrollSpeed; };

    algo.presetDecayMs = 1500;
    algo.properties.push(
      "name:presetDecayMs|type:range|display:Brightness Decay (ms)|" +
      "values:100,8000|write:setDecayMs|read:getDecayMs");
    algo.setDecayMs = function(_v) { algo.presetDecayMs = parseInt(_v); };
    algo.getDecayMs = function() { return algo.presetDecayMs; };

    algo.presetMinIntensity = 15;
    algo.properties.push(
      "name:presetMinIntensity|type:range|display:Min Intensity (%)|" +
      "values:0,100|write:setMinIntensity|read:getMinIntensity");
    algo.setMinIntensity = function(_v) { algo.presetMinIntensity = parseInt(_v); };
    algo.getMinIntensity = function() { return algo.presetMinIntensity; };

    algo.presetTrigger = "Onset";
    algo.properties.push(
      "name:presetTrigger|type:list|display:Trigger Source|" +
      "values:Onset,Flux|write:setTrigger|read:getTrigger");
    algo.setTrigger = function(_v) { algo.presetTrigger = _v; };
    algo.getTrigger = function() { return algo.presetTrigger; };

    algo.presetFluxThreshold = 30;
    algo.properties.push(
      "name:presetFluxThreshold|type:range|display:Flux Threshold (%)|" +
      "values:0,100|write:setFluxThreshold|read:getFluxThreshold");
    algo.setFluxThreshold = function(_v) { algo.presetFluxThreshold = parseInt(_v); };
    algo.getFluxThreshold = function() { return algo.presetFluxThreshold; };

    algo.presetHfcScale = 100;
    algo.properties.push(
      "name:presetHfcScale|type:range|display:HFC Scale (%=1.0)|" +
      "values:10,400|write:setHfcScale|read:getHfcScale");
    algo.setHfcScale = function(_v) { algo.presetHfcScale = parseInt(_v); };
    algo.getHfcScale = function() { return algo.presetHfcScale; };

    algo.presetMaxLines = 40;
    algo.properties.push(
      "name:presetMaxLines|type:range|display:Max Active Lines|" +
      "values:4,200|write:setMaxLines|read:getMaxLines");
    algo.setMaxLines = function(_v) { algo.presetMaxLines = parseInt(_v); };
    algo.getMaxLines = function() { return algo.presetMaxLines; };

    algo.presetMinSpawnMs = 50;
    algo.properties.push(
      "name:presetMinSpawnMs|type:range|display:Min Spawn Interval (ms)|" +
      "values:0,500|write:setMinSpawnMs|read:getMinSpawnMs");
    algo.setMinSpawnMs = function(_v) { algo.presetMinSpawnMs = parseInt(_v); };
    algo.getMinSpawnMs = function() { return algo.presetMinSpawnMs; };

    algo.presetAxis = "Horizontal";
    algo.properties.push(
      "name:presetAxis|type:list|display:Axis|" +
      "values:Horizontal,Vertical|write:setAxis|read:getAxis");
    algo.setAxis = function(_v) { algo.presetAxis = _v; };
    algo.getAxis = function() { return algo.presetAxis; };

    algo.lines = [];
    algo.lastW = -1;
    algo.lastH = -1;
    algo.spawnAccumMs = 0;

    function ensureState(width, height) {
      if (algo.lastW === width && algo.lastH === height) return;
      algo.lines = [];
      algo.spawnAccumMs = 0;
      algo.lastW = width;
      algo.lastH = height;
    }

    algo.rgbMapStepCount = function(_w, _h) { return 1; };
    algo.rgbMapSetColors = function(_raw) { };
    algo.rgbMapGetColors = function() {
      return (algo.gradientBandColors && algo.gradientBandColors.length >= 3)
        ? algo.gradientBandColors.slice() : DEFAULT_GRADIENT.slice();
    };

    algo.rgbMap = function(width, height, _rgb, _step, audio) {
      var dt = audio.timing.consumerDtMs / 1000.0;
      var dtMs = audio.timing.consumerDtMs;
      ensureState(width, height);

      var horizontal = (algo.presetAxis === "Horizontal");
      var N = horizontal ? width : height;

      var triggered;
      var intensity;
      if (algo.presetTrigger === "Onset") {
        triggered = audio.onset.fired;
        intensity = audio.onset.intensity;
      } else {
        triggered = (audio.features.flux > algo.presetFluxThreshold / 100.0);
        intensity = audio.features.flux;
      }
      var threshold = algo.presetMinIntensity / 100.0;

      algo.spawnAccumMs += dtMs;
      var canSpawn = algo.spawnAccumMs >= algo.presetMinSpawnMs;

      if (triggered && intensity >= threshold && canSpawn) {
        var hfcScale = algo.presetHfcScale / 100.0;
        if (hfcScale < 0.001) hfcScale = 0.001;
        var t = audio.features.hfc / hfcScale;
        var gradient = (audio.colors && audio.colors.gradient && audio.colors.gradient.length > 0)
          ? audio.colors.gradient : DEFAULT_GRADIENT;
        var color = RGBUtil.gradientColorAt(gradient, t);
        var dominantPacked = AudioColors.dominantColor(algo, audio, color, 0.05);
        color = AudioColors.blendPacked(color, dominantPacked, DOMINANT_TINT);
        algo.lines.push({
          position: N - 1,
          color: color,
          brightness: intensity
        });
        algo.spawnAccumMs = 0;

        // Cap line count: cull oldest excess.
        while (algo.lines.length > algo.presetMaxLines) {
          algo.lines.shift();
        }
      }

      var scrollPxPerSec = algo.presetScrollSpeed;
      var decayPerSec = 1.0 / (algo.presetDecayMs / 1000.0);
      for (var i = algo.lines.length - 1; i >= 0; i--) {
        var ln = algo.lines[i];
        ln.position -= scrollPxPerSec * dt;
        ln.brightness -= decayPerSec * dt;
        if (ln.brightness < 0) ln.brightness = 0;
        if (ln.position < -algo.presetLineWidth || ln.brightness <= 0.005) {
          algo.lines.splice(i, 1);
        }
      }

      var map = RGBUtil.createFlatMap(width, height);
      var halfW = algo.presetLineWidth / 2;
      for (var li = 0; li < algo.lines.length; li++) {
        var lln = algo.lines[li];
        var x0 = Math.max(0, Math.floor(lln.position - halfW));
        var x1 = Math.min(N - 1, Math.ceil(lln.position + halfW));
        var bright = lln.brightness;
        if (bright > 1) bright = 1;
        if (bright < 0) bright = 0;
        var stamped = RGBUtil.scaleColor(lln.color, bright);
        for (var p = x0; p <= x1; p++) {
          if (horizontal) {
            for (var y = 0; y < height; y++) map[(y) * width + (p)] = RGBUtil.blendAdd(map[(y) * width + (p)], stamped);
          } else {
            for (var x = 0; x < width; x++) map[(p) * width + (x)] = RGBUtil.blendAdd(map[(p) * width + (x)], stamped);
          }
        }
      }

      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
