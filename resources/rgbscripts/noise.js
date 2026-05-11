/*
  Q Light Controller Plus
  Noise.js

  Copyright (c) Doug Puckett

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
        algo.name = "Noise";
        algo.author = "Doug Puckett";
        algo.properties = [];
        algo.acceptColors = 1;
        algo.noisePercentage = "High";
        var dCounter = 0;

        algo.properties.push("name:noisePercentage|type:list|display:Noise Coverage|values:Low,Medium,High|write:setAmount|read:getAmount");

        algo.setAmount = function (_amount) {
            algo.noisePercentage = _amount;
        };

        algo.getAmount = function () {
            return algo.noisePercentage;
        };

        algo.rgbMap = function (width, height, rgb, step)
        {
            var map = RGBUtil.createMap(width, height);

            for (var y = 0; y < height; y++)
            {
                for (var x = 0; x < width; x++)
                {
                    // Random brightness factor 0-1, applied to user color's value
                    var randV = algo.color.v * Math.random();

                    var vDiv = 0;

                    // setup for noise reduction :)
                    switch (algo.noisePercentage)
                    {
                        case "Low":
                            vDiv = Math.random() * 4 + 7;
                        break;
                        case "Medium":
                            vDiv = Math.random() * 5;
                        break;
                        case "High":
                            vDiv = 0;
                        break;
                    }

                    dCounter += 1;
                    if (dCounter >= vDiv) {
                        dCounter = 0;
                        RGBUtil.setPixel(map, width, x, y, algo.color.h, algo.color.s, randV);
                    }
                }
            }

            return map;
        };

        algo.rgbMapStepCount = function (width, height) {
            return width * height;
        };

        // Development tool access
        testAlgo = algo;

        return algo;
    }
)();
