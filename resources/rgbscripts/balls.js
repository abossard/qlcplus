/*
  Q Light Controller Plus
  balls.js

  Copyright (c) Rob Nieuwenhuizen, Tim Cullingworth

  Licensed under the Apache License, Version 2.0 (the 'License');
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an 'AS IS' BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

// Development tool access
var testAlgo;

(
  function () {
    var algo = new Object;
    algo.apiVersion = 3;
    algo.name = "Balls";
    algo.author = "Rob Nieuwenhuizen, Tim Cullingworth";
    algo.acceptColors = 5;
    algo.properties = new Array();
    algo.presetSize = 1;
    algo.properties.push(
      "name:presetSize|type:range|display:Size|" +
      "values:1,20|write:setSize|read:getSize");
    algo.presetNumber = 5;
    algo.properties.push(
      "name:presetNumber|type:range|display:Number|" +
      "values:1,15|write:setNumber|read:getNumber");
    algo.presetCollision = 0;
    algo.properties.push(
      "name:presetCollision|type:list|display:Self Collision|" +
      "values:No,Yes|write:setCollision|read:getCollision");
    algo.presetSize = 5;
    algo.properties.push(
      "name:presetIndex|type:list|display:Preset|" +
      "values:User Defined,Random|" +
      "write:setPreset|read:getPreset");
    algo.presetIndex = 0;

    algo.initialized = false;

    var util = new Object;
    util.colorArray = new Array(algo.acceptColors);

    function addPixel(map, idx, h, s, v) {
      v = Math.max(0, Math.min(1, v));
      if (v <= 0) return;
      var existV = map[idx + 2];
      if (existV <= 0) {
        map[idx] = h; map[idx + 1] = s; map[idx + 2] = v;
      } else {
        if (v > existV) { map[idx] = h; map[idx + 1] = s; }
        map[idx + 2] = Math.min(1, existV + v);
      }
    }

    algo.setSize = function (_size) {
      algo.presetSize = parseInt(_size);
    };
    algo.getSize = function () {
      return algo.presetSize;
    };

    algo.setNumber = function (_step) {
      algo.presetNumber = parseInt(_step);
      algo.initialized = false;
    };
    algo.getNumber = function () {
      return algo.presetNumber;
    };
    algo.setCollision = function (_colision) {
      if (_colision === "Yes") { algo.presetCollision = 0; }
      else if (_colision === "No") { algo.presetCollision = 1; }
    };
    algo.getCollision = function () {
      if (algo.presetCollision === 0) { return "Yes"; }
      else if (algo.presetCollision === 1) { return "No"; }
    };

    util.getRawColor = function (idx) {
      idx = idx % util.colorArray.length;
      var color = util.colorArray[idx];
      return color;
    }

    algo.setPreset = function(_preset)
    {
      algo.acceptColors = 0;
      if (_preset === "User Defined")
      {
        algo.presetIndex = 0;
        algo.acceptColors = 5;
        util.colorArray = [
          {h: 0.333, s: 1, v: 1},
          {h: 0.111, s: 1, v: 1},
          {h: 0.667, s: 1, v: 1},
          {h: 0.167, s: 1, v: 1},
          {h: 0.556, s: 1, v: 1}
        ];
      }
      else if (_preset === "Random")
      {
        algo.presetIndex = 1;
        util.colorArray = new Array();
        for (var i = 0; i < algo.presetNumber; i++)
        {
          util.colorArray[i] = {
            h: Math.random(),
            s: 0.7 + Math.random() * 0.3,
            v: 0.7 + Math.random() * 0.3
          };
        }
      }
      else { algo.presetIndex = 0; }
      util.initialized = false;
    };

    algo.getPreset = function()
    {
      if (algo.presetIndex === 0) { return "User Defined"; }
      else if (algo.presetIndex === 1) { return "Random"; }
      else { return "Rainbow"; }
    };

    algo.rgbMapSetColors = function(rawColors)
    {
    }

    algo.rgbMapGetColors = function()
    {
      return [];
    }

    util.initialize = function (width, height) {
      algo.ball = new Array(algo.presetNumber);
      algo.direction = new Array(algo.presetNumber);

      for (var i = 0; i < algo.presetNumber; i++) {
        var x = Math.random() * (width - 1);
        var y = Math.random() * (height - 1);
        algo.ball[i] = [y, x];
        var yDirection = (Math.random() * 2) - 1;
        var xDirection = (Math.random() * 2) - 1;
        algo.direction[i] = [yDirection, xDirection];
      }
      algo.initialized = true;
      return;
    };

    algo.rgbMap = function (width, height, rgb, progstep) {
      if (algo.initialized === false) {
        util.initialize(width, height);
      }

      var map = HSVUtil.createMap(width, height);

      for (var i = 0; i < algo.presetNumber; i++) {
        var color = util.getRawColor(i);
        var yx = algo.ball[i];
        var step = algo.direction[i];
        var my = Math.floor(yx[0]);
        var mx = Math.floor(yx[1]);
        var boxSize = Math.round(algo.presetSize / 2);

        for (var ry = my - boxSize; ry < my + boxSize + 2; ry++) {

          for (var rx = mx - boxSize; rx < mx + boxSize + 2; rx++) {

            if (rx < width && rx > -1 && ry < height && ry > -1) {
              var offx = rx - yx[1];
              var offy = ry - yx[0];
              var hyp = 1 - (Math.sqrt((offx * offx) + (offy * offy)) / ((algo.presetSize / 2) + 1));

              if (hyp < 0) { hyp = 0; }

              var idx = (ry * width + rx) * 3;
              var newV = color.v * hyp;
              addPixel(map, idx, color.h, color.s, newV);
            }
          }
        }

        if (algo.presetCollision === 0) {
          for (var ti = 0; ti < algo.presetNumber; ti++) {

            if (ti !== i) {
              var disy = (yx[0] + step[0]) - algo.ball[ti][0];
              var disx = (yx[1] + step[1]) - algo.ball[ti][1];
              var dish = Math.sqrt((disx * disx) + (disy * disy));
              if (dish < (1.414) * (algo.presetSize / 2)) {
                var stepy = step[0];
                var stepx = step[1];
                algo.direction[i][0] = algo.direction[ti][0];
                algo.direction[i][1] = algo.direction[ti][1];
                algo.direction[ti][0] = stepy;
                algo.direction[ti][1] = stepx;
              }
            }
          }
        }

        if (yx[0] <= 0 && step[0] < 0) { step[0] *= -1; }
        else if (yx[0] >= height - 1 && step[0] > 0) { step[0] *= -1; }

        if (yx[1] <= 0 && step[1] < 0) { step[1] *= -1; }
        else if (yx[1] >= width - 1 && step[1] > 0) { step[1] *= -1; }

        yx[0] += step[0];
        yx[1] += step[1];

        algo.ball[i] = yx;
        algo.direction[i] = step;
      }
      return map;
    };

    algo.rgbMapStepCount = function (width, height) {
      return 2;
    };

    // Development tool access
    testAlgo = algo;

    return algo;
  }
)();
