/*
  Q Light Controller Plus
  huematrix.h

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

#ifndef HUEMATRIX_H
#define HUEMATRIX_H

#include <QAtomicInteger>
#include <QMutex>
#include <QSize>

#include "rgbmatrix.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * A matrix function that inherits everything RGBMatrix does and adds the
 * fork-specific rendering pipeline on top of it:
 *
 *  - HSV script contract (via HUEScript) plus the HSV script library
 *  - rotation / mirror with selectable blend
 *  - beat-driven transforms
 *  - a post-render brightness multiplier
 *  - the RGBW and RGBW-brighter control modes
 *  - a pre-resolved pixel plan and async rgbMap pre-computation
 *
 * RGBMatrix itself is kept byte-identical to upstream, so write() is fully
 * overridden here rather than hooked into the base implementation. Roughly
 * 35 lines of the write() preamble are therefore duplicated by design.
 */
class HUEMatrix : public RGBMatrix
{
    Q_OBJECT
    Q_DISABLE_COPY(HUEMatrix)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    HUEMatrix(Doc *doc);
    ~HUEMatrix();

    /** @reimp */
    QIcon getIcon() const override;

    /**
     * Names of every algorithm a HUEMatrix can run: the built-in ones, the
     * upstream RGB scripts and the HSV-contract HUE scripts.
     */
    static QStringList availableAlgorithms(Doc *doc);

    /** Instantiate an algorithm by name. Returns NULL when $name is unknown. */
    static RGBAlgorithm *createAlgorithm(Doc *doc, const QString &name);

    /** Load an algorithm from XML, resolving scripts through the HUE cache. */
    static RGBAlgorithm *algorithmLoader(Doc *doc, QXmlStreamReader &root);

    /** @reimp */
    void setTotalDuration(quint32 msec) override;

    /** @reimp */
    quint32 totalDuration() override;

    /** @reimp */
    void setDimmerControl(bool dimmerControl) override;

    /*********************************************************************
     * Copying
     *********************************************************************/
public:
    /** @reimp */
    Function *createCopy(Doc *doc, bool addToDoc = true) override;

    /** @reimp */
    bool copyFrom(const Function *function) override;

    /*********************************************************************
     * Fixture Group / Algorithm
     *********************************************************************/
public:
    /** Set the fixture group and invalidate the pixel plan / precomputed map */
    void setFixtureGroup(quint32 id) override;

    /** Set the algorithm and invalidate the pixel plan / precomputed map */
    void setAlgorithm(RGBAlgorithm *algo) override;

    /** @reimp */
    void previewMap(int step, RGBMatrixStep *handler) override;

protected:
    /** @reimp */
    int algorithmStepsCount() override;

public:

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    /** @reimp */
    bool loadXML(QXmlStreamReader &root) override;

    /** @reimp */
    bool saveXML(QXmlStreamWriter *doc) const override;

    /*********************************************************************
     * Running
     *********************************************************************/
public:
    /** @reimp */
    void preRun(MasterTimer *timer) override;

    /** @reimp */
    void write(MasterTimer *timer, QList<Universe*> universes) override;

    /** @reimp */
    void postRun(MasterTimer *timer, QList<Universe*> universes) override;

protected:
    /** @reimp */
    void roundCheck() override;

    /** Same as roundCheck but assumes m_algorithmMutex is already held.
      * Returns true if the step actually advanced, false if the function
      * stopped (SingleShot end) or algorithm is null. */
    bool roundCheckLocked();

    /** Advance the elapsed time/beat counters by one tick and, when the step
      * is due, move to the next one. Writes the pre-increment elapsed time to
      * $prevElapsed so the caller can detect the first tick of a step.
      * Returns true if the step actually advanced.
      * Assumes m_algorithmMutex is held. */
    bool advanceStep(MasterTimer *timer, quint32 &prevElapsed);

    /** Track the position within the musical bar (m_currentBeat) and latch a
      * new random segment on beat changes. Returns the number of beats per bar
      * to use for the beat transform. No-op when the beat effect is off.
      * Assumes m_algorithmMutex is held. */
    int updateBeatPhase(MasterTimer *timer);

    /** Try to move a matching pre-computed frame into m_stepHandler->m_map.
      * A frame only matches if it was produced for the same generation,
      * algorithm, step, colour and size. A non-matching frame is discarded.
      * Returns true if the map was filled from the pre-computed frame. */
    bool consumePrecomputedMap(const QSize &algoSize, uint stepColor,
                               int stepIndex, quint32 generation);

    QSharedPointer<GenericFader> getFader(Universe *universe);
    void updateFaderValues(FadeChannel &fc, uchar value, uint fadeTime, uint fadeOutTime);

    /** Update FadeChannels when $map has changed since last time */
    void updateMapChannels(const RGBMap &map, const FixtureGroup *grp,
                           QList<Universe *> universes, int beatDuration);

