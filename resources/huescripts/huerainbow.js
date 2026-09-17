/*
  Q Light Controller Plus
  huerainbow.js

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
    algo.name = "Hue Rainbow";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 0;
    algo.usesAudio = false;
    algo.properties = new Array();

    algo.presetSpeed = 1;
    algo.presetFrequency = 2;
    algo.presetFlip = "Off";
    algo.presetMirror = "Off";
    algo.presetBrightness = 1.0;
    algo.presetBlur = 0;
    algo.presetBackgroundMode = "Off";
    algo.presetBackgroundColor = "#000000";
    algo.presetBackgroundBrightness = 1.0;
    algo.phase = 0;
    algo.lastTimeMs = null;

    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed|values:0.1,20|write:setSpeed|read:getSpeed");
    algo.properties.push(
      "name:presetFrequency|type:float|display:Frequency|values:0.1,64|write:setFrequency|read:getFrequency");
    algo.properties.push("name:presetFlip|type:list|display:Flip|values:Off,On|write:setFlip|read:getFlip");
    algo.properties.push("name:presetMirror|type:list|display:Mirror|values:Off,On|write:setMirror|read:getMirror");
    algo.properties.push("name:presetBrightness|type:float|display:Brightness|values:0,1|write:setBrightness|read:getBrightness");
    algo.properties.push("name:presetBlur|type:float|display:Blur|values:0,10|write:setBlur|read:getBlur");
    algo.properties.push("name:presetBackgroundMode|type:list|display:Background Mode|values:Off,Additive|write:setBackgroundMode|read:getBackgroundMode");
    algo.properties.push("name:presetBackgroundColor|type:string|display:Background Color (#rrggbb)|write:setBackgroundColor|read:getBackgroundColor");
    algo.properties.push("name:presetBackgroundBrightness|type:float|display:Background Brightness|values:0,1|write:setBackgroundBrightness|read:getBackgroundBrightness");

    algo.setSpeed = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 1;
      algo.presetSpeed = Math.max(0.1, Math.min(20, value));
    };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setFrequency = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 2;
      algo.presetFrequency = Math.max(0.1, Math.min(64, value));
    };
    algo.getFrequency = function() { return algo.presetFrequency; };
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

    algo.rgbMap = function(width, height, rgb, step)
    {
      var map = HSVUtil.createMap(width, height);
      var seconds = elapsedSeconds();
      if (seconds > 0)
        algo.phase = HSVUtil.mod1(algo.phase + seconds * algo.presetSpeed * 0.1);

      var n = width * height;
      var denom = Math.max(1, n);
      for (var i = 0; i < n; i++) {
        var h = HSVUtil.mod1(algo.phase + (i * algo.presetFrequency) / denom);
        map[i * 3] = h;
        map[i * 3 + 1] = 0.95;
        map[i * 3 + 2] = 1.0;
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
