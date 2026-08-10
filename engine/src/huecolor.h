/*
  Q Light Controller Plus
  huecolor.h

  Copyright (c) QLC+ contributors

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

#ifndef HUECOLOR_H
#define HUECOLOR_H

#include <QJSEngine>
#include <QJSValue>
#include <QColor>

#include <algorithm>
#include <cmath>

/** @addtogroup engine Engine
 * @{
 */

/**
 * HSV <-> packed RGB conversions shared by HUEScript and HUEMatrix.
 *
 * All components are floats in [0,1]; hue wraps.
 */
namespace HUEColor
{
    struct Hsv { float h, s, v; };

    /** Convert HSV (each in [0,1]) to packed 0xAARRGGBB via qRgb(). */
    inline uint hsvToRgb(float h, float s, float v)
    {
        if (!std::isfinite(h)) h = 0.0f;
        if (!std::isfinite(s)) s = 0.0f;
        if (!std::isfinite(v)) v = 0.0f;
        h = h - floorf(h);
        if (h < 0)
            h += 1.0f;
        s = qBound(0.0f, s, 1.0f);
        v = qBound(0.0f, v, 1.0f);

        float c = v * s;
        float x = c * (1.0f - fabsf(fmodf(h * 6.0f, 2.0f) - 1.0f));
        float m = v - c;
        float r, g, b;
        int sector = (int)(h * 6.0f) % 6;
        switch (sector)
        {
            case 0: r = c; g = x; b = 0; break;
            case 1: r = x; g = c; b = 0; break;
            case 2: r = 0; g = c; b = x; break;
            case 3: r = 0; g = x; b = c; break;
            case 4: r = x; g = 0; b = c; break;
            default: r = c; g = 0; b = x; break;
        }
        return qRgb(qRound((r + m) * 255.0f),
                    qRound((g + m) * 255.0f),
                    qRound((b + m) * 255.0f));
    }

    /** Convert packed 0xRRGGBB to HSV floats in [0,1]. */
    inline Hsv rgbToHsv(uint packed)
    {
        float r = float((packed >> 16) & 0xFF) / 255.0f;
        float g = float((packed >> 8) & 0xFF) / 255.0f;
        float b = float(packed & 0xFF) / 255.0f;
        float mx = std::max({r, g, b});
        float mn = std::min({r, g, b});
        float d = mx - mn;
        float h = 0.0f;
        float s = (mx == 0.0f) ? 0.0f : d / mx;
        if (d != 0.0f)
        {
            if (mx == r) h = fmodf((g - b) / d + 6.0f, 6.0f) / 6.0f;
            else if (mx == g) h = ((b - r) / d + 2.0f) / 6.0f;
            else h = ((r - g) / d + 4.0f) / 6.0f;
        }
        return {h, s, mx};
    }

    /** Marshal an Hsv into a QJSValue {h,s,v} object. */
    inline QJSValue hsvToJs(QJSEngine *engine, const Hsv &hsv)
    {
        QJSValue obj = engine->newObject();
        obj.setProperty(QStringLiteral("h"), QJSValue(double(hsv.h)));
        obj.setProperty(QStringLiteral("s"), QJSValue(double(hsv.s)));
        obj.setProperty(QStringLiteral("v"), QJSValue(double(hsv.v)));
        return obj;
    }
}

/** @} */

#endif // HUECOLOR_H
