/*
  Q Light Controller Plus
  blinder.js

  Copyright (c) Hans-Jürgen Tappe

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
    algo.name = "Blinder";
    algo.author = "Hans-Jürgen Tappe";
    algo.acceptColors = 2;
    algo.properties = new Array();

    algo.divisor = 1;
    algo.properties.push("name:divisor|type:range|display:Divisor|values:1,30|write:setDivisor|read:getDivisor");
    
    // Prepare internal properties
    algo.size = 0;
    algo.width = 0;
    algo.height = 0;
    algo.numX = 0;
    algo.numY = 0;
    
    var util = new Object;
    algo.initialized = false;

    algo.setDivisor = function(_divisor)
    {
      algo.divisor = _divisor;
      algo.initialized = false;
    };

    algo.getDivisor = function()
    {
      return algo.divisor;
    };

    util.initialize = function(width, height)
    {
      algo.width = width;
      algo.height = height;
      if (width <= height) {
        algo.size = width / algo.divisor;
        algo.numX = algo.divisor;
        algo.numY = Math.floor(height / algo.size);
      } else {
        algo.size = height / algo.divisor;
        algo.numX = Math.floor(width / algo.size);
        algo.numY = algo.divisor;
      }
      
      algo.bulb = new Array();

      var count = 0;
      for (var w = 0; w < algo.numX; w++) {
        for (var h = 0; h < algo.numY; h++) {
          algo.bulb[count] = {
            x: (2 * w + 1 ) * width / 2 / algo.numX - 0.5,
            y: (2 * h + 1 ) * height / 2 / algo.numY - 0.5,
          };
          algo.bulb[count].xMin = Math.floor(algo.bulb[count].x - algo.size / 2) - 1;
          algo.bulb[count].xMax = Math.ceil(algo.bulb[count].x + algo.size / 2) - 1;
          algo.bulb[count].yMin = Math.floor(algo.bulb[count].y - algo.size / 2) - 1;
          algo.bulb[count].yMax = Math.ceil(algo.bulb[count].y + algo.size / 2) - 1;
          count ++;
        }
      }

      algo.initialized = true;
      return;
    };

    algo.rgbMap = function(width, height, rgb, progstep)
    {
      if (algo.initialized === false || width !== algo.width || height !== algo.height) {
        util.initialize(width, height);
      }

      var map = RGBUtil.createMap(width, height);

      var baseH = algo.color.h;
      var baseS = algo.color.s;
      var baseV = algo.color.v;
      
      var stepPercent =  progstep / (algo.rgbMapStepCount(width, height) - 1);

      var bgPower = Math.pow(stepPercent + 0.1, 2);
      var bgFactor = Math.min(1, bgPower);
      var bgV = baseV * bgFactor;

      // for each bulb displayed
      for (var i = 0; i < algo.bulb.length; i++) {

        for (var ry = algo.bulb[i].yMin; ry <= algo.bulb[i].yMax; ry++) {
          for (var rx = algo.bulb[i].xMin; rx <= algo.bulb[i].xMax; rx++) {
            // Draw only if edges are on the map
            if (rx >= 0 && rx < width && ry >= 0 && ry < height) {
              var offx = Math.abs((rx - algo.bulb[i].x) / (algo.size / 2));
              var offy = Math.abs((ry - algo.bulb[i].y) / (algo.size / 2));

              var s = 1 - offx * offx - offy * offy;
              if (s > 0) {
                var aFactor = 4 * Math.sin(Math.PI / 2 * Math.sqrt(1 - offx * offx - offy * offy));
                var bFactor = 6 * Math.cos(Math.PI / 2 * Math.sqrt(1 - offx * offx - offy * offy));
                var cFactor = Math.sqrt(4 * offx * 4 * offx + 4 * offy * 4 * offy);
  
                var factor = aFactor
                  + bFactor
                  - cFactor
                  - 3
                  + stepPercent;
              
                if (factor < 0) {
                  factor = 0;
                }

                // Additive blend on V channel
                var idx = (ry * width + rx) * 3;
                var newV = baseV * Math.max(0, factor);
                if (newV > 0) {
                  var existV = map[idx + 2];
                  if (existV <= 0) {
                    map[idx] = baseH;
                    map[idx + 1] = baseS;
                  }
                  map[idx + 2] = Math.min(1, existV + newV);
                }
              }
            }
          }
        }
      }
      // Apply background: take max of background V and existing V
      for (var ry = 0; ry < height; ry++) {
        for (var rx = 0; rx < width; rx++) {
          var idx = (ry * width + rx) * 3;
          var existV = map[idx + 2];
          if (bgV > existV) {
            map[idx] = baseH;
            map[idx + 1] = baseS;
            map[idx + 2] = bgV;
          }
        }
      }

      return map;
    };

    algo.rgbMapStepCount = function(width, height)
    {
      return 32;
    };

    // Development tool access
    testAlgo = algo;

    return algo;
  }
)();
