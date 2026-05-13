/*
  Q Light Controller Plus
  verticalfall.js

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
        algo.apiVersion = 1;
        algo.name = "Vertical fall";
        algo.author = "Massimo Callegari";
        algo.acceptColors = 1;

        var util = new Object;
        util.initialized = false;
        util.width = 0;
        util.height = 0;
        util.color = {h: 0, s: 0, v: 0};

        var fallObject = new Array();
        var objYPos = new Array();
        var objmap;

        util.initialize = function(width, height)
        {
            var ch = algo.colors[0].h, cs = algo.colors[0].s, cv = algo.colors[0].v;

            objYPos = new Array(width);
            for (var i = 0; i < width; i++) {
                objYPos[i] = -1;
            }

            fallObject = new Array(height);
            fallObject[0] = {h: ch, s: cs, v: cv};
            fallObject[height - 1] = {h: 0, s: 0, v: 0};
            for (var f = 1; f < height - 1; f++)
            {
                var factor = (height - f - 1) / height;
                fallObject[f] = {h: ch, s: cs, v: cv * factor};
            }

            objmap = HSVUtil.createMap(width, height);

            util.color = {h: ch, s: cs, v: cv};
            util.width = width;
            util.height = height;
            util.initialized = true;
        };

        util.getNextStep = function(width, height)
        {
            for (var x = 0; x < width; x++)
            {
                if (objYPos[x] === -1)
                {
                    // this decides the amount of falling objects
                    var seed = Math.floor(Math.random()*100);
                    if (seed > 80)
                    {
                        objYPos[x] = 0;
                    }
                }

                if (objYPos[x] >= 0)
                {
                    var yPos = objYPos[x];
                    for (var i = 0; i < height; i++)
                    {
                        if (yPos < height)
                        {
                            var fo = fallObject[i];
                            var i3 = (yPos * util.width + x) * 3;
                            objmap[i3] = fo.h;
                            objmap[i3 + 1] = fo.s;
                            objmap[i3 + 2] = fo.v;
                        }
                        yPos--;
                        if (yPos === -1) { break; }
                    }
                    objYPos[x]++;
                }
                if (objYPos[x] === height * 2)
                {
                    objYPos[x] = -1;
                }
            }
            return objmap;
        };

        algo.rgbMap = function(width, height, rgb, step)
        {
            if (util.initialized === false ||
                    util.color.h !== algo.colors[0].h || util.color.s !== algo.colors[0].s || util.color.v !== algo.colors[0].v ||
                    util.width != width || util.height != height) {
                util.initialize(width, height);
            }

            return util.getNextStep(width, height);
        };

        algo.rgbMapStepCount = function(width, height)
        {
            return 1;
        };

        // Development tool access
        testAlgo = algo;

        return algo;
    }
)();
