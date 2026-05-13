/*
  Q Light Controller Plus
  circles.js

  Copyright (c) Massimo Callegari
  with Additions by Branson Matheson 

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
    algo.name = "Circles";
    algo.author = "Massimo Callegari+Branson Matheson";
    algo.acceptColors = 2;
    algo.properties = new Array();
    algo.circlesAmount = 3;
    algo.properties.push("name:circlesAmount|type:range|display:Amount|values:1,10|write:setAmount|read:getAmount");
    algo.circlesSize = 0;
    algo.properties.push("name:circlesSize|type:range|display:Diameter(0=max(h,v))|values:0,32|write:setSize|read:getSize");
    algo.fadeMode = 0;
    algo.properties.push("name:fadeMode|type:list|display:Fade Mode|values:Don't Fade,Fade In,Fade Out,Pulse|write:setFade|read:getFade");
    algo.fillCircles = 0;
    algo.properties.push("name:fillCircles|type:list|display:Fill circles|values:No,Yes|write:setFill|read:getFill");

    var util = new Object;
    util.pixelMap = new Array();
    util.initialized = false;
    util.circlesMaxSize = 0;

    var circles = new Array();

    function Circle(x, y, step)
    {
      this.xCenter = x;
      this.yCenter = y;
      this.step = step;
      this.color = {h: 0, s: 0, v: 0};
    }

    algo.setAmount = function(_amount)
    {
      algo.circlesAmount = _amount;
      util.initialized = false;
    };

    algo.getAmount = function()
    {
      return algo.circlesAmount;
    };

    algo.setSize = function(_size)
    {
      algo.circlesSize = _size;
      util.initialized = false;
    };

    algo.getSize = function()
    {
      return algo.circlesSize;
    };

    algo.setFade = function(_fade)
    {
      if (_fade === "Fade In") { algo.fadeMode = 1; }
      else if (_fade === "Fade Out") { algo.fadeMode = 2; }
      else if (_fade === "Pulse") { algo.fadeMode = 3; }
      else { algo.fadeMode = 0; }
    };

    algo.getFade = function()
    {
      if (algo.fadeMode === 1) { return "Fade In"; }
      else if (algo.fadeMode === 2) { return "Fade Out"; }
      else if (algo.fadeMode === 3) { return "Pulse"; }
      else { return "Don't Fade"; }
    };

    algo.setFill = function (_fill) {
      if (_fill === "Yes") {
        algo.fillCircles = 1;
      } else {
        algo.fillCircles = 0;
      }
    };

    algo.getFill = function () {
      if (algo.fillCircles === 1) {
        return "Yes";
      } else {
        return "No";
      }
    };

    util.initialize = function(size)
    {
      if (size > 0) {
        util.circlesMaxSize = size;
      }

      circles = new Array();
      for (var i = 0; i < algo.circlesAmount; i++) {
        circles[i] = new Circle(-1, -1, 0);
      }

      util.initialized = true;
    };

    util.getStepColor = function(step, color)
    {
      if (algo.fadeMode === 0)
      {
        return {h: color.h, s: color.s, v: color.v};
      }
      else
      {
        var stepCount = Math.floor(util.circlesMaxSize / 2);
        var fadeStep = step;
        if ( algo.fadeMode === 2 ) {
          fadeStep = stepCount - step;
        } else if (algo.fadeMode == 3 && step >= stepCount/2) {
          fadeStep = stepCount - step + (stepCount/2) -1;
        } else if (algo.fadeMode == 3 && step < stepCount/2) {
          fadeStep = step + (stepCount/2) -1;
        }
        var factor = fadeStep / stepCount;
        return {h: color.h, s: color.s, v: color.v * factor};
      }
    };

    // from https://www.redblobgames.com/grids/circle-drawing/ 
    util.drawCircle = function(cx, cy, color, width, height, r) {
      ctop = Math.max(0, cy - r)
      cbottom = Math.min(height, cy + r )
      cleft = Math.max(0, cx - r)
      cright = Math.min(width, cx + r )
      for (py = ctop; py <= cbottom; py++) {
        for (px = cleft; px <= cright; px++ ) {
          dx = cx - px;
          dy = cy - py;
          distance_squared = (dx * dx) + (dy * dy);
          if ( distance_squared <= r*r-1 ) {  // -1 here so edges are clean
            util.drawPixel(px, py, color, width, height);
          }
        }
      }
    }

    util.drawPixel = function(cx, cy, color, width, height)
    {
      cx = Math.round(cx);
      cy = Math.round(cy);
      if (cx >= 0 && cx < width && cy >= 0 && cy < height) {
        var i = (cy * width + cx) * 3;
        util.pixelMap[i] = color.h;
        util.pixelMap[i + 1] = color.s;
        util.pixelMap[i + 2] = color.v;
      }
    };

    util.getNextStep = function(width, height)
    {
      var x = 0;
      var y = 0;
      // create an empty pixelMap (Float32Array, row-major, 3 floats per pixel)
      util.pixelMap = HSVUtil.createMap(width, height);

      for (var i = 0; i < algo.circlesAmount; i++)
      {
        if (circles[i].xCenter === -1)
        {
          circles[i].color = {h: algo.colors[0].h, s: algo.colors[0].s, v: algo.colors[0].v};
        }
        var color = util.getStepColor(circles[i].step, circles[i].color);
        //alert("Circle " + i + " xCenter: " + circles[i].xCenter + " color: " + color.toString(16));
        if (circles[i].xCenter === -1)
        {
          var seed = Math.floor(Math.random()*100);
          if (seed > 50) { continue; }
          circles[i].xCenter = Math.floor(Math.random() * width);
          circles[i].yCenter = Math.floor(Math.random() * height);
          var idx = (circles[i].yCenter * width + circles[i].xCenter) * 3;
          util.pixelMap[idx] = color.h;
          util.pixelMap[idx + 1] = color.s;
          util.pixelMap[idx + 2] = color.v;
        }
        else
        {
          var l = circles[i].step * Math.cos(Math.PI / 4);
          var radius2 = circles[i].step * circles[i].step;
          l = l.toFixed(0);

          if ( algo.fillCircles == 0 ) {
            for (x = 0; x <= l; x++)
            {
              y = Math.sqrt(radius2 - (x * x));

              util.drawPixel(circles[i].xCenter + x, circles[i].yCenter + y, color, width, height);
              util.drawPixel(circles[i].xCenter + x, circles[i].yCenter - y, color, width, height);
              util.drawPixel(circles[i].xCenter - x, circles[i].yCenter + y, color, width, height);
              util.drawPixel(circles[i].xCenter - x, circles[i].yCenter - y, color, width, height);

              util.drawPixel(circles[i].xCenter + y, circles[i].yCenter + x, color, width, height);
              util.drawPixel(circles[i].xCenter + y, circles[i].yCenter - x, color, width, height);
              util.drawPixel(circles[i].xCenter - y, circles[i].yCenter + x, color, width, height);
              util.drawPixel(circles[i].xCenter - y, circles[i].yCenter - x, color, width, height);
            }
          } else {
            util.drawCircle(circles[i].xCenter, circles[i].yCenter, color, width, height, circles[i].step)
          }
        } 

        circles[i].step++;
        if (circles[i].step >= (util.circlesMaxSize / 2))
        {
          circles[i].xCenter = -1;
          circles[i].yCenter = -1;
          circles[i].step = 0;
        }
      }

      return util.pixelMap;
    };

    algo.rgbMap = function(width, height, rgb, step)
    {
      if (util.initialized === false)
      {
        if ( algo.circlesSize > 0 ) {
          util.initialize(algo.circlesSize);
        } else if (height < width) {
          util.initialize(height);
        } else {
          util.initialize(width);
        }
      }

      return util.getNextStep(width, height);
    };

    algo.rgbMapStepCount = function(width, height)
    {
      return 2;
    };

    // Development tool access
    testAlgo = algo;

    return algo;
    }
)();
