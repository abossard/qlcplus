/*
  Q Light Controller Plus
  audiosparklineitem.cpp

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

#include "audiosparklineitem.h"
#include "vcaudiotriggers.h"

#include <QPainter>
#include <QColor>
#include <QFont>
#include <QFontMetrics>

#include <algorithm>
#include <cmath>

namespace
{
    // Background / structural colors
    const QColor kBgFill         ("#0a0a0a");
    const QColor kSectionFillEven("#0d0d0d");
    const QColor kSectionFillOdd ("#101010");
    const QColor kLaneSeparator  ("#1f1f1f");
    const QColor kLabelDim       ("#aaaaaa");
    const QColor kLabelDimmer    ("#666666");
    const QColor kLabelTr        ("#ff8888");
    const QColor kLabelSt        ("#88ffff");
    const QColor kBeatColor      ("#ffffff");
    const QColor kKickColor      (255, 153, 51);

    // 9 onset method colors (E, H, C, P, W, D, K, M, F)
    const QColor kOnsetColors[9] = {
        QColor("#ff3333"), QColor("#ff9900"), QColor("#ffdd33"),
        QColor("#33cc66"), QColor("#33ccff"), QColor("#9966ff"),
        QColor("#ff66cc"), QColor("#cc6600"), QColor("#66ffcc")
    };
    const char *kOnsetLetters[9] = { "E", "H", "C", "P", "W", "D", "K", "M", "F" };

    // Pitch / spectral / TSS colors (match QML)
    const QColor kPitchColor("#ffcc33");
    const QColor kSpectralColors[4] = {
        QColor("#66ccff"), QColor("#cc99ff"), QColor("#ff99cc"), QColor("#99ff99")
    };
    const char *kSpectralLabels[4] = { "Cen", "Flt", "Flx", "RMS" };
    const QColor kTssTransient("#ff6666");
    const QColor kTssSteady   ("#66ffff");

    inline double clamp01(double v)
    {
        if (v < 0.0) return 0.0;
        if (v > 1.0) return 1.0;
        return v;
    }

    inline double readListVal(const QVariantList &list, int idx)
    {
        if (idx < 0 || idx >= list.size()) return 0.0;
        bool ok = false;
        double v = list.at(idx).toDouble(&ok);
        if (!ok || std::isnan(v)) return 0.0;
        return v;
    }
}

AudioSparklineItem::AudioSparklineItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing, true);
    setMipmap(false);
    setAntialiasing(false);
    setFillColor(kBgFill);
    setOpaquePainting(true);
}

AudioSparklineItem::~AudioSparklineItem() = default;

// -----------------------------------------------------------------------------
// Property setters
// -----------------------------------------------------------------------------

void AudioSparklineItem::setSource(VCAudioTriggers *source)
{
    if (source == m_source.data())
        return;

    if (m_source)
    {
        disconnect(m_snapshotConn);
        disconnect(m_configConn);
        disconnect(m_destroyConn);
    }
    m_source = source;
    rebuildConnections();
    resetHistory();
    emit sourceChanged();
}

void AudioSparklineItem::setPixelsPerSample(int v)
{
    if (v < 1) v = 1;
    if (v == m_pixelsPerSample) return;
    m_pixelsPerSample = v;
    resizeRing();
    m_needsFullRepaint = true;
    update();
    emit pixelsPerSampleChanged();
}

void AudioSparklineItem::setOnsetFrac(qreal v)
{
    if (qFuzzyCompare(v + 1.0, m_onsetFrac + 1.0)) return;
    m_onsetFrac = v;
    recomputeLayout();
    m_needsFullRepaint = true;
    update();
    emit layoutChanged();
}

void AudioSparklineItem::setPitchFrac(qreal v)
{
    if (qFuzzyCompare(v + 1.0, m_pitchFrac + 1.0)) return;
    m_pitchFrac = v;
    recomputeLayout();
    m_needsFullRepaint = true;
    update();
    emit layoutChanged();
}

void AudioSparklineItem::setSpectralFrac(qreal v)
{
    if (qFuzzyCompare(v + 1.0, m_spectralFrac + 1.0)) return;
    m_spectralFrac = v;
    recomputeLayout();
    m_needsFullRepaint = true;
    update();
    emit layoutChanged();
}

void AudioSparklineItem::setTssFrac(qreal v)
{
    if (qFuzzyCompare(v + 1.0, m_tssFrac + 1.0)) return;
    m_tssFrac = v;
    recomputeLayout();
    m_needsFullRepaint = true;
    update();
    emit layoutChanged();
}

void AudioSparklineItem::setTriggerModeThreshold(qreal v)
{
    v = clamp01(v);
    if (qFuzzyCompare(v + 1.0, m_triggerModeThreshold + 1.0)) return;
    m_triggerModeThreshold = v;
    m_needsFullRepaint = true;
    update();
    emit triggerModeThresholdChanged();
}

QVariantList AudioSparklineItem::onsetMethodModes() const
{
    QVariantList l;
    l.reserve(kOnsetCount);
    for (uint8_t m : m_onsetModes)
        l.append(static_cast<int>(m));
    return l;
}

void AudioSparklineItem::toggleOnsetMethodMode(int methodIdx)
{
    if (methodIdx < 0 || methodIdx >= kOnsetCount) return;
    m_onsetModes[methodIdx] = (m_onsetModes[methodIdx] == 0) ? 1 : 0;
    m_needsFullRepaint = true;
    update();
    emit onsetMethodModesChanged();
}

int AudioSparklineItem::onsetMethodMode(int methodIdx) const
{
    if (methodIdx < 0 || methodIdx >= kOnsetCount) return 0;
    return m_onsetModes[methodIdx];
}

void AudioSparklineItem::resetHistory()
{
    resizeRing();
    std::fill(m_history.begin(), m_history.end(), 0.0);
    std::fill(m_beatHistory.begin(), m_beatHistory.end(), uint8_t(0));
    std::fill(m_kickHistory.begin(), m_kickHistory.end(), uint8_t(0));
    m_nextWrite   = 0;
    m_paintTo     = 0;
    m_sampleCount = 0;
    m_prevBeat    = false;
    m_prevKick    = false;
    m_needsFullRepaint = true;
    update();
}

// -----------------------------------------------------------------------------
// Geometry / layout / ring sizing
// -----------------------------------------------------------------------------

void AudioSparklineItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
    {
        recomputeLayout();
        resizeRing();
        m_needsFullRepaint = true;
        update();
    }
}

void AudioSparklineItem::recomputeLayout()
{
    const int W = qMax(1, qRound(width()));
    const int H = qMax(1, qRound(height()));
    m_layout.x0 = kLeftGutter;
    m_layout.w  = qMax(0, W - kLeftGutter - kRightMargin);

    int hOnset    = qRound(H * m_onsetFrac);
    int hPitch    = qRound(H * m_pitchFrac);
    int hSpectral = qRound(H * m_spectralFrac);
    int hTss      = H - hOnset - hPitch - hSpectral;
    if (hTss < 0) { hTss = 0; }

    m_layout.yOnset    = 0;
    m_layout.hOnset    = hOnset;
    m_layout.yPitch    = hOnset;
    m_layout.hPitch    = hPitch;
    m_layout.ySpectral = hOnset + hPitch;
    m_layout.hSpectral = hSpectral;
    m_layout.yTss      = hOnset + hPitch + hSpectral;
    m_layout.hTss      = hTss;
    m_layout.spectralRowH = hSpectral > 0 ? hSpectral / 4 : 0;
    m_layout.tssRowH      = hTss      > 0 ? hTss      / 2 : 0;
}

void AudioSparklineItem::resizeRing()
{
    int cap = m_layout.w / qMax(1, m_pixelsPerSample);
    if (cap < 1) cap = 1;
    if (cap == m_capacity && !m_history.empty()) return;

    m_capacity = cap;
    m_history.assign(size_t(kChannelCount) * size_t(cap), 0.0);
    m_beatHistory.assign(cap, 0);
    m_kickHistory.assign(cap, 0);
    m_nextWrite   = 0;
    m_paintTo     = 0;
    m_sampleCount = 0;
}

// -----------------------------------------------------------------------------
// Source connections
// -----------------------------------------------------------------------------

void AudioSparklineItem::rebuildConnections()
{
    if (!m_source) return;
    m_snapshotConn = connect(m_source.data(), &VCAudioTriggers::audioSnapshotChanged,
                             this, &AudioSparklineItem::onSnapshot);
    m_configConn   = connect(m_source.data(), &VCAudioTriggers::configChanged,
                             this, &AudioSparklineItem::onSourceConfigChanged);
    m_destroyConn  = connect(m_source.data(), &QObject::destroyed,
                             this, &AudioSparklineItem::onSourceDestroyed);
}

void AudioSparklineItem::onSourceConfigChanged()
{
    // Number of enabled onset methods / lane layout may have changed.
    m_needsFullRepaint = true;
    update();
}

void AudioSparklineItem::onSourceDestroyed()
{
    m_source.clear();
    resetHistory();
}

// -----------------------------------------------------------------------------
// Snapshot ingestion
// -----------------------------------------------------------------------------

void AudioSparklineItem::onSnapshot()
{
    if (!m_source || m_capacity <= 0) return;

    const int pos = m_nextWrite;

    // Channels 0..8 — onset descriptor display values (0..1).
    QVariantList onsList = m_source->onsetDescriptorDisplay();
    if (onsList.isEmpty())
        onsList = m_source->onsetDescriptorValues();
    for (int i = 0; i < kOnsetCount; ++i)
    {
        double v = clamp01(readListVal(onsList, i));
        m_history[size_t(i) * size_t(m_capacity) + size_t(pos)] = v;
    }
    // Channel 9 — pitch
    m_history[size_t(9) * size_t(m_capacity) + size_t(pos)]  = clamp01(m_source->pitchDisplay());
    // Channels 10..13 — spectral
    m_history[size_t(10) * size_t(m_capacity) + size_t(pos)] = clamp01(m_source->spectralCentroidDisplay());
    m_history[size_t(11) * size_t(m_capacity) + size_t(pos)] = clamp01(m_source->spectralFlatnessDisplay());
    m_history[size_t(12) * size_t(m_capacity) + size_t(pos)] = clamp01(m_source->fluxDisplay());
    m_history[size_t(13) * size_t(m_capacity) + size_t(pos)] = clamp01(m_source->rmsDisplay());
    // Channels 14..15 — TSS
    m_history[size_t(14) * size_t(m_capacity) + size_t(pos)] = clamp01(m_source->tssTransientLevel());
    m_history[size_t(15) * size_t(m_capacity) + size_t(pos)] = clamp01(m_source->tssSteadyLevel());

    // Beat / kick edge-triggered storage
    const bool bNow = m_source->beatActive();
    const bool kNow = m_source->kickFired();
    m_beatHistory[pos] = (bNow && !m_prevBeat) ? 1 : 0;
    double kv = clamp01(m_source->kickValue());
    m_kickHistory[pos] = (kNow && !m_prevKick) ? uint8_t(std::lround(255.0 * kv)) : 0;
    m_prevBeat = bNow;
    m_prevKick = kNow;

    m_nextWrite = (pos + 1) % m_capacity;
    if (m_sampleCount < m_capacity) ++m_sampleCount;

    // Compute dirty rect for incremental paint.
    // Cover the new column plus the kick trail width on the right so the
    // fading trail of a fresh kick gets drawn correctly.
    const int xStart = columnX(pos);
    const int xEnd   = columnX(pos) + m_pixelsPerSample + kKickTrailWidth;
    update(QRect(xStart, 0, xEnd - xStart + 1, qRound(height())));
}

// -----------------------------------------------------------------------------
// Painting
// -----------------------------------------------------------------------------

int AudioSparklineItem::onsetLaneCount(int n)
{
    if (n <= 0) return 0;
    if (n <= 3) return 1;
    if (n <= 6) return 2;
    return 3;
}

std::vector<int> AudioSparklineItem::enabledOnsetMethods() const
{
    std::vector<int> r;
    if (!m_source) return r;
    QVariantList l = m_source->onsetMethodsEnabled();
    r.reserve(kOnsetCount);
    for (int i = 0; i < kOnsetCount && i < l.size(); ++i)
        if (l.at(i).toBool())
            r.push_back(i);
    return r;
}

QColor AudioSparklineItem::onsetMethodColor(int idx)
{
    if (idx < 0 || idx >= 9) return Qt::white;
    return kOnsetColors[idx];
}

const char *AudioSparklineItem::onsetMethodLetter(int idx)
{
    if (idx < 0 || idx >= 9) return "?";
    return kOnsetLetters[idx];
}

void AudioSparklineItem::paint(QPainter *painter)
{
    if (m_capacity <= 0 || m_layout.w <= 0)
        return;

    const QRect fullRect(0, 0, qRound(width()), qRound(height()));
    painter->setRenderHint(QPainter::Antialiasing, false);

    const std::vector<int> enabledOnsets = enabledOnsetMethods();
    const int lanes = onsetLaneCount(int(enabledOnsets.size()));

    if (m_needsFullRepaint)
    {
        paintBackground(painter, fullRect);
        paintGutterLabels(painter);

        // Repaint every populated column. Iterate in chronological order so
        // line traces connect to the correct previous column.
        if (m_sampleCount > 0)
        {
            int firstCol = (m_sampleCount < m_capacity)
                           ? 0
                           : m_nextWrite; // oldest column
            for (int n = 0; n < m_sampleCount; ++n)
            {
                int col = (firstCol + n) % m_capacity;
                // Backgrounds first (per-section fill clears old content),
                // then traces & gridlines for this column.
                paintColumnBackgrounds(painter, col, enabledOnsets, lanes);
                paintColumnGridlines(painter, col);
                paintColumnTraces(painter, col, enabledOnsets, lanes);
            }
        }

        m_paintTo = m_nextWrite;
        m_needsFullRepaint = false;
        return;
    }

    // Incremental path: paint columns [m_paintTo, m_nextWrite) (mod capacity).
    if (m_paintTo == m_nextWrite)
        return; // nothing new

    int col = m_paintTo;
    while (col != m_nextWrite)
    {
        paintColumnBackgrounds(painter, col, enabledOnsets, lanes);
        paintColumnGridlines(painter, col);
        paintColumnTraces(painter, col, enabledOnsets, lanes);
        col = (col + 1) % m_capacity;
    }
    m_paintTo = m_nextWrite;
}

void AudioSparklineItem::paintBackground(QPainter *p, const QRect &fullRect)
{
    p->fillRect(fullRect, kBgFill);
}

void AudioSparklineItem::paintGutterLabels(QPainter *p)
{
    QFont f = p->font();
    f.setPixelSize(9);
    f.setFamily(QStringLiteral("sans-serif"));
    p->setFont(f);

    const std::vector<int> enabled = enabledOnsetMethods();
    const int lanes = onsetLaneCount(int(enabled.size()));

    // Onset lane letters
    if (lanes > 0 && m_layout.hOnset > 0)
    {
        int laneH = m_layout.hOnset / lanes;
        for (int li = 0; li < lanes; ++li)
        {
            QString letters;
            for (int mi = 0; mi < int(enabled.size()); ++mi)
            {
                if ((mi * lanes) / int(enabled.size()) == li)
                {
                    if (!letters.isEmpty()) letters += QChar(' ');
                    letters += QString::fromUtf8(onsetMethodLetter(enabled[mi]));
                }
            }
            p->setPen(kLabelDim);
            p->drawText(QRect(1, m_layout.yOnset + li * laneH + 1,
                              kLeftGutter - 2, laneH - 2),
                        Qt::AlignLeft | Qt::AlignTop, letters);
        }
    }
    else if (m_layout.hOnset > 0)
    {
        p->setPen(kLabelDimmer);
        p->drawText(QRect(1, m_layout.yOnset + 1, kLeftGutter - 2, 12),
                    Qt::AlignLeft | Qt::AlignTop, QStringLiteral("Ons"));
    }

    // Pitch label
    if (m_layout.hPitch > 0)
    {
        p->setPen(kLabelDim);
        p->drawText(QRect(1, m_layout.yPitch + 1, kLeftGutter - 2, 12),
                    Qt::AlignLeft | Qt::AlignTop, QStringLiteral("Pit"));
    }

    // Spectral labels
    if (m_layout.hSpectral > 0)
    {
        int rowH = m_layout.spectralRowH;
        for (int sr = 0; sr < 4; ++sr)
        {
            int rowY = m_layout.ySpectral + sr * rowH;
            p->setPen(kLabelDim);
            p->drawText(QRect(1, rowY + 1, kLeftGutter - 2, 12),
                        Qt::AlignLeft | Qt::AlignTop,
                        QString::fromUtf8(kSpectralLabels[sr]));
        }
    }

    // TSS labels (Tr / St)
    if (m_layout.hTss > 0)
    {
        p->setPen(kLabelTr);
        p->drawText(QRect(1, m_layout.yTss + 1, kLeftGutter - 2, 12),
                    Qt::AlignLeft | Qt::AlignTop, QStringLiteral("Tr"));
        p->setPen(kLabelSt);
        p->drawText(QRect(1, m_layout.yTss + m_layout.tssRowH + 1,
                          kLeftGutter - 2, 12),
                    Qt::AlignLeft | Qt::AlignTop, QStringLiteral("St"));
    }

    // Lane separator hairlines (drawn once per full repaint inside content area)
    if (lanes > 1)
    {
        int laneH = m_layout.hOnset / lanes;
        p->setPen(kLaneSeparator);
        for (int ls = 1; ls < lanes; ++ls)
        {
            int y = m_layout.yOnset + ls * laneH;
            p->drawLine(m_layout.x0, y, m_layout.x0 + m_layout.w - 1, y);
        }
    }
}

void AudioSparklineItem::paintColumnBackgrounds(QPainter *p, int col,
                                                const std::vector<int> &enabledOnsets,
                                                int lanes)
{
    const int x  = columnX(col);
    const int cw = m_pixelsPerSample;

    // Onset background
    if (m_layout.hOnset > 0)
        p->fillRect(QRect(x, m_layout.yOnset, cw, m_layout.hOnset), kSectionFillEven);

    // Pitch background
    if (m_layout.hPitch > 0)
        p->fillRect(QRect(x, m_layout.yPitch, cw, m_layout.hPitch), kSectionFillEven);

    // Spectral backgrounds (alternating shading per row, matching QML)
    if (m_layout.hSpectral > 0)
    {
        int rowH = m_layout.spectralRowH;
        for (int sr = 0; sr < 4; ++sr)
        {
            int rowY = m_layout.ySpectral + sr * rowH;
            int thisH = (sr == 3) ? (m_layout.ySpectral + m_layout.hSpectral - rowY) : rowH;
            p->fillRect(QRect(x, rowY, cw, thisH),
                        (sr % 2 == 0) ? kSectionFillEven : kSectionFillOdd);
        }
    }

    // TSS background
    if (m_layout.hTss > 0)
        p->fillRect(QRect(x, m_layout.yTss, cw, m_layout.hTss), kSectionFillEven);

    // Onset lane separators (per column slice)
    if (lanes > 1 && m_layout.hOnset > 0)
    {
        int laneH = m_layout.hOnset / lanes;
        p->setPen(kLaneSeparator);
        for (int ls = 1; ls < lanes; ++ls)
        {
            int y = m_layout.yOnset + ls * laneH;
            p->drawLine(x, y, x + cw - 1, y);
        }
    }

    Q_UNUSED(enabledOnsets);
}

void AudioSparklineItem::paintColumnGridlines(QPainter *p, int col)
{
    const int H = qRound(height());

    // Kick trail: this column may sit inside the fade trail of a kick that
    // happened up to kKickTrailWidth-1 columns earlier. Composite the strongest
    // alpha across the trail samples.
    uint8_t bestAlpha = 0;
    int bestOffset   = 0;
    for (int t = 0; t < kKickTrailWidth; ++t)
    {
        int srcCol = (col - t + m_capacity) % m_capacity;
        uint8_t ki = m_kickHistory[srcCol];
        if (ki == 0) continue;
        // Linear fade: full alpha at t==0, fades to near-zero at t==kKickTrailWidth-1
        double fade = 1.0 - (double(t) / double(kKickTrailWidth));
        uint8_t eff = uint8_t(std::lround(double(ki) * fade));
        if (eff > bestAlpha) { bestAlpha = eff; bestOffset = t; }
    }
    if (bestAlpha > 0)
    {
        QColor c = kKickColor;
        c.setAlpha(bestAlpha);
        // Head columns (offset 0) draw a 3-px wide stripe across the full
        // column slice for visual punch; trail columns draw 1 px wide.
        const int x = columnX(col);
        const int cw = (bestOffset == 0) ? qMax(m_pixelsPerSample, 3) : 1;
        p->fillRect(QRect(x, 0, cw, H), c);
    }

    // Beat: 1px white at the column itself
    if (m_beatHistory[col])
    {
        p->fillRect(QRect(columnX(col), 0, 1, H), kBeatColor);
    }
}

void AudioSparklineItem::paintColumnTraces(QPainter *p, int col,
                                           const std::vector<int> &enabledOnsets,
                                           int lanes)
{
    const int x  = columnX(col);
    const int xPrev = (col == 0) ? x : columnX(col - 1);
    const int prevCol = (col - 1 + m_capacity) % m_capacity;
    const bool hasPrev = (col != 0) && (m_sampleCount > 1);

    auto drawTrace = [&](int channel, int laneY, int laneH, const QColor &color)
    {
        if (laneH <= 0) return;
        double v = sampleAt(channel, col);
        if (v < 0.0) v = 0.0; else if (v > 1.0) v = 1.0;
        int y = laneY + laneH - int(std::lround(double(laneH) * v));
        if (y < laneY) y = laneY;
        if (y > laneY + laneH - 1) y = laneY + laneH - 1;
        if (hasPrev)
        {
            double vp = sampleAt(channel, prevCol);
            if (vp < 0.0) vp = 0.0; else if (vp > 1.0) vp = 1.0;
            int yp = laneY + laneH - int(std::lround(double(laneH) * vp));
            if (yp < laneY) yp = laneY;
            if (yp > laneY + laneH - 1) yp = laneY + laneH - 1;
            p->setPen(QPen(color, 1));
            p->drawLine(xPrev, yp, x, y);
        }
        else
        {
            p->fillRect(QRect(x, y, 1, 1), color);
        }
    };

    auto drawTriggerBar = [&](int channel, int laneY, int laneH, const QColor &color)
    {
        if (laneH <= 0) return;
        double v = sampleAt(channel, col);
        if (v >= m_triggerModeThreshold)
            p->fillRect(QRect(x, laneY, m_pixelsPerSample, laneH), color);
    };

    // -------- Onset section (1/2/3 lanes) --------
    if (lanes > 0 && m_layout.hOnset > 0)
    {
        int laneH = m_layout.hOnset / lanes;
        for (int mi = 0; mi < int(enabledOnsets.size()); ++mi)
        {
            int laneIdx = (mi * lanes) / int(enabledOnsets.size());
            int laneY = m_layout.yOnset + laneIdx * laneH;
            int ch    = enabledOnsets[mi];
            const QColor &c = onsetMethodColor(ch);
            if (m_onsetModes[ch] == 1)
                drawTriggerBar(ch, laneY, laneH, c);
            else
                drawTrace(ch, laneY, laneH, c);
        }
    }

    // -------- Pitch --------
    drawTrace(9, m_layout.yPitch, m_layout.hPitch, kPitchColor);

    // -------- Spectral (4 rows) --------
    if (m_layout.hSpectral > 0)
    {
        int rowH = m_layout.spectralRowH;
        for (int sr = 0; sr < 4; ++sr)
        {
            int rowY = m_layout.ySpectral + sr * rowH;
            int thisH = (sr == 3) ? (m_layout.ySpectral + m_layout.hSpectral - rowY) : rowH;
            drawTrace(10 + sr, rowY, thisH, kSpectralColors[sr]);
        }
    }

    // -------- TSS (transient + steady) --------
    if (m_layout.hTss > 0)
    {
        int tssH = m_layout.tssRowH;
        drawTrace(14, m_layout.yTss, tssH, kTssTransient);
        drawTrace(15, m_layout.yTss + tssH, m_layout.hTss - tssH, kTssSteady);
    }
}

void AudioSparklineItem::paintColumn(QPainter *p, int col)
{
    const std::vector<int> en = enabledOnsetMethods();
    int lanes = onsetLaneCount(int(en.size()));
    paintColumnBackgrounds(p, col, en, lanes);
    paintColumnGridlines(p, col);
    paintColumnTraces(p, col, en, lanes);
}
