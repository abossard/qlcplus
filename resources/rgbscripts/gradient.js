/*
  Q Light Controller Plus
  gradient.js

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

// Development tool access
var testAlgo;

(
  function()
  {
    var algo = new Object;
    algo.apiVersion = 2;
    algo.name = "Gradient";
    algo.author = "Massimo Callegari";
    algo.acceptColors = 0;
    algo.properties = new Array();
    algo.presetIndex = 0;
    algo.properties.push("name:presetIndex|type:list|display:Preset|values:Rainbow,Sunset,Abstract,Ocean|write:setPreset|read:getPreset");
    algo.presetSize = 5;
    algo.properties.push("name:presetSize|type:range|display:Size|values:1,40|write:setSize|read:getSize");
    algo.orientation = 0;
    algo.properties.push("name:orientation|type:list|display:Orientation|values:Horizontal,Vertical,Radial|write:setOrientation|read:getOrientation");

    var util = new Object;
    util.initialized = false;
    util.gradientData = new Array();
    util.presets = new Array();
    // Rainbow: Red -> Green -> Blue
    util.presets.push([{h: 0, s: 1, v: 1}, {h: 0.3333, s: 1, v: 1}, {h: 0.6667, s: 1, v: 1}]);
    // Sunset: Yellow -> Red
    util.presets.push([{h: 0.1667, s: 1, v: 1}, {h: 0, s: 1, v: 1}]);
    // Abstract: Blue-ish -> Cyan -> Magenta -> Yellow
    util.presets.push([{h: 0.639, s: 0.667, v: 1}, {h: 0.5, s: 1, v: 1}, {h: 0.8333, s: 1, v: 1}, {h: 0.1667, s: 1, v: 1}]);
    // Ocean: Deep Blue -> Cyan
    util.presets.push([{h: 0.614, s: 1, v: 0.725}, {h: 0.514, s: 0.992, v: 1}]);

    algo.setPreset = function(_preset)
    {
      if (_preset === "Rainbow") { algo.presetIndex = 0; }
      else if (_preset === "Sunset") { algo.presetIndex = 1; }
      else if (_preset === "Abstract") { algo.presetIndex = 2; }
      else if (_preset === "Ocean") { algo.presetIndex = 3; }
      else { algo.presetIndex = 0; }
      util.initialize();
    };

    algo.getPreset = function()
    {
      if (algo.presetIndex === 0) { return "Rainbow"; }
      else if (algo.presetIndex === 1) { return "Sunset"; }
      else if (algo.presetIndex === 2) { return "Abstract"; }
      else if (algo.presetIndex === 3) { return "Ocean"; }
      else { return "Rainbow"; }
    };

    algo.setSize = function(_size)
    {
      algo.presetSize = _size;
      util.initialize();
    };

    algo.getSize = function()
    {
      return algo.presetSize;
    };

    algo.setOrientation = function(_orientation)
    {
      if (_orientation === "Vertical") { algo.orientation = 1; }
      else if (_orientation === "Radial") { algo.orientation = 2; }
      else { algo.orientation = 0; }
      util.initialize();
    };

    algo.getOrientation = function()
    {
      if (algo.orientation === 1) { return "Vertical"; }
      else if (algo.orientation === 2) { return "Radial"; }
      else { return "Horizontal"; }
    };

    // Shortest-arc hue interpolation
    function lerpHue(h1, h2, t) {
      var diff = h2 - h1;
      if (diff > 0.5) { diff -= 1; }
      else if (diff < -0.5) { diff += 1; }
      var h = h1 + diff * t;
      if (h < 0) { h += 1; }
      else if (h >= 1) { h -= 1; }
      return h;
    }

    util.initialize = function()
    {
      var gradIdx = 0;
      util.gradientData = new Array();
      var preset = util.presets[algo.presetIndex];
      for (var i = 0; i < preset.length; i++)
      {
        var sColor = preset[i];
        var eColor = preset[(i + 1) % preset.length];

        util.gradientData[gradIdx++] = {h: sColor.h, s: sColor.s, v: sColor.v};

        for (var s = 1; s < algo.presetSize; s++)
        {
          var t = s / algo.presetSize;
          util.gradientData[gradIdx++] = {
            h: lerpHue(sColor.h, eColor.h, t),
            s: sColor.s + (eColor.s - sColor.s) * t,
            v: sColor.v + (eColor.v - sColor.v) * t
          };
        }
      }
      util.initialized = true;
    };

    algo.rgbMap = function(width, height, rgb, step)
    {
      if (util.initialized === false)
      {
        util.initialize(width);
      }

      var gradStep = 0;
      var map = HSVUtil.createMap(width, height);
      for (var y = 0; y < height; y++)
      {

          if (algo.orientation === 1) {
            gradStep = step + y;
          }
          for (var x = 0; x < width; x++)
          {
            if (algo.orientation === 0)
            {
              gradStep = step + x;
            }
            else if (algo.orientation === 2)
            {
              var xdis = x - ((width-1)/2);
              var ydis = y - ((height-1)/2);
              gradStep = step + Math.round( Math.sqrt((xdis * xdis) + (ydis * ydis)));
            }
            if (gradStep >= util.gradientData.length)
            {
              gradStep = (gradStep % util.gradientData.length);
            }

            var color = util.gradientData[gradStep];
            var i = (y * width + x) * 3;
            map[i] = color.h; map[i + 1] = color.s; map[i + 2] = color.v;
          }
      }

      return map;
    };

    algo.rgbMapStepCount = function(width, height)
    {
      if (util.initialized === false) {
        util.initialize();
      }
      return util.gradientData.length;
    };

    // Development tool access
    testAlgo = algo;

    return algo;
  }
)();
