/*
  Q Light Controller Plus
  rgbmatrix.h

  Copyright (c) Heikki Junnila
                Massimo Callegari

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

#ifndef RGBMATRIX_H
#define RGBMATRIX_H

#include <QElapsedTimer>
#include <QVector>
#include <QColor>
#include <QList>
#include <QSize>
#include <QPair>
#include <QMap>
#include <QMutex>
#include <QAtomicInteger>

#ifdef QT_QML_LIB
  #include "rgbscriptv4.h"
#else
  #include "rgbscript.h"
#endif
#include "function.h"

class FixtureGroup;
class GenericFader;
class FadeChannel;
class QDir;

/** @addtogroup engine_functions Functions
 * @{
 */

class RGBMatrixStep final
{
public:
    RGBMatrixStep();
    ~RGBMatrixStep() { }

public:
    /** Set/Get the current step index */
    void setCurrentStepIndex(int index);
    int currentStepIndex() const;

    /** Calculate the RGB components delta between $startColor and $endColor */
    void calculateColorDelta(const QColor& startColor, const QColor& endColor, const RGBAlgorithm *algorithm);

    /** Set/Get the final color of the next step to be reproduced */
    void setStepColor(QColor color);
    QColor stepColor() const;

    /** Update the color of the next step to be reproduced, considering the step index,
     *  the start color and the steps count */
    void updateStepColor(int step, QColor startColor, int stepsCount);

    /** Initialize the playback direction and set the initial step index and
      * color based on $startColor and $endColor */
    void initializeDirection(Function::Direction direction, const QColor& startColor, const QColor& endColor, int stepsCount, const RGBAlgorithm *algorithm);

    /** Check the steps progression based on $order and the internal m_direction.
     *  This method returns true if the RGBMatrix can continue to run, otherwise
     *  false is returned and the caller should stop the RGBMatrix */
    bool checkNextStep(Function::RunOrder order, QColor startColor, QColor endColor, int stepsNumber);

public:
    /** Matrix RGB data of the current step */
    RGBMap m_map;

private:
    /** The current direction of the steps playback */
    Function::Direction m_direction;
    /** The index of the algorithm step currently being reproduced */
    int m_currentStepIndex;
    /** The RGB color passed to the currently loaded algorithm */
    QColor m_stepColor;
    /** Color delta values of the RGB components between each step */
    int m_crDelta, m_cgDelta, m_cbDelta;
};

class RGBMatrix final : public Function
{
    Q_OBJECT
    Q_DISABLE_COPY(RGBMatrix)

   /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    RGBMatrix(Doc* parent);
    ~RGBMatrix();

    /** @reimp */
    QIcon getIcon() const override;

    enum MatrixAttribute
    {
        Color1Attr = Function::Intensity + 1,
        Color2Attr = Color1Attr + 1,
        Color3Attr = Color1Attr + 2,
        Color4Attr = Color1Attr + 3,
        Color5Attr = Color1Attr + 4,
        ColorLastAttr = Color1Attr + RGBAlgorithmColorDisplayCount - 1,
        PatternAttr = ColorLastAttr + 1,
        /** First index of the attributes dynamically registered by a Script algorithm. */
        ScriptPropertyAttr = PatternAttr + 1
    };
    enum { ColorAttributeCount = RGBAlgorithmColorDisplayCount };
    static_assert(RGBAlgorithmColorDisplayCount >= 5,
                  "RGBMatrix exposes Color1..Color5 compatibility attributes and requires at least 5 colors");

    /** Return the index of the currently selected algorithm. */
    int algorithmIndex() const;

    /** Re-apply style attributes (colors + pattern) to runtime state. */
    void applyStyleAttributes();

    /*********************************************************************
     * Contents
     *********************************************************************/
public:
    /** @reimp */
    void setTotalDuration(quint32 msec) override;

    /** @reimp */
    quint32 totalDuration() override;

    /** Set the matrix to control or not the dimmer channel */
    void setDimmerControl(bool dimmerControl);

    /** Get the matrix ability to control the dimmer channel */
    bool dimmerControl() const;

private:
    // LEGACY: replaced by ControlModeDimmer
    bool m_dimmerControl;

    /*********************************************************************
     * Copying
     *********************************************************************/
public:
    /** @reimp */
    virtual Function* createCopy(Doc* doc, bool addToDoc = true) override;

    /** @reimp */
    virtual bool copyFrom(const Function* function) override;

    /************************************************************************
     * Fixture Group
     ************************************************************************/
public:
    /** Get/Set the Fixture Group associated to this RGBMatrix */
    quint32 fixtureGroup() const;
    void setFixtureGroup(quint32 id);

    /** @reimp */
    QList<quint32> components() const override;

private:
    quint32 m_fixtureGroupID;
    FixtureGroup *m_group;

    /************************************************************************
     * Algorithm
     ************************************************************************/
public:
    /** Set the current RGB Algorithm. RGBMatrix takes ownership of the pointer. */
    void setAlgorithm(RGBAlgorithm* algo);

    /** Get the current RGB Algorithm. */
    RGBAlgorithm* algorithm() const;

    /** Get the algorithm protection mutex */
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    QMutex& algorithmMutex();
#else
    QRecursiveMutex& algorithmMutex();
#endif

    /** Get the number of steps of the current algorithm */
    int stepsCount() const;

    /** Get the preview of the current algorithm at the given step */
    void previewMap(int step, RGBMatrixStep *handler);

private:
    int algorithmStepsCount();

private:
    bool m_requestEngineCreation;
    RGBAlgorithm *m_runAlgorithm;
    RGBAlgorithm *m_algorithm;
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    QMutex m_algorithmMutex;
#else
    QRecursiveMutex m_algorithmMutex;
#endif