    /*********************************************************************
     * Pixel plan
     *********************************************************************/
protected:
    /** Pre-computed channel writes for one fixture-head pixel.
     *  See rebuildPixelPlan() for invalidation triggers. */
    enum PixelValueSource : quint8
    {
        VS_Red, VS_Green, VS_Blue,
        VS_Cyan, VS_Magenta, VS_Yellow,
        VS_White,           ///< min(R,G,B)
        VS_RedSubW, VS_GreenSubW, VS_BlueSubW,
        VS_Grey,            ///< rgbToGrey(col)
        VS_GreyOrFull       ///< 0 if grey == 0 else 255
    };

    struct PixelPlanEntry
    {
        quint16 x;
        quint16 y;
        quint32 universeIndex;
        quint32 fixtureID;
        quint32 channel;            ///< channel number relative to fixture
        PixelValueSource source;
    };

    /** Flat, pre-resolved list of channel writes for the current fixture group
     *  + control mode. Avoids per-tick fixture/head/channel lookups. */
    QVector<PixelPlanEntry> m_pixelPlan;

    /** Cached pointer used to detect that the resolved FixtureGroup changed
     *  (in addition to the dirty flag). */
    const FixtureGroup *m_pixelPlanGroup = nullptr;

    /** When non-zero, m_pixelPlan must be rebuilt before use. Marked from
     *  fixture / fixture-group / control-mode changes. Atomic so that signals
     *  from the main thread can flip it while the MasterTimer thread reads. */
    QAtomicInteger<int> m_pixelPlanDirty;

    /** Rebuild m_pixelPlan from $grp using the current control mode and
     *  dimmer-control flag. Caller must hold m_algorithmMutex. */
    void rebuildPixelPlan(const FixtureGroup *grp);

    /*********************************************************************
     * Async pre-computation of next frame's RGBMap
     *
     * The JS rgbMap() call dominates the MasterTimer budget. We pre-compute
     * the *next* frame's pixel data on the shared JSThread at the END of the
     * current write(), so that by the time the MasterTimer reaches us again
     * the result is already waiting in m_precomputedMap.
     *
     * Validity is gated by:
     *   - m_precomputedReady flag (atomic): producer/consumer handshake.
     *   - Generation counter: invalidated on algorithm/group/control mode
     *     changes that would render a precomputed map unsafe or wrong.
     *   - Per-frame match: step index, step color, algorithm size and the
     *     algorithm pointer must equal what the consuming write() needs.
     *
     * If any check fails, we fall back to the synchronous rgbMap() path —
     * correctness is preserved, only the perf win is missed.
     *********************************************************************/
protected:
    /** Upper bound, in milliseconds, on how long ~HUEMatrix() waits for an
     *  in-flight async precompute to drain. Exceeding it means the JS thread
     *  went away before the queued job ran, so the flag will never clear. */
    static const int precomputeDrainTimeoutMs = 2000;

    /** Pre-computed pixel map for the next frame. Protected by
     *  m_precomputedMutex. */
    RGBMap m_precomputedMap;

    /** Guards the m_precomputed* fields below. Cheap (uncontended) — the
     *  only writer is the JSThread async task; the only reader is the
     *  MasterTimer thread once per tick. */
    QMutex m_precomputedMutex;

    /** 0 = no precomputed frame available, 1 = ready to consume. */
    QAtomicInteger<int> m_precomputedReady;

    /** Set to 1 while a JSThread async task is queued or running for this
     *  matrix. Used to throttle (one outstanding task at a time) and to let
     *  the destructor wait for completion before tearing down. */
    QAtomicInteger<int> m_precomputedInFlight;

    /** Bumped on any change that invalidates an in-flight or stored
     *  precomputed map (setAlgorithm, setFixtureGroup, setControlMode,
     *  preRun). Color changes do NOT bump — they are validated per-frame
     *  via m_precomputedColor instead, so rapid color attribute fades
     *  don't thrash the cache. */
    QAtomicInteger<quint32> m_currentGeneration;

    /** Generation snapshot taken at kick-off; compared against
     *  m_currentGeneration before storing/consuming the precomputed map. */
    quint32 m_precomputedGeneration;

    /** Step index, step color and algorithm size the precomputed map was
     *  rendered for. The consuming write() requires these to match its
     *  intended frame parameters. */
    int m_precomputedStep;
    uint m_precomputedColor;
    QSize m_precomputedAlgoSize;

    /** Algorithm pointer the precomputed map was rendered against.
     *  Compared on consume to detect setAlgorithm() races that the
     *  generation counter alone might miss. */
    RGBAlgorithm *m_precomputedAlgorithm;

