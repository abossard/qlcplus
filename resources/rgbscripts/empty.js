/*
  Q Light Controller Plus
  empty.js

  Copyright (c) Your Name

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
    algo.apiVersion = 3;
    algo.name = "Script name";
    algo.author = "Your Name";
    algo.acceptColors = 2;
    algo.properties = new Array();

    algo.rgbMapSetColors = function(rawColors) { };
    algo.rgbMapGetColors = function() { return []; };

    algo.rgbMap = function(width, height, rgb, step)
    {
      var map = HSVUtil.createMap(width, height);
      var h = algo.colors[0].h, s = algo.colors[0].s, v = algo.colors[0].v;
      for (var y = 0; y < height; y++)
      {
        for (var x = 0; x < width; x++) {
          HSVUtil.setPixel(map, width, x, y, h, s, v);
        }
      }

      return map;
    };

    /**
      * Tells RGB Matrix how many steps this algorithm produces with size($width, $height)
      *
      * @param width The width of the map
      * @param height The height of the map
      * @return Number of steps required for a map of size($width, $height)
      */
    algo.rgbMapStepCount = function(width, height)
    {
      return width;
    };

    // Development tool access
    testAlgo = algo;

    return algo;
  }
)();
