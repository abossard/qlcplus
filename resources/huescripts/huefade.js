/*
  Q Light Controller Plus
  huefade.js

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
    algo.name = "Hue Fade";
    algo.author = "Ported from LedFx";
    algo.acceptColors = 5;
    algo.usesAudio = false;
    algo.properties = new Array();

    algo.presetSpeed = 1.0;
    algo.presetPositions = "";
    algo.presetFlip = "Off";
    algo.presetMirror = "Off";
    algo.presetBrightness = 1.0;
    algo.presetBlur = 0;
    algo.presetBackgroundMode = "Off";
    algo.presetBackgroundColor = "#000000";
    algo.presetBackgroundBrightness = 1.0;
    algo.properties.push(
      "name:presetSpeed|type:float|display:Speed|values:0.1,10|write:setSpeed|read:getSpeed");
    algo.properties.push("name:presetPositions|type:string|display:Positions (0..1 CSV)|write:setPositions|read:getPositions");
    algo.properties.push("name:presetFlip|type:list|display:Flip|values:Off,On|write:setFlip|read:getFlip");
    algo.properties.push("name:presetMirror|type:list|display:Mirror|values:Off,On|write:setMirror|read:getMirror");
    algo.properties.push("name:presetBrightness|type:float|display:Brightness|values:0,1|write:setBrightness|read:getBrightness");
    algo.properties.push("name:presetBlur|type:float|display:Blur|values:0,10|write:setBlur|read:getBlur");
    algo.properties.push("name:presetBackgroundMode|type:list|display:Background Mode|values:Off,Additive|write:setBackgroundMode|read:getBackgroundMode");
    algo.properties.push("name:presetBackgroundColor|type:string|display:Background Color (#rrggbb)|write:setBackgroundColor|read:getBackgroundColor");
    algo.properties.push("name:presetBackgroundBrightness|type:float|display:Background Brightness|values:0,1|write:setBackgroundBrightness|read:getBackgroundBrightness");

    algo.setSpeed = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 1.0;
      algo.presetSpeed = Math.max(0.1, Math.min(10, value));
    };
    algo.getSpeed = function() { return algo.presetSpeed; };
    algo.setPositions = function(v) {
      algo.presetPositions = typeof v === "string" ? v : "";
    };
    algo.getPositions = function() { return algo.presetPositions; };
    algo.setFlip = function(v) { algo.presetFlip = v === "On" ? "On" : "Off"; };
    algo.getFlip = function() { return algo.presetFlip; };
    algo.setMirror = function(v) { algo.presetMirror = v === "On" ? "On" : "Off"; };
    algo.getMirror = function() { return algo.presetMirror; };
    algo.setBrightness = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 1.0;
      algo.presetBrightness = Math.max(0, Math.min(1, value));
    };
    algo.getBrightness = function() { return algo.presetBrightness; };
    algo.setBlur = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 0;
      algo.presetBlur = Math.max(0, Math.min(10, value));
    };
    algo.getBlur = function() { return algo.presetBlur; };
    algo.setBackgroundMode = function(v) {
      algo.presetBackgroundMode = v === "Additive" ? "Additive" : "Off";
    };
    algo.getBackgroundMode = function() { return algo.presetBackgroundMode; };
    algo.setBackgroundColor = function(v) {
      if (typeof v === "string" && /^#[0-9a-f]{6}$/i.test(v))
      algo.presetBackgroundColor = v;
    };
    algo.getBackgroundColor = function() { return algo.presetBackgroundColor; };
    algo.setBackgroundBrightness = function(v) {
      var value = parseFloat(v);
      if (!isFinite(value)) value = 1.0;
      algo.presetBackgroundBrightness = Math.max(0, Math.min(1, value));
    };
    algo.getBackgroundBrightness = function() { return algo.presetBackgroundBrightness; };

    algo.position = 0;
    algo.direction = 1;
    algo.lastTimeMs = null;

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

    function advance(seconds) {
      var delta = seconds * algo.presetSpeed * 0.015;
      var next = algo.position + delta * algo.direction;
      while (next > 1 || next < 0) {
        if (next > 1) {
          next = 2 - next;
          algo.direction = -1;
        } else if (next < 0) {
          next = -next;
          algo.direction = 1;
        }
      }
      algo.position = next;
    }

    algo.rgbMapStepCount = function(width, height) { return 2; };
    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step)
    {
      var map = HSVUtil.createMap(width, height);
      if (width <= 0 || height <= 0) return map;
      var seconds = elapsedSeconds();
      if (seconds > 0) advance(seconds);
      var stops = (algo.colors && algo.colors.length > 0) ? algo.colors : [{h: 0, s: 0, v: 1}];
      var rgbColor = HSVUtil.gradientRgbAt(stops, algo.position, algo.presetPositions);
      var color = HSVUtil.rgbToHsv(rgbColor[0], rgbColor[1], rgbColor[2]);
      for (var i = 0; i < width * height; i++) {
        map[i * 3] = color.h;
        map[i * 3 + 1] = color.s;
        map[i * 3 + 2] = color.v;
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