    /** Kick a non-blocking pre-computation of the next frame's rgbMap on
     *  the JSThread. Caller must hold m_algorithmMutex. No-op if the
     *  algorithm isn't a Script (other algorithms are fast and don't
     *  benefit) or if a task is already in flight. */
    void kickAsyncRgbMap(RGBAlgorithm *algo, const QSize &algoSize,
                         uint stepColor, int step, quint32 generation);

    /** Delete an algorithm pointer safely. Script-typed algorithms are
     *  deleted via the JSThread to drain any pending precompute tasks
     *  (FIFO ordering) before destruction. Other types are deleted inline. */
    static void deferDeleteAlgorithm(RGBAlgorithm *algo);

protected slots:
    /** Slot connected to Doc fixture/group change signals. Marks the
     *  PixelPlan as dirty so it will be rebuilt on the next tick. */
    void invalidatePixelPlan(quint32 id = 0);

    /*************************************************************************
     * Control Mode
     *************************************************************************/
public:
    /** Set the control mode, invalidating the pixel plan */
    void setControlMode(RGBMatrix::ControlMode mode) override;

    /** Return a control mode from a string, including the RGBW modes */
    static RGBMatrix::ControlMode stringToControlMode(QString mode);

    /** Return a string from a control mode, including the RGBW modes */
    static QString controlModeToString(RGBMatrix::ControlMode mode);

    /*************************************************************************
     * Rotation & Mirror
     *************************************************************************/
public:
    /** Mirror blend algorithms */
    enum MirrorBlend
    {
        MirrorFlip = 0,    ///< Pure reflection (copy one half to other)
        MirrorMax,         ///< qMax per channel (LedFX-style bloom)
        MirrorAverage,     ///< (a + b) / 2 per channel
        MirrorAdditive     ///< min(255, a + b) per channel
    };

    /** Get/Set rotation: 0=0°, 1=90°CW, 2=180°, 3=270°CW */
    int rotation() const;
    void setRotation(int r);

    /** Get/Set mirror: 0=off, 1=horizontal, 2=vertical, 3=both */
    int mirror() const;
    void setMirror(int m);

    /** Get/Set mirror blend algorithm */
    MirrorBlend mirrorBlend() const;
    void setMirrorBlend(MirrorBlend b);

    /** Return the effective algorithm size, accounting for 90/270 rotation */
    QSize effectiveAlgorithmSize() const;

    static QString mirrorBlendToString(MirrorBlend b);
    static MirrorBlend stringToMirrorBlend(const QString &s);

    /** Apply rotation and mirror transforms to a rendered RGB map.
     *  srcSize = the size the algorithm was given (may be swapped for 90/270).
     *  dstSize = the physical fixture group size. */
    static void applyTransforms(RGBMap &map, const QSize &srcSize, const QSize &dstSize,
                                int rotation, int mirror, MirrorBlend blend);

protected:
    /** Return effective algorithm size for a given fixture group */
    QSize effectiveAlgorithmSize(const FixtureGroup *grp) const;

    int m_rotation;
    int m_mirror;
    MirrorBlend m_mirrorBlend;

    /*************************************************************************
     * Brightness multiplier
     *************************************************************************/
public:
    /** Get/Set the post-render brightness multiplier (>= 0.0, 1.0 = unity) */
    qreal brightness() const;
    void setBrightness(qreal b);

protected:
    qreal m_brightness;

    /*************************************************************************
     * Beat Transform
     *************************************************************************/
public:
    enum BeatEffect
    {
        BeatEffectOff = 0,
        BeatEffectMirror,
        BeatEffectColorInvert,
        BeatEffectBlackout,
        BeatEffectWhiteout
    };

    enum BeatSelection
    {
        BeatSelAllOnDownbeat = 0,
        BeatSelWalk,
        BeatSelRandom
    };

    enum BeatOrientation
    {
        BeatOrientRows = 0,
        BeatOrientColumns
    };

    BeatEffect beatEffect() const;
    void setBeatEffect(BeatEffect e);

    BeatSelection beatSelection() const;
    void setBeatSelection(BeatSelection s);

    BeatOrientation beatOrientation() const;
    void setBeatOrientation(BeatOrientation o);

    static QString beatEffectToString(BeatEffect e);
    static BeatEffect stringToBeatEffect(const QString &s);
    static QString beatSelectionToString(BeatSelection s);
    static BeatSelection stringToBeatSelection(const QString &s);
    static QString beatOrientationToString(BeatOrientation o);
    static BeatOrientation stringToBeatOrientation(const QString &s);

protected:
    void applyBeatTransform(RGBMap &map, int currentBeat, int beatsPerBar);

    static void segmentRange(int segment, int total, int segmentsCount, int &start, int &end);

    BeatEffect m_beatEffect;
    BeatSelection m_beatSelection;
    BeatOrientation m_beatOrientation;
    int m_currentBeat;
    int m_lastBeat;
    int m_randomSegment;
};

/** @} */

#endif // HUEMATRIX_H