    /************************************************************************
     * Color
     ************************************************************************/
public:
    void setColor(int i, QColor c);
    QColor getColor(int i) const;
    QVector <QColor> getColors() const;

    void updateColorDelta();

    /** Set the colors of the current algorithm */
    void setMapColors(RGBAlgorithm *algorithm);

private:
    QVector<QColor> m_rgbColors;
    RGBMatrixStep *m_stepHandler;

    /************************************************************************
     * Properties
     ************************************************************************/
public:
    /** Set the value of the property with the given name */
    void setProperty(QString propName, QString value);

    /** Retrieve the value of the property with the given name */
    QString property(QString propName);

private:
    /** Return the properties of the currently loaded Script algorithm that
     *  are exposed as Function attributes. Index 0 of the returned list
     *  matches the attribute index $ScriptPropertyAttr */
    QList<RGBScriptProperty> scriptPropertyAttributes() const;

    /** Return the attribute name used to expose the given Script property */
    static QString scriptPropertyAttributeName(const RGBScriptProperty &prop);

    /** Register a Function attribute for every property exposed by the
     *  currently loaded Script algorithm, so that they can be controlled
     *  by a VC Slider in 'Adjust' mode */
    void registerScriptPropertyAttributes();

    /** Unregister the attributes of the currently loaded Script algorithm.
     *  To be called before replacing it, since the attribute names are
     *  retrieved from the algorithm itself */
    void unregisterScriptPropertyAttributes();

    /** Apply the value of a Script property attribute to the algorithm.
     *  $attrIndex is an index of $scriptPropertyAttributes */
    void applyScriptPropertyAttribute(int attrIndex, qreal value);

private:
    /** A map of the custom properties for this matrix */
    QMap<QString, QString>m_properties;

    /************************************************************************
     * Load & Save
     ************************************************************************/
public:
    /** @reimp */
    bool loadXML(QXmlStreamReader &root) override;

    /** @reimp */
    bool saveXML(QXmlStreamWriter *doc) const override;

    /************************************************************************
     * Running
     ************************************************************************/
public:
    /** @reimp */
    void tap() override;

    /** @reimp */
    void preRun(MasterTimer *timer) override;

    /** @reimp */
    void write(MasterTimer *timer, QList<Universe*> universes) override;

    /** @reimp */
    void postRun(MasterTimer *timer, QList<Universe*> universes) override;

private:
    /** Check what should be done when elapsed() >= duration() */
    void roundCheck();

    /** Same as roundCheck but assumes m_algorithmMutex is already held.
      * Returns true if the step actually advanced, false if the function
      * stopped (SingleShot end) or algorithm is null. */
    bool roundCheckLocked();

    /** Check if the engine needs to be re-created */
    void checkEngineCreation();

    QSharedPointer<GenericFader> getFader(Universe *universe);
    void updateFaderValues(FadeChannel &fc, uchar value, uint fadeTime, uint fadeOutTime);

    /** Update FadeChannels when $map has changed since last time */
    void updateMapChannels(const RGBMap& map, const FixtureGroup* grp, QList<Universe *> universes, int beatDuration);
    void applyColorAttribute(int colorIndex, qreal packedColor);
    void applyPatternAttribute(qreal patternIndex);

private:
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
     * Phase 4: Async pre-computation of next frame's RGBMap
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
private:
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

private slots:
    /** Slot connected to Doc fixture/group change signals. Marks the
     *  PixelPlan as dirty so it will be rebuilt on the next tick. */
    void invalidatePixelPlan(quint32 id = 0);

private:

public:
    /** Convert color values to fader value */
    static uchar rgbToGrey(uint col);

private:
    /** Reference to a timer counting the time in ms between steps */
    QElapsedTimer m_roundTime;

    /** The number of steps returned by the currently loaded algorithm */
    int m_stepsCount;

    /** The duration of a step based on the current BPM (Beats tempo only) */
    uint m_stepBeatDuration;

    /** Continuous phase (0.0 - 1.0) for accurate phase scaling during runtime speed changes.
     *  This prevents cumulative rounding errors when properties are changed multiple times.
     *  Analogous to EFX's m_currentAngle, but for RGBMatrix step-based animations. */
    double m_continuousPhase;

    bool m_applyingStyleAttributes;

    /*********************************************************************
     * Attributes
     *********************************************************************/
public:
    /** @reimp */
    int adjustAttribute(qreal fraction, int attributeId) override;

    /*************************************************************************
     * Blend mode
     *************************************************************************/
public:
    /** @reimp */
    void setBlendMode(Universe::BlendMode mode) override;

    /*************************************************************************
     * Control Mode
     *************************************************************************/
public:
    /** Control modes for the RGB Matrix */
    enum ControlMode
    {
        ControlModeRgb = 0,
        ControlModeWhite,
        ControlModeAmber,
        ControlModeUV,
        ControlModeDimmer,
        ControlModeShutter,
        ControlModeRgbw = 6,
        ControlModeRgbwBrighter = 7
    };

    /** Get/Set the control mode associated to this RGBMatrix */
    RGBMatrix::ControlMode controlMode() const;
    void setControlMode(RGBMatrix::ControlMode mode);

    /** Return a control mode from a string */
    static RGBMatrix::ControlMode stringToControlMode(QString mode);

    /** Return a string from a control mode, to be saved into a XML */
    static QString controlModeToString(RGBMatrix::ControlMode mode);

private:
    RGBMatrix::ControlMode m_controlMode;

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

private:
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

private:
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

private:
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

#endif
