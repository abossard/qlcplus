/*
  Q Light Controller Plus
  onebyone.js

  Copyright (c) Jano Svitok

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
    algo.name = "One By One";
    algo.author = "Jano Svitok";
    algo.acceptColors = 2;

    algo.properties = new Array();

    algo.rgbMap = function(width, height, rgb, step)
    {
        var map = RGBUtil.createMap(width, height);
        var xx = step % width;
        var yy = (step - xx) / width;
        RGBUtil.setPixel(map, width, xx, yy, algo.color.h, algo.color.s, algo.color.v);

        return map;
    };

    algo.rgbMapStepCount = function(width, height)
    {
        return width * height;
    };

    // Development tool access
    testAlgo = algo;

    return algo;
    }
)();
