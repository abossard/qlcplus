/*
  Q Light Controller Plus
  flyingobjects.js

  Copyright (c) Hans-Jürgen Tappe
  derived from on balls.js, Copyright (c) Tim Cullingworth

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

(function()
  {
    var util = new Object;

    var geometryCalc = new Object;

    var algo = new Object;
    algo.apiVersion = 2;
    algo.name = "Flying Objects";
    algo.author = "Hans-Jürgen Tappe";
    algo.acceptColors = 1;
    algo.properties = new Array();

    // Algorithms ----------------------------

    var ballAlgo = new Object;
    ballAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt((offx * offx) + (offy * offy));
      var factor = 1 - (distance / (algo.presetRadius + 1));
      if (factor < 0) {
        factor = 0;
      }
      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var bellAlgo = new Object;
    bellAlgo.cache = {
      presetRadius: 0,
    };
    bellAlgo.updateCache = function()
    {
      bellAlgo.cache.scaling = 0.5 - 0.008 * algo.presetSize;
      bellAlgo.cache.bottomOffset = 1.1 * algo.presetRadius;
      bellAlgo.cache.clapperSize = 0.8 * algo.presetRadius - 1;
      bellAlgo.cache.clapperSwing = 0.5 * algo.presetRadius;
      bellAlgo.cache.clapperDeflection = 0.025 * algo.twoPi;
      bellAlgo.cache.presetRadius = algo.presetRadius;
    }
    bellAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (bellAlgo.cache.presetRadius != algo.presetRadius) {
        bellAlgo.updateCache();
      }
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt((offx * offx) + (offy * offy));
      var factor = 0;

      var realOffy = offy + bellAlgo.cache.bottomOffset;
      var scaling = bellAlgo.cache.scaling;
      factor = 1 - Math.sqrt(offx * offx * scaling + realOffy * realOffy) + realOffy;
      factor *= util.blindoutPercent(1 - Math.abs(realOffy) / (algo.presetRadius * 1.9), 10);

      if (offy > bellAlgo.cache.clapperSize) {
    var realOffx = rx - algo.obj[i].x;
    var stepInput = bellAlgo.cache.clapperDeflection;
    stepInput *= algo.progstep;
    stepInput += algo.twoPi * algo.obj[i].random;
    var stepPercent = Math.sin(stepInput);
    realOffx += bellAlgo.cache.clapperSwing * stepPercent;
        realOffy = offy - bellAlgo.cache.clapperSize;
        distance = Math.sqrt((realOffx * realOffx) + (realOffy * realOffy));
        factor = Math.max(factor, 1 - (distance / (algo.presetRadius / 3 + 1)));
      }

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    };

    var candleAlgo = new Object;
    candleAlgo.cache = {
      presetRadius: 0,
    };
    candleAlgo.updateCache = function()
    {
      candleAlgo.cache.bottomWidth = 0.9 * algo.presetRadius;
      candleAlgo.cache.bottomHeight = 0.8 * algo.presetRadius;
      candleAlgo.cache.blindoutPercent = algo.presetRadius * 1.9;
      candleAlgo.cache.sharpness = 1 / algo.presetRadius;
      candleAlgo.cache.presetRadius = algo.presetRadius;
    }
    candleAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (candleAlgo.cache.presetRadius != algo.presetRadius) {
        candleAlgo.updateCache();
      }
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt((offx * offx) + (offy * offy));
      var tips = 3;
      var distPercentY = offy / algo.presetRadius;

      var rotationPercent = -algo.obj[i].xDirection;
      rotationPercent = rotationPercent * Math.max(0, -distPercentY);
      var rotation = (1 - 0.15 * rotationPercent) * Math.PI;
      var realY = 0.09 * ry;
      var realOffy = realY - algo.obj[i].y;
      var angle = geometryCalc.getAngle(offx, realOffy) + rotation;

      angle = angle % algo.twoPi;
      var targetDistance = geometryCalc.getTargetDistance(angle, tips);
      var distPercent = Math.abs(distance / targetDistance);

      var factor = util.blindoutPercent(1 - distPercent, candleAlgo.cache.sharpness);

      realOffy = offy + algo.presetRadius;
      factor *= util.blindoutPercent(1 - Math.abs(realOffy) / candleAlgo.cache.blindoutPercent, 10);

      if (offy > candleAlgo.cache.bottomHeight) {
        distance = Math.sqrt((offx * offx) + 1);
        factor = Math.max(factor, 1 - (distance / candleAlgo.cache.bottomWidth));
      }

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var circleAlgo = new Object;
    circleAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var distPercent = distance / algo.presetRadius;

      var factor = util.blindoutPercent(1 - distPercent, 0.5);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var diamondAlgo = new Object;
    diamondAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var percentX = offx / algo.presetRadius;
      var percentY = offy / algo.presetRadius;
      var saturation = 1.5;

      var factor = Math.sqrt(percentX * percentX / saturation)
              + Math.sqrt(percentY * percentY / saturation);
      factor = 1 - Math.max(0, Math.min(1, factor));

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var diskAlgo = new Object;
    diskAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var distPercent = distance / algo.presetRadius;

      var angle = geometryCalc.getAngle(offx, offy);
      angle -= (algo.twoPi) * ((algo.progstep / 64) % 1);
      angle = (angle + algo.twoPi) % (algo.twoPi);

      var factor = 0.5 * (Math.abs(Math.cos(angle)) + 1);

      factor *= util.blindoutPercent(1 - distPercent, 0.5);

      var inner = 0.2;
      if (algo.presetSize < 7) {
          inner = 0.5;
      }
      distPercent = distance / (algo.presetRadius * inner);
      factor -= util.blindoutPercent(1 - distPercent, 0.3);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var eyeAlgo = new Object;
    eyeAlgo.cache = {
      presetRadius: 0,
    };
    eyeAlgo.updateCache = function()
    {
      eyeAlgo.cache.turn = algo.presetRadius / 10;
      eyeAlgo.cache.targetDistanceOuter = algo.presetRadius / 3;
      eyeAlgo.cache.targetDistanceInner = algo.presetRadius / 8;
      eyeAlgo.cache.presetRadius = algo.presetRadius;
    }
    eyeAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (eyeAlgo.cache.presetRadius != algo.presetRadius) {
        eyeAlgo.updateCache();
      }
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var angle = geometryCalc.getAngle(offx, offy);
      var targetDistance = algo.presetRadius;
      var anglePercent = Math.abs(Math.cos(angle));
      var contraTip = 0.5 * targetDistance * anglePercent;
      var distPercent = distance / (targetDistance - contraTip);
      var factor = util.blindoutPercent(1 - distPercent, 0.5);

      var turn = eyeAlgo.cache.turn;
      offx = rx - turn * algo.obj[i].xDirection - algo.obj[i].x;
      offy = ry - turn * algo.obj[i].yDirection- algo.obj[i].y;
      distance = Math.sqrt(offx * offx + offy * offy);
      
      distPercent = distance / (eyeAlgo.cache.targetDistanceOuter);
      factor -= util.blindoutPercent(1 - distPercent, 0.5);

      distPercent = distance / (eyeAlgo.cache.targetDistanceInner);
      factor += util.blindoutPercent(1 - distPercent, 0.5);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var flowerAlgo = new Object;
    flowerAlgo.cache = {
      presetRadius: 0,
    };
    flowerAlgo.updateCache = function()
    {
      flowerAlgo.cache.innerCircle = 0.3 * algo.presetRadius;
      flowerAlgo.cache.presetRadius = algo.presetRadius;
    }
    flowerAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (flowerAlgo.cache.presetRadius != algo.presetRadius) {
        flowerAlgo.updateCache();
      }
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var distPercent = Math.min(1, distance / algo.presetSize);
      var distPercentInner = distance / flowerAlgo.cache.innerCircle;
      var angle = geometryCalc.getAngle(offx, offy)
      var leafs = 5;
      var factor = 0;
      var baseIntensity = 0.8;

      if (algo.presetSize < 10) {
          leafs = 4;
      }

      angle += algo.obj[i].random * algo.twoPi;
      angle = angle * leafs;
      angle = (angle + algo.twoPi) % (algo.twoPi);

      var scaling = Math.abs(angle - Math.PI) / algo.twoPi * 1.1;
      var percent = Math.min(1, distance / algo.presetRadius + scaling);
      factor = util.blindoutPercent(1 - percent, 0.4);
      factor = (baseIntensity + (1 - baseIntensity) * distPercent) * factor;

      factor = Math.max(factor, util.blindoutPercent(1 - distPercentInner, 3));

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var heartAlgo = new Object;
    heartAlgo.cache = {
      presetRadius: 0,
    };
    heartAlgo.updateCache = function()
    {
      heartAlgo.cache.targetDistanceTop = algo.presetRadius * 3 / 7;
      heartAlgo.cache.circleOffset = algo.presetSize / 5;
      heartAlgo.cache.presetRadius = algo.presetRadius;
    }
    heartAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (heartAlgo.cache.presetRadius != algo.presetRadius) {
        heartAlgo.updateCache();
      }
      // top left
      var offx = rx + heartAlgo.cache.circleOffset - algo.obj[i].x;
      var offy = ry + algo.halfRadius - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var angle = geometryCalc.getAngle(offx, offy);
      var distPercent = distance / heartAlgo.cache.targetDistanceTop;
      var factor = Math.max(0, util.blindoutPercent(1 - distPercent, 2));

      // top right
      offx = rx - heartAlgo.cache.circleOffset - algo.obj[i].x;
      offy = ry + algo.halfRadius - algo.obj[i].y;
      distance = Math.sqrt(offx * offx + offy * offy);
      angle = geometryCalc.getAngle(offx, offy);
      distPercent = distance / heartAlgo.cache.targetDistanceTop;
      factor = Math.max(factor, util.blindoutPercent(1 - distPercent, 2));

      // triangle
      offx = rx - algo.obj[i].x;
      offy = ry - algo.obj[i].y;
      var tips = 3; 
      distance = Math.sqrt(offx * offx + offy * offy);
      angle = geometryCalc.getAngle(offx, offy);
      targetDistance = geometryCalc.getTargetDistance(angle, tips);
      distPercent = distance / targetDistance;
      factor = Math.max(factor, util.blindoutPercent(1 - distPercent, 2));

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var hexagonAlgo = new Object;
    hexagonAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      var tips = 6;
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var angle = geometryCalc.getAngle(offx, offy);
      var targetDistance = geometryCalc.getTargetDistance(angle, tips);
      var distPercent = distance / targetDistance;
      var factor = util.blindoutPercent(1 - distPercent, 1.5);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var maskAlgo = new Object;
    maskAlgo.cache = {
      presetRadius: 0,
    };
    maskAlgo.updateCache = function()
    {
      maskAlgo.cache.targetDistanceOpening = algo.presetRadius / 5;
      maskAlgo.cache.openingOffset = algo.presetSize / 5;
      maskAlgo.cache.presetRadius = algo.presetRadius;
    }
    maskAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (maskAlgo.cache.presetRadius != algo.presetRadius) {
        maskAlgo.updateCache();
      }
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var angle = geometryCalc.getAngle(offx, offy);
      var anglePercent = Math.abs(Math.cos(angle - Math.PI));
      var contraTip = 0.8 * algo.presetRadius * anglePercent;
      var distPercent = distance / (algo.presetRadius - contraTip);
      var factor = util.blindoutPercent(1 - distPercent, 0.5);

      offx = rx - maskAlgo.cache.openingOffset - algo.obj[i].x;
      distance = Math.sqrt(offx * offx + offy * offy);
      angle = geometryCalc.getAngle(offx, offy);
      distPercent = distance / maskAlgo.cache.targetDistanceOpening;
      factor -= util.blindoutPercent(1 - distPercent, 3);

      offx = rx + maskAlgo.cache.openingOffset - algo.obj[i].x;
      distance = Math.sqrt(offx * offx + offy * offy);
      angle = geometryCalc.getAngle(offx, offy);
      distPercent = distance / maskAlgo.cache.targetDistanceOpening;
      factor -= util.blindoutPercent(1 - distPercent, 3);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var pentagonAlgo = new Object;
    pentagonAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      var tips = 5;
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var angle = (geometryCalc.getAngle(offx, offy) + Math.PI) % algo.twoPi;
      var targetDistance = geometryCalc.getTargetDistance(angle, tips);
      var distPercent = distance / targetDistance;
      var factor = util.blindoutPercent(1 - distPercent, 1.5);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var ringAlgo = new Object;
    ringAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var distPercent = distance / algo.presetRadius;

      var factor = util.blindoutPercent(1 - distPercent, 3);

      var inner = 0.7;
      if (algo.presetSize < 7) {
          inner = 0.5;
      }
      distPercent = distance / (algo.presetRadius * inner);
      factor -= util.blindoutPercent(1 - distPercent, 0.5);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var snowflakeAlgo = new Object;
    snowflakeAlgo.cache = {
      presetRadius: 0,
    };
    snowflakeAlgo.updateCache = function()
    {
      snowflakeAlgo.cache.lineWidth = 0.9 + 0.02 * algo.presetRadius;
      snowflakeAlgo.cache.intersect = Math.max(4.5, 0.6 * algo.presetRadius);
      snowflakeAlgo.cache.cWidthSmall = 0.5 * algo.presetRadius;
      snowflakeAlgo.cache.cWidthMain = 1.5 * algo.presetRadius;
      snowflakeAlgo.cache.presetRadius = algo.presetRadius;
    }
    snowflakeAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (snowflakeAlgo.cache.presetRadius != algo.presetRadius) {
          snowflakeAlgo.updateCache();
      }
      var lines = 3;
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var factor = 0;
      var a = 0;
      var c = 0;
      var aWidth = snowflakeAlgo.cache.lineWidth;
      var cWidth = 0;

      var intersect = snowflakeAlgo.cache.intersect;
      if (distance > intersect) {
        var xOffset = Math.sin(Math.PI / 3) * intersect;
        var yOffset = Math.cos(Math.PI / 3) * intersect;
        var smallTips = [
          {"x": 0,        "y": intersect,  "inScope": (ry + intersect < algo.obj[i].y)},
          {"x": 0,        "y": -intersect, "inScope": (ry - intersect > algo.obj[i].y)},
          {"x": xOffset,  "y": yOffset,    "inScope": ((rx < algo.obj[i].x) && (ry < algo.obj[i].y))},
          {"x": xOffset,  "y": -yOffset,   "inScope": ((rx < algo.obj[i].x) && (ry > algo.obj[i].y))},
          {"x": -xOffset, "y": yOffset,    "inScope": ((rx > algo.obj[i].x) && (ry < algo.obj[i].y))},
          {"x": -xOffset, "y": -yOffset,   "inScope": ((rx > algo.obj[i].x) && (ry > algo.obj[i].y))},
        ];
        for (var n = 0; n < smallTips.length; n++) {
          var offset = smallTips[n];
          if (offset.inScope) {
            var realx = rx + offset.x;
            var realy = ry + offset.y;
            var realOffx = Math.abs(realx - algo.obj[i].x);
            var realOffy = Math.abs(realy - algo.obj[i].y);
            var realDistance = Math.sqrt(realOffx * realOffx + realOffy * realOffy);
            var realAngle = geometryCalc.getAngle(realOffx, realOffy);
            realAngle *= lines;
            a = Math.sin(realAngle) * realDistance;
            c = Math.abs(Math.cos(realAngle) * realDistance);
            cWidth = snowflakeAlgo.cache.cWidthSmall;
            a = a / aWidth / lines;
            c = c / cWidth;
            factor = Math.max(factor, 1 - (a * a) + 1 - (c * c) - 1);
          }
        }
      }

      var angle = geometryCalc.getAngle(offx, offy) * lines;
      a = Math.sin(angle) * distance;
      a = a / aWidth / lines;
      cWidth = snowflakeAlgo.cache.cWidthMain;
      c = Math.cos(angle) * distance;
      c = c / cWidth;
      factor = Math.max(factor, 1 - (a * a) + 1 - (c * c) - 1);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var snowmanAlgo = new Object;
    snowmanAlgo.cache = {
      presetRadius: 0,
    };
    snowmanAlgo.updateCache = function()
    {
      snowmanAlgo.cache.size1 = algo.presetSize / 5;
      snowmanAlgo.cache.yOffset1 = algo.presetRadius - 0.75 * snowmanAlgo.cache.size1;
      snowmanAlgo.cache.size2 = algo.presetSize * 1.5 / 5;
      snowmanAlgo.cache.yOffset2 = snowmanAlgo.cache.size2 * 0.3;
      snowmanAlgo.cache.size3 = algo.presetSize * 2 / 5;
      snowmanAlgo.cache.yOffset3 = 0.75 * snowmanAlgo.cache.size3 - algo.presetRadius;
      snowmanAlgo.cache.presetRadius = algo.presetRadius;
    }
    snowmanAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (snowmanAlgo.cache.presetRadius != algo.presetRadius) {
        snowmanAlgo.updateCache();
      }
      var offx = rx - algo.obj[i].x;
 
      // Ball 1
      var offy = ry - algo.obj[i].y + snowmanAlgo.cache.yOffset1;
      var factor1 = Math.max(0, Math.min(1, 1 - (Math.sqrt((offx * offx) + (1.5 * offy * offy)) / (snowmanAlgo.cache.size1 + 1))));

      // Ball 2
      offy = ry - algo.obj[i].y + snowmanAlgo.cache.yOffset2;
      var factor2 = Math.max(0, Math.min(1, 1 - (Math.sqrt((offx * offx) + (offy * offy)) / (snowmanAlgo.cache.size2 + 1))));

      // Ball 3
      offy = ry - algo.obj[i].y + snowmanAlgo.cache.yOffset3;
      var factor3 = Math.max(0, Math.min(1, 1 - (Math.sqrt((offx * offx) + (1.5 * offy * offy)) / (snowmanAlgo.cache.size3 + 1))));

      var factor = Math.max(factor1, factor2);
      factor = Math.max(factor, factor3);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var squareAlgo = new Object;
    squareAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      var tips = 4;
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var angle = geometryCalc.getAngle(offx, offy);
      if (algo.presetSize >= 8) {
          var rotation = algo.twoPi * algo.obj[i].random;
          angle = (angle + rotation) % algo.twoPi;
      }
      var targetDistance = geometryCalc.getTargetDistance(angle, tips);
      var distPercent = distance / targetDistance;
      var factor = util.blindoutPercent(1 - distPercent, 2.5);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var starAlgo = new Object;
    starAlgo.cache = {
      presetRadius: 0,
    };
    starAlgo.updateCache = function()
    {
      starAlgo.cache.triangleSide = algo.presetRadius * Math.sqrt(3);
      starAlgo.cache.expression = 1 + algo.presetSize / 50;
      starAlgo.cache.sharpness = 0.3 + algo.presetSize / 18;
      starAlgo.cache.presetRadius = algo.presetRadius;
    }
    starAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (starAlgo.cache.presetRadius != algo.presetRadius) {
        starAlgo.updateCache();
      }
      var baseIntensity = 0.3;
      var tips = 5;
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var angle = 0;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var factor = 0;

      if (algo.presetSize < 10) {
        tips = 6;
        angle = geometryCalc.getAngle(offx, offy);
        angle *= (tips / 2);
        a = Math.abs(Math.sin(angle) * distance);
        var c = Math.abs(Math.cos(angle) * distance);
        var aWidth = 1.0;
        var cWidth = algo.presetSize;
        a = a / aWidth / (tips / 2);
        c = c / cWidth;
        factor = Math.max(0, 1 - (a * a) + 1 - (c * c) - 1);
      } else {
        angle = geometryCalc.getAngle(offx, offy) + Math.PI;
        var colorAngle = tips * angle;
        colorAngle = (colorAngle + (Math.PI)) % (algo.twoPi);

        var distPercent = distance / algo.presetRadius;
        factor = baseIntensity + (1 - baseIntensity)
          * (Math.abs(colorAngle - Math.PI) / algo.twoPi
            + (1 - distPercent));

        var targetDistance = geometryCalc.getTargetDistance(angle, tips);

        var tipsAngle = (angle % (algo.twoPi / tips)) - (Math.PI / tips);
        var angleSide = Math.abs(Math.sin(tipsAngle) * targetDistance);
        var anglePercent = angleSide / starAlgo.cache.triangleSide;
        var contraTip = starAlgo.cache.expression * targetDistance * anglePercent;
        distPercent = distance / (targetDistance - contraTip);
        factor = factor * util.blindoutPercent(1 - distPercent, starAlgo.cache.sharpness);
      }

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var steeringwheelAlgo = new Object;
    steeringwheelAlgo.cache = {
      presetRadius: 0,
    };
    steeringwheelAlgo.updateCache = function()
    {
      if (algo.presetSize < 7) {
        steeringwheelAlgo.cache.outerCircle = 0.5 * algo.presetRadius;
        steeringwheelAlgo.cache.innerCircle = 0;
      } else {
        steeringwheelAlgo.cache.outerCircle = 0.75 * algo.presetRadius;
        steeringwheelAlgo.cache.innerCircle = 0.65 * algo.presetRadius;
      }
      steeringwheelAlgo.cache.centerCircle = 0.2 * algo.presetRadius;
      steeringwheelAlgo.cache.lineWidth = 0.9 + 0.06 * algo.presetRadius;
      steeringwheelAlgo.cache.lineLength = 1.4 * algo.presetRadius;
      steeringwheelAlgo.cache.presetRadius = algo.presetRadius;
    }
    steeringwheelAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (steeringwheelAlgo.cache.presetRadius != algo.presetRadius) {
        steeringwheelAlgo.updateCache();
      }
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var distPercentCenter = distance / steeringwheelAlgo.cache.centerCircle;
      var lines = 3;
      var factor = 0;

      var angle = geometryCalc.getAngle(offx, offy) * lines;
      var a = Math.abs(Math.sin(angle) * distance);
      a = a / steeringwheelAlgo.cache.lineWidth / lines;
      c = Math.abs(Math.cos(angle) * distance);
      c = c / steeringwheelAlgo.cache.lineLength;
      factor = 1 - (a * a) - (c * c);

      var distPercent = distance / steeringwheelAlgo.cache.outerCircle;
      var ringFactor = Math.max(factor, util.blindoutPercent(1 - distPercent, 3));
      distPercent = distance / steeringwheelAlgo.cache.innerCircle;
      ringFactor -= util.blindoutPercent(1 - distPercent, 5);
      factor = Math.max(factor, ringFactor);

      factor = Math.max(factor, util.blindoutPercent(1 - distPercentCenter, 3));

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var tornadoAlgo = new Object;
    tornadoAlgo.cache = {
      presetRadius: 0,
    };
    tornadoAlgo.updateCache = function()
    {
      tornadoAlgo.cache.centerCircle = 0.15 * algo.presetRadius;
      tornadoAlgo.cache.presetRadius = algo.presetRadius;
    }
    tornadoAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (tornadoAlgo.cache.presetRadius != algo.presetRadius) {
        tornadoAlgo.updateCache();
      }
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var angle = geometryCalc.getAngle(offx, offy);
      var percent = 0;
      var factor = 0;
      
      var fanblades = 3;
      if (algo.presetSize >= 9) {
          fanblades = 5;
      }
      
      angle -= (distance / (algo.presetRadius)) * algo.halfPi;
      angle = fanblades * angle;
      angle += (algo.twoPi) * (algo.progstep / 8 % 1);
      angle = (angle + algo.twoPi) % (algo.twoPi);

      factor = Math.cos(angle);

      percent = Math.max(0, 1 - (distance / algo.presetRadius));
      factor *= util.blindoutPercent(percent, 1)
      
      var distPercentCenter = distance / tornadoAlgo.cache.centerCircle;
      factor = Math.max(factor, util.blindoutPercent(1 - distPercentCenter, 3));

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var treeAlgo = new Object;
    treeAlgo.cache = {
      presetRadius: 0,
    };
    treeAlgo.updateCache = function()
    {
      treeAlgo.cache.bottomWidth = 0.3 * algo.presetRadius;
      treeAlgo.cache.bottomHeight = 0.2 * algo.presetSize;
      treeAlgo.cache.blindoutPercent = algo.presetRadius * 1.9;
      treeAlgo.cache.offyScaling = 0.1 * algo.presetSize;
      treeAlgo.cache.sharpness = 0.025 * algo.presetRadius;
      treeAlgo.cache.presetRadius = algo.presetRadius;
    }
    treeAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      if (treeAlgo.cache.presetRadius != algo.presetRadius) {
        treeAlgo.updateCache();
      }
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var factor = 0;

      var tips = 3;
      var realX = rx;
      var realY = ry;
      var realOffx = 1.0 * realX - 1.0 * algo.obj[i].x;
      var realOffy = 0.7 * (realY - algo.obj[i].y - treeAlgo.cache.offyScaling);
      var realDistance = Math.sqrt((realOffx * realOffx) + (realOffy * realOffy));
      var angle = geometryCalc.getAngle(realOffx, realOffy);
      angle = angle + Math.PI;
      angle = angle % algo.twoPi;
      var targetDistance = geometryCalc.getTargetDistance(angle, tips);
      var distPercent = realDistance / targetDistance;
      factor = util.blindoutPercent(1 - distPercent, treeAlgo.cache.sharpness);

      factor *= util.blindoutPercent(1 - Math.abs(offy) / treeAlgo.cache.blindoutPercent, 10);

      if (offy > treeAlgo.cache.bottomHeight) {
        realOffy = offy - treeAlgo.cache.bottomHeight;
        var distance = Math.sqrt((offx * offx) + 1);
        factor = Math.max(factor, 1 - (distance / treeAlgo.cache.bottomWidth));
      }
    
      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }
    treeAlgo.unused = function(i, rx, ry, h, s, v)
    {
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var factor = 0;

      if (ry === Math.floor(algo.obj[i].y) + algo.boxRadius + 1) {
        offy = 0;
      } else {
        offy += (algo.presetRadius) + 0.7;
      }
      factor = ((1 - (Math.sqrt((offx * offx * 1.8) + (offy * offy * 1.4)))) * 2.5) + offy * 3;
      factor = factor / 3.27;
 
      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    };

    var triangleAlgo = new Object;
    triangleAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      var tips = 3;
      var rotation = algo.twoPi * algo.obj[i].random;
      if (algo.presetSize < 10) {
          rotation = Math.PI;
      }
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var angle = (geometryCalc.getAngle(offx, offy) + rotation) % algo.twoPi;
      var targetDistance = geometryCalc.getTargetDistance(angle, tips);
      var distPercent = distance / targetDistance;
      var factor = util.blindoutPercent(1 - distPercent, 3);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var ufoAlgo = new Object;
    ufoAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      var offx = rx - algo.obj[i].x;
 
      var size = algo.presetSize * 2 / 5;
      var offy1 = ry - algo.obj[i].y;

      var factor1 = 1 - (Math.sqrt((offx * offx) + (offy1 * offy1)) / ((size / 2) + 1));
      if (factor1 < 0) {
        factor1 = 0;
      }

      var factor2 = 0;
      var offy2 = ry - algo.obj[i].y - size / 4;
      if (offy2 <= 0) {
        size = algo.presetSize;
        factor2 = 1 - (Math.sqrt((offx * offx) + (offy2 * offy2) * (size / 1.5)) / ((size / 2) + 1));
        if (factor2 < 0) {
          factor2 = 0;
        }
      }

      var factor = factor1 + factor2;
      if (factor > 1) {
        factor = 1;
      }

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    var ventilatorAlgo = new Object;
    ventilatorAlgo.drawPixel = function(i, rx, ry, h, s, v)
    {
      var factor = 1.0;
      var offx = rx - algo.obj[i].x;
      var offy = ry - algo.obj[i].y;
      var distance = Math.sqrt(offx * offx + offy * offy);
      var angle = geometryCalc.getAngle(offx, offy);
      var baseIntensity = 0.1;
      var percent = 0;

      var fanblades = 3;
      if (algo.presetSize >= 10) {
          fanblades = 5;
      }
      angle = fanblades * angle;
      angle -= (algo.twoPi) * (algo.progstep / 16 % 1);
      angle = (angle + algo.twoPi) % (algo.twoPi);

      percent = Math.max(0, 1 - (distance / (0.2 * algo.presetRadius)));
      var factorC = algo.halfPi * Math.acos(1 / (2.5 * percent + 1)) / (algo.halfPi);

      factor = Math.max(0, (1.0 + baseIntensity) * angle / (algo.twoPi) - baseIntensity);

      percent = Math.max(0, 1 - (angle / algo.twoPi));
      factor *= (1.0 - baseIntensity) * (Math.acos(1 / (2.5 * percent + 1)) / (algo.halfPi)) + baseIntensity;

      percent = Math.max(0, 1 - (distance / (algo.presetRadius)));
      factor *= algo.halfPi * Math.acos(1 / (2.5 * percent + 1)) / (algo.halfPi);
      
      factor = Math.max(factorC, factor * 1.5);

      var idx = (ry * algo.mapWidth + rx) * 3;
      util.addPixel(idx, h, s, HSVUtil.clamp01(factor) * v);
    }

    // Algorithm registration and methods ------------------------
    var shapes = new Object;
    shapes.collection = new Array(
      ["Ball", ballAlgo],
      ["Bell", bellAlgo],
      ["Candle", candleAlgo],
      ["Circle", circleAlgo],
      ["Diamond", diamondAlgo],
      ["Disk", diskAlgo],
      ["Eye", eyeAlgo],
      ["Flower", flowerAlgo],
      ["Heart", heartAlgo],
      ["Hexagon", hexagonAlgo],
      ["Mask", maskAlgo],
      ["Pentagon", pentagonAlgo],
      ["Ring", ringAlgo],
      ["Snowflake", snowflakeAlgo],
      ["Snowman", snowmanAlgo],
      ["Square", squareAlgo],
      ["Star", starAlgo],
      ["Steeringwheel", steeringwheelAlgo],
      ["Tornado", tornadoAlgo],
      ["Tree", treeAlgo],
      ["Triangle", triangleAlgo],
      ["Ufo", ufoAlgo],
      ["Ventilator", ventilatorAlgo]
    );
    shapes.makeSubArray = function(_index) {
      var _array = new Array();
      for (var i = 0; i < shapes.collection.length; i++) {
        _array.push(shapes.collection[parseInt(i)][parseInt(_index)]);
      }
      return _array;
    };
    shapes.getAlgoIndex = function(_name) {
      var idx = shapes.names.indexOf(_name);
      if (idx === -1) {
        idx = (shapes.collection.length - 1);
      }
      return idx;
    };
    shapes.getAlgoEntry = function(_index) {
      if (_index < 0) {
          _index = 0;
      }
      if (_index >= shapes.collection.length) {
        _index = (shapes.collection.length - 1);
      }
      return shapes.collection[parseInt(_index)];
    };
    shapes.getAlgoName = function(_index) {
      return shapes.getAlgoEntry(_index)[0];
    };
    shapes.getAlgoObject = function(_index) {
      return shapes.getAlgoEntry(_index)[1];
    };
    shapes.names = shapes.makeSubArray(0);

    // Setters and Getters ------------------------

    algo.presetSize = 5;
    algo.properties.push("name:presetSize|type:range|display:Size|values:5,40|write:setSize|read:getSize");
    algo.presetNumber = 3;
    algo.properties.push("name:presetNumber|type:range|display:Number|values:1,50|write:setNumber|read:getNumber");
    algo.presetRandom = 1;
    algo.properties.push("name:presetRandom|type:list|display:Random Colour|values:No,Yes|write:setRandom|read:getRandom");
    algo.presetCollision = 1;
    algo.properties.push("name:presetCollision|type:list|display:Self Collision|values:No,Yes|write:setCollision|read:getCollision");

    algo.selectedAlgo = shapes.getAlgoIndex("Trees");
    algo.properties.push("name:selectedAlgo|type:list|display:Shape|"
        + "values:" + shapes.names.toString() + "|"
        + "write:setSelectedAlgo|read:getSelectedAlgo");

    algo.presetRadius = algo.presetSize / 2;
    algo.halfRadius = algo.presetRadius / 2;
    algo.initialized = false;
    algo.boxRadius = 1;
    algo.realsize = 1;
    algo.progstep = 0;
    algo.totalSteps = 1024;
    algo.twoPi = 2 * Math.PI;
    algo.halfPi = Math.PI / 2;

    algo.setSize = function(_size)
    {
      algo.presetSize = _size;
      algo.presetRadius = _size / 2
      algo.halfRadius = _size / 4;
      algo.initialized = false;
    };

    algo.getSize = function()
    {
      return algo.presetSize;
    };

    algo.setNumber = function(_step)
    {
      algo.presetNumber = _step;
      algo.initialized = false;
    };

    algo.getNumber = function()
    {
      return algo.presetNumber;
    };

    algo.setRandom = function(_random)
    {
      if (_random === "Yes") {
        algo.presetRandom = 0;
      } else if (_random === "No") {
        algo.presetRandom = 1;
      }
      algo.initialized = false;
    };

    algo.getRandom = function()
    {
      if (algo.presetRandom === 0) {
        return "Yes";
      } else if (algo.presetRandom === 1) {
        return "No";
      }
    };

    algo.setCollision = function(_collision)
    {
      if (_collision === "Yes") {
        algo.presetCollision = 0;
      } else if (_collision === "No") {
        algo.presetCollision = 1;
      }
    };

    algo.getCollision = function()
    {
      if (algo.presetCollision === 0) {
        return "Yes";
      } else if (algo.presetCollision === 1) {
        return "No";
      }
    };

    algo.setSelectedAlgo = function(_name) {
        algo.selectedAlgo = shapes.getAlgoIndex(_name);
    };
    algo.getSelectedAlgo = function() {
      return shapes.getAlgoName(algo.selectedAlgo);
    };

    // General Purpose Functions ------------------

    geometryCalc.cache = {
      presetRadius: 0,
    };
    geometryCalc.updateCache = function(tips)
    {
      if (tips === 6) {
        geometryCalc.cache.innerRadius =
          0.866 * algo.presetRadius;
      } else if (tips === 5) {
        geometryCalc.cache.innerRadius =
          0.688 * algo.presetRadius / 0.851; 
      } else if (tips === 4) {
        geometryCalc.cache.innerRadius =
          algo.presetRadius * 0.707;
      } else { // tips === 3
        geometryCalc.cache.innerRadius =
          algo.halfRadius;
      }
      geometryCalc.cache.r =
        algo.twoPi / tips / 2;
      geometryCalc.cache.presetRadius = algo.presetRadius;
      geometryCalc.cache.tips = tips;
    }
    geometryCalc.getTargetDistance = function(angle, tips)
    {
      if (geometryCalc.cache.presetRadius != algo.presetRadius ||
        geometryCalc.cache.tips != tips) {
        geometryCalc.updateCache(tips);
      }
      
      var anglePart = (angle + geometryCalc.cache.r) % (algo.twoPi / tips)
        - geometryCalc.cache.r;
      var targetDistance = geometryCalc.cache.innerRadius /
        Math.cos(anglePart);

      return targetDistance;
    }

    // calculate the angle from 0 to 2 pi starting north and counting clockwise
    geometryCalc.getAngle = function(offx, offy)
    {
      var angle = 0;
      if (offx == 0) {
        if (offy < 0) {
          angle = -1 * Math.PI / 2;
        } else {
          angle = Math.PI / 2;
        }
      } else {
        var gradient = offy / offx;
        angle = Math.atan(gradient);
      }
      angle += Math.PI / 2;
      if (offx < 0) {
        angle += Math.PI;  
      }
      return angle;
    }
    
    // Utility functions --------------------------

    util.initialize = function(width, height)
    {
      algo.obj = new Array(algo.presetNumber);

      for (var i = 0; i < algo.presetNumber; i++) {
        algo.obj[i] = {
          x: Math.random() * (width - 1),
          y: Math.random() * (height - 1),
          random: Math.random(),
        };
        do {
          do {
            algo.obj[i].xDirection = (Math.random() * 2) - 1;
          } while (Math.abs(algo.obj[i].xDirection) < 0.1);
          do {
            algo.obj[i].yDirection = (Math.random() * 2) - 1;
          } while (Math.abs(algo.obj[i].yDirection) < 0.1);
        } while (Math.abs(algo.obj[i].xDirection + algo.obj[i].yDirection) < 0.3);
        // Random HSV colour for each object
        algo.obj[i].h = Math.random();
        algo.obj[i].s = 0.7 + Math.random() * 0.3;
        algo.obj[i].v = 0.7 + Math.random() * 0.3;
      }

      algo.boxRadius = Math.round(algo.presetRadius);
      algo.realsize = Math.floor(algo.presetRadius) * 2 + 1;

      algo.initialized = true;
      return;
    };

    // Additive HSV pixel blending
    util.addPixel = function(idx, h, s, v)
    {
      v = Math.max(0, Math.min(1, v));
      if (v <= 0) return;
      var existV = algo.map[idx + 2];
      if (existV <= 0) {
        algo.map[idx] = h;
        algo.map[idx + 1] = s;
        algo.map[idx + 2] = v;
      } else {
        if (v > existV) {
          algo.map[idx] = h;
          algo.map[idx + 1] = s;
        }
        algo.map[idx + 2] = Math.min(1, existV + v);
      }
    };

    // Blind out towards 0 percent
    util.blindoutPercent = function(percent, sharpness)
    {
      if (undefined === sharpness) {
        sharpness = 1;
      }
      if (percent < 0) {
        return 0;
      }
      percent = Math.min(1, percent);
      var factor = Math.min(1, Math.acos(1 /
        (Math.sqrt(sharpness * percent * percent) + 1)
      ) * algo.halfPi);
      return factor;
    }

    // Key Functions ------------------------------
    algo.rgbMap = function(width, height, rgb, progstep)
    {
      if (algo.initialized === false) {
        util.initialize(width, height);
      }
      algo.progstep = progstep;

      algo.map = HSVUtil.createMap(width, height);
      algo.mapWidth = width;
      algo.mapHeight = height;

      var shape = shapes.getAlgoObject(algo.selectedAlgo);
      for (var i = 0; i < algo.presetNumber; i++) {
        var mx = Math.floor(algo.obj[i].x);
        var my = Math.floor(algo.obj[i].y);

        var h = algo.obj[i].h;
        var s = algo.obj[i].s;
        var v = algo.obj[i].v;
        if (algo.presetRandom != 0) {
          h = algo.colors[0].h;
          s = algo.colors[0].s;
          v = algo.colors[0].v;
        }

        for (var ry = my - algo.boxRadius; ry < my + algo.boxRadius + 2; ry++) {
          for (var rx = mx - algo.boxRadius; rx < mx + algo.boxRadius + 2; rx++) {
            if (rx < width && rx > -1 && ry < height && ry > -1) {
              shape.drawPixel(i, rx, ry, h, s, v);
            }
          }
        }

        if (algo.presetCollision === 0) {
          for (var ti = 0; ti < algo.presetNumber; ti++) {
            if (ti !== i) {
              var disx = (algo.obj[i].x + algo.obj[i].xDirection) - algo.obj[ti].x;
              var disy = (algo.obj[i].y + algo.obj[i].yDirection) - algo.obj[ti].y;
              var dish = Math.sqrt((disx * disx) + (disy * disy));
              if (dish < (1.414) * (algo.presetRadius)) {
                var stepx = algo.obj[i].xDirection;
                var stepy = algo.obj[i].yDirection;
                algo.obj[i].xDirection = algo.obj[ti].xDirection;
                algo.obj[i].yDirection = algo.obj[ti].yDirection;
                algo.obj[ti].xDirection = stepx;
                algo.obj[ti].yDirection = stepy;
              }
            }
          }
        }

        if (algo.obj[i].y <= 0 && algo.obj[i].yDirection < 0) {
          algo.obj[i].yDirection *= -1;
        } else if (algo.obj[i].y >= height - 1 && algo.obj[i].yDirection > 0) {
          algo.obj[i].yDirection *= -1;
        }

        if (algo.obj[i].x <= 0 && algo.obj[i].xDirection < 0) {
          algo.obj[i].xDirection *= -1;
        } else if (algo.obj[i].x >= width - 1 && algo.obj[i].xDirection > 0) {
          algo.obj[i].xDirection *= -1;
        }

        algo.obj[i].x += algo.obj[i].xDirection;
        algo.obj[i].y += algo.obj[i].yDirection;
      }

      return algo.map;
    };

    algo.rgbMapStepCount = function(width, height)
    {
      return algo.totalSteps;
    };

    // Development tool access
    testAlgo = algo;

    return algo;
  }
)();
