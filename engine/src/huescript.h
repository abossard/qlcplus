/*
  Q Light Controller Plus
  huescript.h

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

#ifndef HUESCRIPT_H
#define HUESCRIPT_H

#include <QStringList>
#include <functional>

#include "rgbscriptv4.h"

class AudioCapture;
class RGBMatrix;

/** @addtogroup engine Engine
 * @{
 */

/**
 * A script algorithm usable by a HUEMatrix.
 *
 * On top of RGBScript it adds:
 *  - the HSV contract: rgbMap() may return a flat Float32Array of
 *    width*height*3 floats in [0,1] which is converted to packed RGB here.
 *    Scripts that still return the upstream nested packed-uint arrays keep
 *    working, so a HUEMatrix can run every stock RGB script unchanged.
 *  - live audio input, injected as a 5th rgbMap() argument.
 *  - the algo.colors / algo.hasUserColors / algo.displayWidth|Height
 *    injected properties.
 */
class HUEScript : public RGBScript
{
public:
    HUEScript(Doc *doc);
    HUEScript(const HUEScript &s);
    ~HUEScript();

    /** @reimp */
    RGBAlgorithm *clone() const override;

    /**
     * Select the contract this script is expected to honour.
     *
     * true  -> rgbMap() returns a Float32Array of HSV triples and receives
     *          its colour arguments as {h,s,v} objects.
     * false -> upstream behaviour (nested packed-uint arrays, packed uint
     *          colour arguments).
     *
     * Set by HUEScriptsCache from the directory the script was found in.
     */
    void setHsvContract(bool hsv);
    bool hsvContract() const;

    /** @reimp */
    bool evaluate() override;

    /** @reimp */
    void rgbMap(const QSize &size, uint rgb, int step, RGBMap &map) override;

    /** @reimp */
    void rgbMapSetColors(const QVector<uint> &colors) override;

    /** @reimp */
    bool usesAudio() const override;

    /** @reimp */
    QStringList audioInputCategories() const override;

    /** @reimp */
    void setDisplaySize(const QSize &size) override;

    /** @reimp */
    void postRun() override;

    /**
     * Schedule a callable on the shared JS thread. Used to dispatch async
     * rgbMap pre-computations without blocking the MasterTimer thread, and
     * to defer deletion of script algorithms until in-flight tasks drained.
     *
     * Returns false when the JS thread is not running, in which case the
     * caller must fall back to a synchronous path. Safe from any thread.
     */
    static bool scheduleOnJSThread(std::function<void()> fn);

protected:
    /** Evaluate the shared HSV helper scripts into the engine global scope. */
    static void loadHsvShims();

    /** Read algo.usesAudio / algo.audioInputs after a successful evaluate(). */
    void parseAudioMetadata();

    /** Connect to AudioCapture and register for spectrum data */
    void setupAudioCapture();

    /** Disconnect from AudioCapture and release resources */
    void teardownAudioCapture();

    /** Build the JS object passed as the 5th rgbMap() argument */
    QJSValue buildAudioDataObject();

    /**
     * Inject the matrix colour palette as algo.colors (array of {h,s,v}) and
     * algo.hasUserColors.
     *
     * @param rgb fallback colour (packed 0xRRGGBB) used when the matrix has
     *            no valid colour stops.
     */
    void injectColors(uint rgb, RGBMatrix *matrix);

protected:
    bool m_hsvContract;                 //! Whether rgbMap() speaks HSV
    bool m_usesAudio;                   //! algo.usesAudio was declared
    QStringList m_audioInputCategories; //! Top-level audio.X categories used
    AudioCapture *m_audioInput;
    bool m_audioRegistered;
    bool m_hsvContractValidated;        //! Skip type checks after first frame
};

/** @} */

#endif // HUESCRIPT_H
