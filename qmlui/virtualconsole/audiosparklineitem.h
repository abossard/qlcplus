/*
  Q Light Controller Plus
  audiosparklineitem.h

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

#ifndef AUDIOSPARKLINEITEM_H
#define AUDIOSPARKLINEITEM_H

#include <QQuickPaintedItem>
#include <QPointer>
#include <QVariantList>
#include <vector>
#include <array>
#include <cstdint>

#include "vcaudiotriggers.h"

/*
 * AudioSparklineItem
 *
 * C++ replacement for the Canvas2D sparkline used inside VCAudioTriggersItem.qml.
 *
 * Renders 16 audio "channels" (9 onset methods, 1 pitch, 4 spectral, 2 TSS) as
 * stacked sparklines in oscilloscope-style (sweep) mode. New samples overwrite
 * the column at the current write head; the rest of the FBO is preserved.
 *
 * Key properties:
 *   - source           : VCAudioTriggers backend; we connect to its
 *                        audioSnapshotChanged signal and pull display values.
 *   - pixelsPerSample  : horizontal spread of a single sample (default 3).
 *   - onsetFrac/pitchFrac/spectralFrac/tssFrac : section heights (0..1).
 *
 * Drawing model:
 *   - QQuickPaintedItem with FBO render target. The framebuffer is preserved
 *     between paint() calls, so an incremental "draw only the new strip"
 *     approach is correct -- we only need to repaint the columns that have
 *     been written since last paint, plus any column needed by the kick
 *     trail (5 px to the right of a kick gridline).
 *   - update(QRect) is used to limit Qt's invalidation rect to the new strip.
 *   - On layout / capacity / configuration change, m_needsFullRepaint is set;
 *     paint() repaints every populated column and re-draws gutter labels.
 *
 * Threading:
 *   - All work happens on the GUI thread (signals are queued/direct same-
 *     thread). No locks.
 */
class AudioSparklineItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(VCAudioTriggers* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int pixelsPerSample READ pixelsPerSample WRITE setPixelsPerSample NOTIFY pixelsPerSampleChanged)
    Q_PROPERTY(qreal onsetFrac    READ onsetFrac    WRITE setOnsetFrac    NOTIFY layoutChanged)
    Q_PROPERTY(qreal pitchFrac    READ pitchFrac    WRITE setPitchFrac    NOTIFY layoutChanged)
    Q_PROPERTY(qreal spectralFrac READ spectralFrac WRITE setSpectralFrac NOTIFY layoutChanged)
    Q_PROPERTY(qreal tssFrac      READ tssFrac      WRITE setTssFrac      NOTIFY layoutChanged)
    Q_PROPERTY(qreal triggerModeThreshold READ triggerModeThreshold WRITE setTriggerModeThreshold NOTIFY triggerModeThresholdChanged)
    Q_PROPERTY(QVariantList onsetMethodModes READ onsetMethodModes NOTIFY onsetMethodModesChanged)

public:
    explicit AudioSparklineItem(QQuickItem *parent = nullptr);
    ~AudioSparklineItem() override;

    // QQuickPaintedItem
    void paint(QPainter *painter) override;

    // Property accessors
    VCAudioTriggers *source() const { return m_source.data(); }
    void setSource(VCAudioTriggers *source);

    int pixelsPerSample() const { return m_pixelsPerSample; }
    void setPixelsPerSample(int v);

    qreal onsetFrac()    const { return m_onsetFrac; }
    qreal pitchFrac()    const { return m_pitchFrac; }
    qreal spectralFrac() const { return m_spectralFrac; }
    qreal tssFrac()      const { return m_tssFrac; }
    void setOnsetFrac(qreal v);
    void setPitchFrac(qreal v);
    void setSpectralFrac(qreal v);
    void setTssFrac(qreal v);

    qreal triggerModeThreshold() const { return m_triggerModeThreshold; }
    void setTriggerModeThreshold(qreal v);

    QVariantList onsetMethodModes() const;

    // Right-click handler hook from QML: flip method between trace (0)
    // and trigger-bar (1) display mode. Resets to all-trace on widget reload.
    Q_INVOKABLE void toggleOnsetMethodMode(int methodIdx);
    Q_INVOKABLE int  onsetMethodMode(int methodIdx) const;

    // Force-clear the ring buffer (e.g. when the user reconfigures history depth).
    Q_INVOKABLE void resetHistory();

