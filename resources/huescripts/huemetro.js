/*
  Q Light Controller Plus
  huemetro.js

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
    algo.name = "Hue Metro";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 2;
    algo.usesAudio = false;
    algo.properties = new Array();

    algo.presetPeriod = 4;
    algo.presetDuty = 0.6;
    algo.presetSteps = 4;
    algo.startSeconds = null;
    algo.properties.push(
      "name:presetPeriod|type:float|display:Period (s)|values:1,10|write:setPeriod|read:getPeriod");
    algo.properties.push(
      "name:presetDuty|type:float|display:Duty|values:0.1,0.9|write:setDuty|read:getDuty");
    algo.properties.push(
      "name:presetSteps|type:range|display:Steps|values:1,6|write:setSteps|read:getSteps");

    algo.setPeriod = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 4;
      algo.presetPeriod = Math.max(1, Math.min(10, value));
    };
    algo.getPeriod = function() { return algo.presetPeriod; };
    algo.setDuty = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0.6;
      algo.presetDuty = Math.max(0.1, Math.min(0.9, value));
    };
    algo.getDuty = function() { return algo.presetDuty; };
    algo.setSteps = function(v) {
      var value = parseInt(v);
      if (!isFinite(value)) value = 4;
      algo.presetSteps = Math.max(1, Math.min(6, value));
    };
    algo.getSteps = function() { return algo.presetSteps; };

    function color(index, fallback) {
      if (algo.colors && algo.colors[index]) return algo.colors[index];
      return fallback;
    }

    function isLit(index, n, stage) {
      if (stage <= 0) return true;
      var blockCount = Math.pow(2, stage - 1);
      if (blockCount <= 0) return false;
      for (var b = 0; b < blockCount; b++) {
        var start = Math.floor((b * n) / blockCount);
        var end = Math.floor(((b + 1) * n) / blockCount);
        var span = Math.max(0, end - start);
        var lit = Math.max(0, Math.floor(span / 2) - 1);
        if (index >= start && index < start + lit) return true;
      }
      return false;
    }

    algo.rgbMapStepCount = function(width, height) { return 2; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step)
    {
      var map = HSVUtil.createMap(width, height);
      var n = width * height;
      var bg = color(0, {h: 0, s: 0, v: 0});
      var fg = color(1, {h: 0, s: 0, v: 1});

      var nowSeconds = Date.now() / 1000;
      if (algo.startSeconds === null) algo.startSeconds = nowSeconds;
      var passTime = Math.max(0, nowSeconds - algo.startSeconds);
      var cycleTime = passTime % algo.presetPeriod;
      var onCycle = cycleTime <= algo.presetPeriod * algo.presetDuty;
      var stage = Math.floor(passTime / algo.presetPeriod) % Math.max(1, algo.presetSteps);

      for (var i = 0; i < n; i++) {
        var c = (onCycle && isLit(i, n, stage)) ? fg : bg;
        map[i * 3] = c.h;
        map[i * 3 + 1] = c.s;
        map[i * 3 + 2] = c.v;
      }
      return map;
    };

    testAlgo = algo;
    return algo;
  }
)();
