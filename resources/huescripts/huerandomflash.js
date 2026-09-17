/*
  Q Light Controller Plus
  huerandomflash.js

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
    algo.name = "Hue Random Flash";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 1;
    algo.usesAudio = false;
    algo.properties = new Array();

    algo.presetDuration = 0.5;
    algo.presetProbability = 0.25;
    algo.presetSize = 25;
    algo.lastTimeMs = null;
    algo.active = null;
    algo.footprint = null;

    algo.properties.push(
      "name:presetDuration|type:float|display:Duration|values:0.1,5|write:setDuration|read:getDuration");
    algo.properties.push(
      "name:presetProbability|type:float|display:Probability|values:0.01,1|write:setProbability|read:getProbability");
    algo.properties.push(
      "name:presetSize|type:float|display:Size %|values:1,100|write:setSize|read:getSize");

    algo.setDuration = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0.5;
      algo.presetDuration = Math.max(0.1, Math.min(5, value));
    };
    algo.getDuration = function() { return algo.presetDuration; };
    algo.setProbability = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0.25;
      algo.presetProbability = Math.max(0.01, Math.min(1, value));
    };
    algo.getProbability = function() { return algo.presetProbability; };
    algo.setSize = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 25;
      algo.presetSize = Math.max(1, Math.min(100, value));
    };
    algo.getSize = function() { return algo.presetSize; };

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

    function spawn(n) {
      var size = Math.max(1, Math.round(n * algo.presetSize / 100));
      var start = Math.floor(Math.random() * Math.max(1, n - size + 1));
      algo.active = { start: start, size: size, remaining: algo.presetDuration };
    }

    function color() {
      if (algo.colors && algo.colors.length > 0) return algo.colors[0];
      return {h: 0, s: 0, v: 1};
    }

    algo.rgbMapStepCount = function(width, height) { return 2; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step)
    {
      var map = HSVUtil.createMap(width, height);
      var n = width * height;
      var seconds = elapsedSeconds();
      if (!algo.footprint || algo.footprint.width !== width || algo.footprint.height !== height) {
        algo.footprint = { width: width, height: height };
        algo.active = null;
      }
      if (algo.active && (algo.active.start < 0 || algo.active.start >= n
          || algo.active.start + algo.active.size > n))
        algo.active = null;

      if (algo.active) {
        algo.active.remaining -= seconds;
        if (algo.active.remaining <= 0) algo.active = null;
      } else if (seconds > 0) {
        var chance = 1 - Math.pow(1 - algo.presetProbability, seconds);
        if (Math.random() < chance) spawn(n);
      }

      if (!algo.active) return map;

      var c = color();
      var level = HSVUtil.clamp01(algo.active.remaining / algo.presetDuration);
      var end = Math.min(n, algo.active.start + algo.active.size);
      for (var i = algo.active.start; i < end; i++) {
        map[i * 3] = c.h;
        map[i * 3 + 1] = c.s;
        map[i * 3 + 2] = c.v * level;
      }
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