signals:
    void sourceChanged();
    void pixelsPerSampleChanged();
    void layoutChanged();
    void triggerModeThresholdChanged();
    void onsetMethodModesChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private slots:
    void onSnapshot();
    void onSourceConfigChanged();
    void onSourceDestroyed();

private:
    // Constants
    static constexpr int kChannelCount   = 16;
    static constexpr int kOnsetCount     = 9;
    static constexpr int kLeftGutter     = 18;
    static constexpr int kRightMargin    = 2;
    static constexpr int kKickTrailWidth = 6;   // px to the right of a kick column

    // Source / connections
    QPointer<VCAudioTriggers> m_source;
    QMetaObject::Connection   m_snapshotConn;
    QMetaObject::Connection   m_configConn;
    QMetaObject::Connection   m_destroyConn;

    // Public properties
    int   m_pixelsPerSample      = 3;
    qreal m_onsetFrac            = 0.45;
    qreal m_pitchFrac            = 0.15;
    qreal m_spectralFrac         = 0.25;
    qreal m_tssFrac              = 0.15;
    qreal m_triggerModeThreshold = 0.45;

    // Per-onset-method display modes: 0 = trace, 1 = trigger bars.
    // QML-local state, not persisted. Reset on widget reload.
    std::array<uint8_t, kOnsetCount> m_onsetModes = {{0,0,0,0,0,0,0,0,0}};

    // Ring buffer storage (column-indexed). m_capacity = number of sample
    // columns that fit in the usable canvas width.
    int m_capacity     = 0;
    int m_nextWrite    = 0;     // next column to write
    int m_paintTo      = 0;     // first column not yet painted (== m_nextWrite once flushed)
    int m_sampleCount  = 0;     // number of columns ever filled (saturates at m_capacity)
    bool m_prevBeat    = false;
    bool m_prevKick    = false;

    std::vector<double>  m_history;       // kChannelCount * m_capacity
    std::vector<uint8_t> m_beatHistory;   // 0/1
    std::vector<uint8_t> m_kickHistory;   // 0..255 (alpha-encoded kick intensity)

    // Paint state
    bool m_needsFullRepaint = true;

    // Cached layout (computed in recomputeLayout)
    struct Layout {
        int x0 = kLeftGutter;
        int w  = 0;
        int yOnset = 0, hOnset = 0;
        int yPitch = 0, hPitch = 0;
        int ySpectral = 0, hSpectral = 0;
        int yTss = 0,   hTss = 0;
        int spectralRowH = 0;
        int tssRowH      = 0;
    } m_layout;

    // Helpers
    void rebuildConnections();
    void recomputeLayout();
    void resizeRing();
    int  columnX(int col) const { return m_layout.x0 + col * m_pixelsPerSample; }
    double sampleAt(int channel, int col) const
        { return m_history[channel * m_capacity + col]; }

    // Painting helpers
    void paintBackground(QPainter *p, const QRect &fullRect);
    void paintGutterLabels(QPainter *p);
    void paintColumn(QPainter *p, int col);
    void paintColumnBackgrounds(QPainter *p, int col,
                                const std::vector<int> &enabledOnsets, int lanes);
    void paintColumnTraces(QPainter *p, int col,
                           const std::vector<int> &enabledOnsets, int lanes);
    void paintColumnGridlines(QPainter *p, int col);

    static int onsetLaneCount(int enabledCount);
    std::vector<int> enabledOnsetMethods() const;
    static QColor onsetMethodColor(int idx);
    static const char *onsetMethodLetter(int idx);
};

#endif // AUDIOSPARKLINEITEM_H
