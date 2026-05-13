/*
  Q Light Controller Plus
  starfield.js

  Copyright (c) Doug Puckett
  With Additons by Branson Matheson

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
  function() {
    var algo = {};
    algo.apiVersion = 2;
    algo.name = "3D Starfield";
    algo.author = "Doug Puckett+Branson Matheson";
    algo.properties = [];
    algo.acceptColors = 1;
    algo.presetColor = {h: 0, s: 0, v: 0};
    algo.properties.push("name:StarsAmount|type:range|display:Number of Stars (10-255)|values:10,255|write:setAmount|read:getAmount");
    algo.presetStars = 50;          // 50 stars on screen at one time (default)
    algo.properties.push("name:MultiColor|type:list|display:MultiColored Stars?|values:No,Yes|write:setMulti|read:getMulti");
    algo.multiColor = 0;            // Multicolor stars off defaultly (1 = stars will be randomly colored)
    algo.properties.push("name:InvertBrightness|type:list|display:Invert Brightness|values:Dim->Bright,Bright->Dim|write:setInvert|read:getInvert");
    algo.invertColor = 0;           // Reverse Brightness
    var depth = 128;                // depth - best not to change
    var stars = new Array(255);     // main star position array

    algo.setAmount = function(_amount) {
      algo.presetStars = _amount;
    };

    algo.getAmount = function() {
      return algo.presetStars;
    };

    algo.setMulti = function(_multic) {
      if (_multic === "Yes") {
        algo.multiColor = 1;    // Random Colored Stars
      } else {
        algo.multiColor = 0;    // Stars are chosen color
      }
    };

    algo.getMulti = function() {
      if (algo.multiColor === 1) {
        return "Yes";
      } else {
        return "No";
      }
    };

    algo.setInvert = function(_invert) {
      if (_invert === "Bright->Dim") {
        algo.invertColor = 1;    // Start Bright -> Dim
      } else {
        algo.invertColor = 0;    // Start Dim -> Bright
      }
    };

    algo.getInvert = function() {
      if (algo.invertColor === 1) {
        return "Bright->Dim";
      } else {
        return "Dim->Bright";
      }
    };


    var util = new Object;
    algo.initialized = false;

    //random position function for new star
    function getNewNumberRange(minVal, maxVal) {
      minVal = Math.random() * minVal;
      maxVal = Math.random() * maxVal;
      return Math.floor(Math.random() * (maxVal - minVal + 1)) + minVal;
    }

    //set color of star - if multicolor, choose random color
    function getNewColor(isMultiColor, zColor) {
      if (isMultiColor === 1) {
        return {
          h: Math.random(),
          s: 0.7 + Math.random() * 0.3,
          v: 1
        };
      } else {
        return zColor;
      }
    }

    // initialize the stars and load random positions
    util.initialize = function(width, height) {
      for (var i = 0; i < stars.length; i++) {
        stars[i] = {
          x: getNewNumberRange(-10, 10),
          y: getNewNumberRange(-10, 10),
          z: depth,
          c: getNewColor(algo.multiColor, algo.presetColor)
        };
      }

      algo.initialized = true;
      return;
    };

    // main QLC+ routine where the work is done
    algo.rgbMap = function(width, height, rgb, step) {
      if (algo.initialized === false) {
        util.initialize(width, height);
      }

      var map = HSVUtil.createMap(width, height);

      // find center of display
      var halfWidth = width / 2;
      var halfHeight = height / 2;

      // start moving the stars
      for (var i = 0; i < algo.presetStars - 1; i++) {

        // decrease depth on each pass through 
        if (height >= width) { stars[i].z -= height / (height / 4); }
        else { stars[i].z -= width / (width / 4); }

        // if star is off screen, create a new star near center
        if (stars[i].z <= 0) {
          stars[i].x = getNewNumberRange(-10, 10);
          stars[i].y = getNewNumberRange(-10, 10);
          stars[i].z = depth;
          stars[i].c = getNewColor(algo.multiColor, algo.colors[0]);
        }

        // calculate the stars next position
        var k = 200 / stars[i].z;
        var px = Math.floor(stars[i].x * k + halfWidth);
        var py = Math.floor(stars[i].y * k + halfHeight);

        if (px >= 0 && px < width && py >= 0 && py < height) {
          // Brightness factor based on depth
          var factor;
          if (algo.invertColor == 1) {
            factor = Math.max(0, Math.min(1, stars[i].z * 2 / 255));
          } else {
            factor = Math.max(0, Math.min(1, 1 - stars[i].z * 2 / 255));
          }

          px = Math.floor(px);
          py = Math.floor(py);
          HSVUtil.setPixel(map, width, px, py,
            stars[i].c.h, stars[i].c.s, stars[i].c.v * factor);
        } else {
          stars[i].z = 0;     // force a new star
        }
      }
      return map;

    };

    algo.rgbMapStepCount = function(width, height) {
      return 2;
    };

    // Development tool access
    testAlgo = algo;

    return algo;
  }
)();
