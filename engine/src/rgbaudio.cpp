/*
  Q Light Controller Plus
  rgbaudio.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include "rgbaudio.h"
#include "audiocapture.h"
#include "audiosnapshot.h"
#include "doc.h"
#include <cmath>

RGBAudio::RGBAudio(Doc * doc)
    : RGBAlgorithm(doc)
    , m_audioInput(NULL)
{
}

RGBAudio::RGBAudio(const RGBAudio& a, QObject *parent)
    : QObject(parent)
    , RGBAlgorithm(a.doc())
    , m_audioInput(NULL)
{
}

RGBAudio::~RGBAudio()
{
    postRun();
}

RGBAlgorithm* RGBAudio::clone() const
{
    RGBAudio* audio = new RGBAudio(*this);
    return static_cast<RGBAlgorithm*> (audio);
}

void RGBAudio::calculateColors(int barsHeight)
{
    if (barsHeight > 0)
    {
        QColor startColor = getColor(0);
        QColor endColor = getColor(1);
        m_barColors.clear();
        if (!startColor.isValid())
            return;
        if (endColor == QColor()
            || barsHeight == 1) // to avoid division by 0 below
        {
            for (int i = 0; i < barsHeight; i++)
                m_barColors.append(startColor.rgb());
        }
        else
        {
            int crDelta = (endColor.red() - startColor.red()) / (barsHeight - 1);
            int cgDelta = (endColor.green() - startColor.green()) / (barsHeight - 1);
            int cbDelta = (endColor.blue() - startColor.blue()) / (barsHeight - 1);
            QColor pixelColor = startColor;

            for (int i = 0; i < barsHeight; i++)
            {
                m_barColors.append(pixelColor.rgb());
                pixelColor = QColor(pixelColor.red() + crDelta,
                                    pixelColor.green() + cgDelta,
                                    pixelColor.blue() + cbDelta);
            }
        }
    }
}

/****************************************************************************
 * RGBAlgorithm
 ****************************************************************************/

int RGBAudio::rgbMapStepCount(const QSize& size)
{
    Q_UNUSED(size);
    return 1;
}

void RGBAudio::rgbMapSetColors(const QVector<uint> &colors)
{
    Q_UNUSED(colors);
}

QVector<uint> RGBAudio::rgbMapGetColors()
{
    return QVector<uint>();
}

void RGBAudio::rgbMap(const QSize& size, uint rgb, int step, RGBMap &map)
{
    Q_UNUSED(step);

    rgbMapWithAudio(size, rgb, map, resolveAudio());
}

AudioRenderView RGBAudio::resolveAudio()
{
    if (m_audioInput == nullptr && doc() != nullptr)
    {
        m_audioInput = doc()->audioInputCapture().data();
        if (m_audioInput != nullptr)
            m_audioInput->registerBandsNumber(1);
    }
    return AudioRenderView::fromSnapshot(
        doc() ? doc()->audioSnapshot() : AudioSnapshot(), AudioRenderView::nowNs());
}

void RGBAudio::rgbMapWithAudio(const QSize &size, uint rgb, RGBMap &map,
                              const AudioRenderView &audio)
{
    QMutexLocker locker(&m_mutex);
    if (size.isEmpty())
        return;
    map.resize(size.height());
    for (int y = 0; y < size.height(); y++)
    {
        map[y].resize(size.width());
        map[y].fill(0);
    }

    if (m_barColors.count() != size.height())
        calculateColors(size.height());

    for (int x = 0; x < size.width(); x++)
    {
        const int barHeight = int(std::ceil(audioSpectrum(audio, x, size.width()) * size.height()));
        for (int y = size.height() - barHeight; y < size.height(); y++)
        {
            if (m_barColors.count() == 0)
                map[y][x] = rgb;
            else
                map[y][x] = m_barColors.at(y);
        }
    }
}

void RGBAudio::postRun()
{
    QMutexLocker locker(&m_mutex);

    if (m_audioInput != nullptr)
        m_audioInput->unregisterBandsNumber(1);
    m_audioInput = NULL;
}

QString RGBAudio::name() const
{
    return QString("Audio Spectrum");
}

QString RGBAudio::author() const
{
    return QString("Massimo Callegari");
}

int RGBAudio::apiVersion() const
{
    return 1;
}

void RGBAudio::setColors(QVector<QColor> colors)
{
    RGBAlgorithm::setColors(colors);

    // invalidate bars colors so the next time a rendering is
    // required, it will be filled with the right values
    m_barColors.clear();
}

RGBAlgorithm::Type RGBAudio::type() const
{
    return RGBAlgorithm::Audio;
}

int RGBAudio::acceptColors() const
{
    return 2; // start and end colors accepted
}

bool RGBAudio::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCRGBAlgorithm)
    {
        qWarning() << Q_FUNC_INFO << "RGB Algorithm node not found";
        return false;
    }

    if (root.attributes().value(KXMLQLCRGBAlgorithmType).toString() != KXMLQLCRGBAudio)
    {
        qWarning() << Q_FUNC_INFO << "RGB Algorithm is not Audio";
        return false;
    }

    root.skipCurrentElement();

    return true;
}

bool RGBAudio::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    doc->writeStartElement(KXMLQLCRGBAlgorithm);
    doc->writeAttribute(KXMLQLCRGBAlgorithmType, KXMLQLCRGBAudio);
    doc->writeEndElement();

    return true;
}
