/*
  Q Light Controller Plus
  fixturecontrols.cpp

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

#include <algorithm>
#include <QHash>
#include <QSet>

#include "fixturecontrols.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcchannel.h"

namespace
{

/** True when $ch is part of a colour mix: a coloured emitter or an HSV axis */
bool isColorComponent(const QLCChannel *ch)
{
    if (ch->group() != QLCChannel::Intensity)
        return false;
    if (ch->colour() != QLCChannel::NoColour)
        return true;
    return ch->preset() >= QLCChannel::IntensityHue &&
           ch->preset() <= QLCChannel::IntensityValueFine;
}

/** True when $ch is set by sliding through its range rather than by picking
 *  one of its capabilities */
bool isContinuous(const QLCChannel *ch)
{
    switch (ch->group())
    {
        case QLCChannel::Intensity:
        case QLCChannel::Pan:
        case QLCChannel::Tilt:
        case QLCChannel::Speed:
            return true;
        default:
        break;
    }

    switch (ch->preset())
    {
        case QLCChannel::ColorCTOMixer:
        case QLCChannel::ColorCTCMixer:
        case QLCChannel::ColorCTBMixer:
        case QLCChannel::GoboIndex:
        case QLCChannel::ShutterIrisMinToMax:
        case QLCChannel::ShutterIrisMaxToMin:
        case QLCChannel::BeamFocusNearFar:
        case QLCChannel::BeamFocusFarNear:
        case QLCChannel::BeamZoomSmallBig:
        case QLCChannel::BeamZoomBigSmall:
        case QLCChannel::PrismRotationSlowFast:
        case QLCChannel::PrismRotationFastSlow:
            return true;
        default:
            return ch->capabilities().size() <= 1;
    }
}

/** True when $fine can be the low byte of $coarse */
bool isFineOf(const QLCChannel *fine, const QLCChannel *coarse)
{
    if (fine->group() != coarse->group() || fine->colour() != coarse->colour())
        return false;
    if (fine->preset() == QLCChannel::Custom || coarse->preset() == QLCChannel::Custom)
        return true;

    // Fine presets are named after their coarse preset: "BeamZoomFine"
    // belongs to both "BeamZoomSmallBig" and "BeamZoomBigSmall"
    QString stem = QLCChannel::presetToString(fine->preset());
    if (stem.endsWith("Fine"))
        stem.chop(4);
    return QLCChannel::presetToString(coarse->preset()).startsWith(stem);
}

/** Append the controls of one head ($head -1: channels outside every head)
 *  made of the mode channels in $scope, given in mode order */
void analyzeScope(const QLCFixtureMode *mode, int head, const QList<quint32> &scope,
                  QList<FixtureControls::Control> &controls)
{
    QList<quint32> coarse;
    for (quint32 ch : scope)
    {
        const QLCChannel *channel = mode->channel(ch);
        if (channel->group() != QLCChannel::Nothing && channel->controlByte() == QLCChannel::MSB)
            coarse.append(ch);
    }

    // A fine channel joins the nearest unpaired coarse channel before it,
    // else the nearest one after it. Without a partner it is a control alone.
    QHash<quint32, quint32> fineOf;
    QList<quint32> loneFines;
    for (quint32 ch : scope)
    {
        const QLCChannel *channel = mode->channel(ch);
        if (channel->group() == QLCChannel::Nothing || channel->controlByte() == QLCChannel::MSB)
            continue;

        auto fits = [&](quint32 c)
        {
            return !fineOf.contains(c) && isFineOf(channel, mode->channel(c));
        };
        auto after = std::lower_bound(coarse.cbegin(), coarse.cend(), ch);
        auto before = std::find_if(std::make_reverse_iterator(after), coarse.crend(), fits);
        if (before != coarse.crend())
        {
            fineOf.insert(*before, ch);
            continue;
        }
        auto next = std::find_if(after, coarse.cend(), fits);
        if (next != coarse.cend())
            fineOf.insert(*next, ch);
        else
            loneFines.append(ch);
    }

    auto withFines = [&](const QList<quint32> &channels)
    {
        QList<quint32> all = channels;
        for (quint32 ch : channels)
            if (fineOf.contains(ch))
                all.append(fineOf.value(ch));
        std::sort(all.begin(), all.end());
        return all;
    };

    QList<quint32> colors, pans, tilts, others;
    for (quint32 ch : coarse)
    {
        const QLCChannel *channel = mode->channel(ch);
        if (isColorComponent(channel))
            colors.append(ch);
        else if (channel->group() == QLCChannel::Pan)
            pans.append(ch);
        else if (channel->group() == QLCChannel::Tilt)
            tilts.append(ch);
        else
            others.append(ch);
    }

    if (colors.size() > 1)
        controls.append({ FixtureControls::ColorMixer, head, withFines(colors) });
    else
        others.append(colors);

    if (!pans.isEmpty() && !tilts.isEmpty())
        controls.append({ FixtureControls::PanTilt, head,
                          withFines({ pans.takeFirst(), tilts.takeFirst() }) });
    others.append(pans);
    others.append(tilts);

    for (quint32 ch : others)
    {
        FixtureControls::Kind kind = isContinuous(mode->channel(ch)) ? FixtureControls::Fader
                                                                     : FixtureControls::Presets;
        controls.append({ kind, head, withFines({ ch }) });
    }

    for (quint32 ch : loneFines)
        controls.append({ FixtureControls::Fader, head, { ch } });
}

} // namespace

QList<FixtureControls::Control> FixtureControls::analyze(const QLCFixtureMode *mode)
{
    QList<Control> controls;
    if (mode == nullptr)
        return controls;

    const quint32 channelCount = quint32(mode->channels().size());

    // A channel listed by several heads belongs to the first of them
    QSet<quint32> taken;
    const QVector<QLCFixtureHead> &heads = mode->heads();
    for (int h = 0; h < heads.size(); ++h)
    {
        QList<quint32> scope;
        for (quint32 ch : heads.at(h).channels())
        {
            if (ch < channelCount && !taken.contains(ch))
            {
                taken.insert(ch);
                scope.append(ch);
            }
        }
        std::sort(scope.begin(), scope.end());
        analyzeScope(mode, h, scope, controls);
    }

    QList<quint32> rest;
    for (quint32 ch = 0; ch < channelCount; ++ch)
        if (!taken.contains(ch))
            rest.append(ch);
    analyzeScope(mode, -1, rest, controls);

    std::sort(controls.begin(), controls.end(), [](const Control &a, const Control &b)
    {
        return a.channels.first() < b.channels.first();
    });
    return controls;
}
