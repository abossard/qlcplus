/*
  Q Light Controller Plus
  opposite.js

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
    algo.name = "Opposite";
    algo.author = "Massimo Callegari";
    algo.acceptColors = 2;
    algo.orientation = 0;
    algo.properties = new Array();
    algo.properties.push("name:orientation|type:list|display:Orientation|values:Horizontal,Vertical|write:setOrientation|read:getOrientation");

    algo.setOrientation = function(_orientation)
    {
      if (_orientation === "Vertical") { algo.orientation = 1; }
      else { algo.orientation = 0; }
    };

    algo.getOrientation = function()
    {
      if (algo.orientation === 1) { return "Vertical"; }
      else { return "Horizontal"; }
    };

    algo.rgbMap = function(width, height, rgb, step)
    {
      var map = RGBUtil.createMap(width, height);
      var h = algo.color.h, s = algo.color.s, v = algo.color.v;
      for (var y = 0; y < height; y++)
      {
        for (var x = 0; x < width; x++)
        {
          if (algo.orientation === 1)
          {
            if ((x % 2) === 0)
            {
              if (y === step) {
                RGBUtil.setPixel(map, width, x, y, h, s, v);
              }
            }
            else
            {
              if (y === ((height - 1) - step)) {
                RGBUtil.setPixel(map, width, x, y, h, s, v);
              }
            }
          }
          else
          {
            if ((y % 2) === 0)
            {
              if (x === step) {
                RGBUtil.setPixel(map, width, x, y, h, s, v);
              }
            }
            else
            {
              if (x === ((width - 1) - step)) {
                RGBUtil.setPixel(map, width, x, y, h, s, v);
              }
            }
          }
         }
      }

      return map;
    };

    algo.rgbMapStepCount = function(width, height)
    {
      if (algo.orientation === 0) { return width; }
      else { return height; }
    };

    // Development tool access
    testAlgo = algo;

    return algo;
  }
)();
