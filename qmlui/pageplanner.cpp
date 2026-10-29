/*
  Q Light Controller Plus
  pageplanner.cpp

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

#include <QHash>
#include <QMetaType>
#include <cmath>
#include <functional>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include "pageplanner.h"
#include "qlccapability.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "qlcfixturedef.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

namespace
{

/** A slot of one model, before models are compared */
struct ModelSlot
{
    PagePlanner::Slot slot;
    /** Presets only: the capabilities every merged channel shares, empty
     *  when they differ and one value means different things */
    QString capabilities;
    bool strip = false;
};

QString capabilitySignature(const QLCChannel *channel)
{
    QStringList parts;
    for (const QLCCapability *cap : channel->capabilities())
        parts << QString("%1-%2:%3").arg(cap->min()).arg(cap->max()).arg(cap->name());
    return parts.join('|');
}

/** The preset name without the direction or resolution it is given in, so
 *  "BeamZoomSmallBig" and "BeamZoomBigSmall" are the same slot */
QString presetStem(QLCChannel::Preset preset)
{
    QString stem = QLCChannel::presetToString(preset);
    for (const char *suffix : { "Fine", "SmallBig", "BigSmall", "NearFar", "FarNear",
                                "MinToMax", "MaxToMin", "SlowFast", "FastSlow" })
        if (stem.endsWith(QLatin1String(suffix)))
            stem.chop(int(qstrlen(suffix)));
    return stem;
}

QString groupName(QLCChannel::Group group)
{
    return QLCChannel::groupToString(group);
}

/** The model independent name of $control. $ordinals counts the preset
 *  channels of each group seen so far in the same head */
QString slotKey(const QLCFixtureMode *mode, const FixtureControls::Control &control,
                QHash<QString, int> &ordinals)
{
    switch (control.kind)
    {
        case FixtureControls::PanTilt: return "Position";
        case FixtureControls::ColorMixer: return "Color";
        default: break;
    }

    const QLCChannel *ch = mode->channel(control.channels.first());
    if (ch->group() == QLCChannel::Intensity)
    {
        if (ch->colour() != QLCChannel::NoColour)
            return "Intensity/" + QLCChannel::colourToString(ch->colour());
        if (ch->preset() >= QLCChannel::IntensityHue && ch->preset() <= QLCChannel::IntensityValueFine)
            return "Color/" + presetStem(ch->preset());
        return "Intensity/Dimmer";
    }
    if (ch->preset() != QLCChannel::Custom)
        return groupName(ch->group()) + "/" + presetStem(ch->preset());
    switch (ch->group())
    {
        case QLCChannel::Colour:
        case QLCChannel::Gobo:
        case QLCChannel::Shutter:
        case QLCChannel::Prism:
        {
            const QString group = groupName(ch->group());
            return QString("%1#%2").arg(group).arg(++ordinals[group]);
        }
        default:
            return groupName(ch->group()) + "/" + ch->name().toLower().simplified();
    }
}

/** A caption for a slot merged from channels with different names */
QString keyLabel(const QString &key)
{
    if (key.endsWith("/rate"))
        return QObject::tr("%1 speed").arg(keyLabel(key.chopped(5)));
    if (key == "Intensity/Dimmer")
        return QObject::tr("Dimmer");
    if (key.startsWith("Intensity/"))
        return key.mid(10);
    static const QHash<QString, QString> names = {
        { "Speed/SpeedPanTilt", QObject::tr("P/T speed") },
        { "Beam/BeamFocus", QObject::tr("Focus") },
        { "Beam/BeamZoom", QObject::tr("Zoom") },
        { "Shutter/ShutterIris", QObject::tr("Iris") },
        { "Shutter/ShutterStrobe", QObject::tr("Strobe") },
        { "Colour", QObject::tr("Color wheel") },
        { "Gobo", QObject::tr("Gobo wheel") },
        { "Shutter", QObject::tr("Shutter") },
        { "Prism", QObject::tr("Prism") },
    };
    const QString base = key.section('~', 0, 0).section('#', 0, 0);
    QString label = names.value(base, base.section('/', -1));
    if (key.contains('#') && key.section('#', 1).toInt() > 1)
        label += " " + key.section('#', 1);
    return label;
}

QString modelName(const Fixture *fixture)
{
    if (fixture->fixtureDef() == nullptr)
        return fixture->name();
    return fixture->fixtureDef()->model();
}

QString modelKey(const Fixture *fixture)
{
    const QLCFixtureDef *def = fixture->fixtureDef();
    const QLCFixtureMode *mode = fixture->fixtureMode();
    return QString("%1/%2/%3/%4").arg(def ? def->manufacturer() : QString(), def ? def->model() : QString(),
                                      mode ? mode->name() : QString())
                                 .arg(mode ? mode->channels().size() : 0);
}

/** Merge $more into the slots of $into by key, keeping first-seen order */
void mergeSlot(QList<ModelSlot> &into, QHash<QString, int> &index, const ModelSlot &more)
{
    auto it = index.constFind(more.slot.key);
    if (it == index.constEnd())
    {
        index.insert(more.slot.key, into.size());
        into.append(more);
        return;
    }
    ModelSlot &slot = into[it.value()];
    slot.slot.channels.append(more.slot.channels);
    slot.slot.heads.append(more.slot.heads);
    // the same capabilities: choice i means the same on both
    if (slot.slot.choices.size() == more.slot.choices.size())
        for (int i = 0; i < slot.slot.choices.size(); ++i)
            slot.slot.choices[i].values.append(more.slot.choices.at(i).values);
    if (slot.slot.label != more.slot.label)
        slot.slot.label = keyLabel(slot.slot.key);
    if (slot.capabilities != more.capabilities)
        slot.capabilities.clear();
}

/** A fader per emitter of the mixer $control, its fine channel with it. The
 *  Click & Go colour picker sets only red, green, blue, white, amber and UV
 *  (Lime and Indigo stay at 0), and a fader trims one emitter */
QList<ModelSlot> componentSlots(const QLCFixtureMode *mode, quint32 id, const FixtureControls::Control &control)
{
    QList<ModelSlot> result;
    for (quint32 coarse : control.channels)
    {
        const QLCChannel *ch = mode->channel(coarse);
        if (ch->controlByte() != QLCChannel::MSB)
            continue;
        ModelSlot ms;
        ms.slot.kind = FixtureControls::Fader;
        ms.slot.key = "Color/" + QLCChannel::colourToString(ch->colour());
        ms.slot.label = QLCChannel::colourToString(ch->colour());
        ms.slot.channels.append({ id, coarse });
        for (quint32 fine : control.channels)
        {
            const QLCChannel *other = mode->channel(fine);
            if (other->controlByte() == QLCChannel::LSB && other->colour() == ch->colour())
                ms.slot.channels.append({ id, fine });
        }
        result.append(ms);
    }
    return result;
}

/** The caption of the button of $cap: a fixture's "No function" turns the
 *  effect of its channel off */
QString choiceLabel(const QLCCapability *cap)
{
    static const QStringList noOps = { "no function", "no effect", "nothing", "none", "unused" };
    return noOps.contains(cap->name().trimmed().toLower()) ? QObject::tr("Off") : cap->name();
}

/** One capability per button caption: "LED on" can be listed once per range */
QList<const QLCCapability *> distinctCapabilities(const QLCChannel *ch)
{
    QList<const QLCCapability *> caps;
    QSet<QString> names;
    for (const QLCCapability *cap : ch->capabilities())
    {
        const QString name = choiceLabel(cap).toLower();
        if (names.contains(name))
            continue;
        names.insert(name);
        caps << cap;
    }
    return caps;
}

/** One capability per colour or picture, mixed colours left out */
QList<const QLCCapability *> swatchCapabilities(const QLCChannel *ch)
{
    QList<const QLCCapability *> caps;
    QSet<QString> resources;
    for (const QLCCapability *cap : ch->capabilities())
    {
        if (cap->presetType() != QLCCapability::SingleColor && cap->presetType() != QLCCapability::Picture)
            continue;
        const QString resource = cap->resource(0).toString();
        if (resource.isEmpty() || resource.endsWith('/') || resources.contains(resource))
            continue;
        resources.insert(resource);
        caps << cap;
    }
    return caps;
}

bool isVisual(const QLCCapability *cap)
{
    return cap->presetType() == QLCCapability::SingleColor || cap->presetType() == QLCCapability::DoubleColor ||
           cap->presetType() == QLCCapability::Picture;
}

/** What the value inside the capability range sets, when it matters */
enum class Range { None, Speed, Size };

Range rangeOf(const QLCCapability *cap)
{
    switch (cap->preset())
    {
        case QLCCapability::NearToFar:
        case QLCCapability::FarToNear:
        case QLCCapability::BigToSmall:
        case QLCCapability::SmallToBig:
            return Range::Size;
        case QLCCapability::SlowToFast:
        case QLCCapability::FastToSlow:
        case QLCCapability::StrobeSlowToFast:
        case QLCCapability::StrobeFastToSlow:
        case QLCCapability::StrobeRandomSlowToFast:
        case QLCCapability::StrobeRandomFastToSlow:
        case QLCCapability::StrobeFreqRange:
        case QLCCapability::PulseSlowToFast:
        case QLCCapability::PulseFastToSlow:
        case QLCCapability::PulseFreqRange:
        case QLCCapability::RampUpSlowToFast:
        case QLCCapability::RampUpFastToSlow:
        case QLCCapability::RampDownSlowToFast:
        case QLCCapability::RampDownFastToSlow:
        case QLCCapability::RampUpFreqRange:
        case QLCCapability::RampDownFreqRange:
        case QLCCapability::RotationClockwiseSlowToFast:
        case QLCCapability::RotationClockwiseFastToSlow:
        case QLCCapability::RotationCounterClockwiseSlowToFast:
        case QLCCapability::RotationCounterClockwiseFastToSlow:
            return Range::Speed;
        default:
            return Range::None;
    }
}

/** The widgets of a Presets slot: a button per capability for a short,
 *  mostly tagged list (a fader beside them when a value inside a range
 *  matters, like a strobe rate), a swatch button per colour or picture plus
 *  the Click & Go picker for a wheel, else the picker alone */
QList<ModelSlot> presetWidgets(const Doc *doc, const ModelSlot &ms)
{
    static const int maxButtons = 8, maxSwatches = 24;
    const PagePlanner::Channel first = ms.slot.channels.first();
    const QLCChannel *ch = doc->fixture(first.fixture)->channel(first.channel);
    const QList<const QLCCapability *> distinct = distinctCapabilities(ch);
    const QList<const QLCCapability *> swatches = swatchCapabilities(ch);
    int tagged = 0, ranges = 0, speeds = 0;
    for (const QLCCapability *cap : distinct)
    {
        tagged += cap->preset() != QLCCapability::Custom || isVisual(cap) ? 1 : 0;
        ranges += rangeOf(cap) != Range::None ? 1 : 0;
        speeds += rangeOf(cap) == Range::Speed ? 1 : 0;
    }

    QList<const QLCCapability *> buttons;
    bool fader = false, picker = true;
    if (distinct.size() >= 2 && distinct.size() <= maxButtons && tagged * 2 >= distinct.size())
    {
        buttons = distinct;
        fader = ranges > 0;
        picker = false;
    }
    else if (swatches.size() >= 2 && swatches.size() <= maxSwatches)
        buttons = swatches;

    QList<ModelSlot> result;
    if (!buttons.isEmpty())
    {
        ModelSlot choices = ms;
        choices.slot.key += "/buttons";
        choices.slot.channels.clear();
        for (const QLCCapability *cap : buttons)
        {
            PagePlanner::Choice choice { choiceLabel(cap), QColor(), QString(), {} };
            if (cap->presetType() == QLCCapability::SingleColor)
                choice.color = cap->resource(0).value<QColor>();
            else if (cap->presetType() == QLCCapability::Picture)
                choice.picture = cap->resource(0).toString();
            for (const PagePlanner::Channel &channel : ms.slot.channels)
            {
                const QLCChannel *qlcch = doc->fixture(channel.fixture)->channel(channel.channel);
                if (qlcch->controlByte() == QLCChannel::MSB)
                    choice.values.append({ channel.fixture, channel.channel, cap->middle() });
            }
            choices.slot.choices.append(choice);
        }
        result.append(choices);
    }
    // beside the buttons, a fader or picker named after what it adds to them
    if (fader)
    {
        ModelSlot rate = ms;
        rate.slot.key += "/rate";
        rate.slot.kind = FixtureControls::Fader;
        rate.slot.label = (speeds > 0 ? QObject::tr("%1 speed") : QObject::tr("%1 range")).arg(ms.slot.label);
        result.append(rate);
    }
    if (picker)
    {
        ModelSlot all = ms;
        if (!buttons.isEmpty())
            all.slot.label = QObject::tr("%1 presets").arg(ms.slot.label);
        result.append(all);
    }
    return result;
}

/** The slots of the fixtures of one model, merged across fixtures and,
 *  for keys without a head suffix, across heads */
QList<ModelSlot> modelSlots(const Doc *doc, const QList<quint32> &fixtureIDs,
                            const PagePlanner::Options &options)
{
    QList<ModelSlot> items;
    QHash<QString, int> index;
    for (quint32 id : fixtureIDs)
    {
        const Fixture *fixture = doc->fixture(id);
        const QLCFixtureMode *mode = fixture ? fixture->fixtureMode() : nullptr;
        if (mode == nullptr)
            continue;
        const int heads = mode->heads().size();
        const bool strips = options.heads == PagePlanner::HeadStrips && heads > 1 &&
                            heads <= options.maxHeadStrips;
        const QList<FixtureControls::Control> controls = FixtureControls::analyze(mode);
        QHash<int, QHash<QString, int>> ordinals;
        QHash<int, QSet<QString>> usedKeys;
        for (const FixtureControls::Control &control : controls)
        {
            ModelSlot ms;
            ms.slot.kind = control.kind;
            // two controls of one head under one key are two controls: number the next
            const QString key = slotKey(mode, control, ordinals[control.head]);
            ms.slot.key = key;
            for (int n = 2; usedKeys[control.head].contains(ms.slot.key); ++n)
                ms.slot.key = QString("%1~%2").arg(key).arg(n);
            usedKeys[control.head].insert(ms.slot.key);
            const QLCChannel *first = mode->channel(control.channels.first());
            ms.slot.label = control.kind == FixtureControls::ColorMixer ? QObject::tr("Color")
                          : control.kind == FixtureControls::PanTilt ? QObject::tr("Position")
                          : first->name();
            if (control.kind == FixtureControls::Presets)
                ms.capabilities = capabilitySignature(first);
            for (quint32 ch : control.channels)
                ms.slot.channels.append({ id, ch });
            // Pan and tilt outside every head go on head 0: a head without its
            // own takes the fixture's (QLCFixtureHead::cacheChannels)
            if (control.kind == FixtureControls::PanTilt)
                ms.slot.heads.append({ id, qMax(0, control.head) });

            // Multi-head controls merge across heads under a neutral caption
            const bool perHead = control.head >= 0 && heads > 1 && control.kind != FixtureControls::PanTilt;
            if (perHead)
            {
                // "@all": the same control of every head, never one outside the heads
                ModelSlot merged = ms;
                merged.slot.key += "@all";
                merged.slot.label = keyLabel(ms.slot.key);
                mergeSlot(items, index, merged);
            }
            else
                mergeSlot(items, index, ms);

            if (control.kind == FixtureControls::ColorMixer)
                for (const ModelSlot &component : componentSlots(mode, id, control))
                    mergeSlot(items, index, component);

            if (perHead && strips)
            {
                ms.slot.key += QString("@%1").arg(control.head);
                ms.slot.label = QString::number(control.head + 1);
                ms.strip = true;
                mergeSlot(items, index, ms);
            }
        }
    }

    // a model without a control outside its heads needs no "@all": the
    // plain key merges with the same control of single-head models
    QSet<QString> keys;
    for (const ModelSlot &ms : items)
        keys.insert(ms.slot.key);
    for (ModelSlot &ms : items)
        if (ms.slot.key.endsWith("@all") && !keys.contains(ms.slot.key.chopped(4)))
            ms.slot.key.chop(4);

    // per-head strips stay pickers: a row of buttons per head is too much
    QList<ModelSlot> result;
    for (const ModelSlot &ms : items)
    {
        if (ms.slot.kind == FixtureControls::Presets && !ms.strip && ms.slot.choices.isEmpty())
            result.append(presetWidgets(doc, ms));
        else
            result.append(ms);
    }
    return result;
}

/** What a mixer of the colour of $channel sends for $color */
uchar mixerValue(const QLCChannel *channel, const QColor &color)
{
    switch (channel->colour())
    {
        case QLCChannel::Red: return uchar(color.red());
        case QLCChannel::Green: return uchar(color.green());
        case QLCChannel::Blue: return uchar(color.blue());
        case QLCChannel::Cyan: return uchar(255 - color.red());
        case QLCChannel::Magenta: return uchar(255 - color.green());
        case QLCChannel::Yellow: return uchar(255 - color.blue());
        case QLCChannel::White: return color == Qt::white ? 255 : 0;
        default: return 0;
    }
}

qreal colorDistance(const QColor &a, const QColor &b)
{
    const int dr = a.red() - b.red(), dg = a.green() - b.green(), db = a.blue() - b.blue();
    return std::sqrt(qreal(dr * dr + dg * dg + db * db));
}

/** The values that give $fixture the colour $color: its mixers, else the
 *  closest slot of a colour wheel. Empty when it has neither */
QList<PagePlanner::Value> colorValues(const Fixture *fixture, const QColor &color)
{
    QList<PagePlanner::Value> mixer, wheel;
    qreal best = 110; // farther than this is another colour
    const QLCFixtureMode *mode = fixture->fixtureMode();
    for (int i = 0; i < mode->channels().size(); ++i)
    {
        const QLCChannel *ch = mode->channel(i);
        if (ch->controlByte() != QLCChannel::MSB)
            continue;
        if (ch->group() == QLCChannel::Intensity && ch->colour() != QLCChannel::NoColour)
            mixer.append({ fixture->id(), quint32(i), mixerValue(ch, color) });
        if (ch->group() != QLCChannel::Colour)
            continue;
        for (const QLCCapability *cap : ch->capabilities())
        {
            if (cap->resource(0).userType() != QMetaType::QColor)
                continue;
            const qreal distance = colorDistance(cap->resource(0).value<QColor>(), color);
            if (distance < best)
            {
                best = distance;
                wheel = { { fixture->id(), quint32(i), cap->middle() } };
            }
        }
    }
    return mixer.isEmpty() ? wheel : mixer;
}

/** The values that put $fixture in one of $presets: the first shutter
 *  capability tagged with one of them */
QList<PagePlanner::Value> shutterValues(const Fixture *fixture, const QList<QLCCapability::Preset> &presets)
{
    const QLCFixtureMode *mode = fixture->fixtureMode();
    for (int i = 0; i < mode->channels().size(); ++i)
    {
        const QLCChannel *ch = mode->channel(i);
        if (ch->group() != QLCChannel::Shutter || ch->controlByte() != QLCChannel::MSB)
            continue;
        for (const QLCCapability *cap : ch->capabilities())
            if (presets.contains(cap->preset()))
                return { { fixture->id(), quint32(i), cap->middle() } };
    }
    return {};
}

/** Slots of choices that mean the same on several models, though each
 *  model needs its own channel and value: colours and shutter states.
 *  A choice is kept when two models or more can do it */
QList<ModelSlot> semanticSlots(const Doc *doc, const QList<QList<quint32>> &modelFixtures)
{
    using Values = std::function<QList<PagePlanner::Value>(const Fixture *)>;
    auto choice = [&](const QString &label, const QColor &color, const Values &values,
                      PagePlanner::Slot &slot)
    {
        PagePlanner::Choice c { label, color, QString(), {} };
        int models = 0;
        for (const QList<quint32> &fixtures : modelFixtures)
        {
            bool has = false;
            for (quint32 id : fixtures)
            {
                const QList<PagePlanner::Value> v = values(doc->fixture(id));
                c.values.append(v);
                has = has || !v.isEmpty();
            }
            models += has ? 1 : 0;
        }
        if (models >= 2)
            slot.choices.append(c);
    };

    ModelSlot colors;
    colors.slot.key = "Color/Choices";
    colors.slot.label = QObject::tr("Colors");
    colors.slot.kind = FixtureControls::Presets;
    static const QList<QPair<QString, QColor>> palette = {
        { QObject::tr("White"), Qt::white }, { QObject::tr("Red"), Qt::red },
        { QObject::tr("Orange"), QColor(255, 128, 0) }, { QObject::tr("Yellow"), Qt::yellow },
        { QObject::tr("Green"), Qt::green }, { QObject::tr("Cyan"), Qt::cyan },
        { QObject::tr("Blue"), Qt::blue }, { QObject::tr("Magenta"), Qt::magenta } };
    for (const QPair<QString, QColor> &color : palette)
        choice(color.first, color.second, [&](const Fixture *f) { return colorValues(f, color.second); }, colors.slot);

    ModelSlot shutter;
    shutter.slot.key = "Shutter/Choices";
    shutter.slot.label = QObject::tr("Shutter");
    shutter.slot.kind = FixtureControls::Presets;
    static const QList<QPair<QString, QList<QLCCapability::Preset>>> states = {
        { QObject::tr("Open"), { QLCCapability::ShutterOpen } },
        { QObject::tr("Closed"), { QLCCapability::ShutterClose } },
        { QObject::tr("Strobe"), { QLCCapability::StrobeSlowToFast, QLCCapability::StrobeFastToSlow,
                                   QLCCapability::StrobeFrequency, QLCCapability::StrobeFreqRange } },
        { QObject::tr("Pulse"), { QLCCapability::PulseSlowToFast, QLCCapability::PulseFastToSlow,
                                  QLCCapability::PulseFrequency, QLCCapability::PulseFreqRange } },
        { QObject::tr("Random"), { QLCCapability::StrobeRandom, QLCCapability::StrobeRandomSlowToFast,
                                   QLCCapability::StrobeRandomFastToSlow } } };
    for (const auto &state : states)
        choice(state.first, QColor(), [&](const Fixture *f) { return shutterValues(f, state.second); }, shutter.slot);

    QList<ModelSlot> result;
    for (const ModelSlot &slot : { colors, shutter })
        if (!slot.slot.choices.isEmpty())
            result.append(slot);
    return result;
}

bool mergesAcrossModels(const ModelSlot &slot)
{
    return slot.slot.kind != FixtureControls::Presets || !slot.capabilities.isEmpty();
}

/** Move each speed fader right after the buttons whose ranges it sets: the
 *  buttons of its channel, else buttons of its group (the shutter states
 *  every model shares) */
void keepSpeedsByButtons(QList<PagePlanner::Slot> &items)
{
    const auto group = [](const QString &key) { return key.section('#', 0, 0).section('/', 0, 0); };
    for (int i = 0; i < items.size(); ++i)
    {
        if (!items.at(i).key.endsWith("/rate"))
            continue;
        const QString base = items.at(i).key.chopped(5);
        int buttons = -1;
        for (int j = 0; j < items.size(); ++j)
        {
            if (items.at(j).choices.isEmpty())
                continue;
            if (items.at(j).key == base + "/buttons")
            {
                buttons = j;
                break;
            }
            if (buttons < 0 && group(items.at(j).key) == group(base))
                buttons = j;
        }
        if (buttons < 0 || buttons == i - 1)
            continue;
        const PagePlanner::Slot rate = items.takeAt(i);
        if (buttons > i)
        {
            items.insert(buttons, rate);
            --i; // the slot after the fader moved up into its place
        }
        else
            items.insert(buttons + 1, rate);
    }
}

/** The sections of a page in function order: $shared slots first, then a
 *  part per model for the slots of that function only that model has */
QList<PagePlanner::Section> sections(const QList<ModelSlot> &shared,
                                     const QList<QPair<QString, QList<ModelSlot>>> &own, bool atmospheric)
{
    // what an operator needs first: light, where it points, its colour, then beam shaping.
    // Translated like the titles PagePlanner::sectionOf() gives
    const QStringList order = { QObject::tr("Intensity"), QObject::tr("Output"), QObject::tr("Fan"),
                                QObject::tr("Position"), QObject::tr("Color"), QObject::tr("Matrix"),
                                QObject::tr("Gobo"), QObject::tr("Beam"), QObject::tr("Effects"),
                                QObject::tr("Setup") };
    QMap<int, PagePlanner::Section> ordered;
    auto sectionOf = [&](const ModelSlot &ms) -> PagePlanner::Section &
    {
        QString title = PagePlanner::sectionOf(ms.slot.key);
        // a hazer or a fog machine: its output and its fan
        if (atmospheric && title != QObject::tr("Setup"))
            title = ms.slot.key.startsWith("Speed") ? QObject::tr("Fan") : QObject::tr("Output");
        PagePlanner::Section &section = ordered[order.indexOf(title) * 2 + (ms.strip ? 1 : 0)];
        if (section.title.isEmpty())
        {
            section.title = ms.strip ? QObject::tr("%1 per head").arg(title) : title;
            section.strip = ms.strip;
        }
        return section;
    };
    for (const ModelSlot &ms : shared)
        sectionOf(ms).items.append(ms.slot);
    for (const QPair<QString, QList<ModelSlot>> &model : own)
    {
        for (const ModelSlot &ms : model.second)
        {
            PagePlanner::Section &section = sectionOf(ms);
            if (section.parts.isEmpty() || section.parts.last().model != model.first)
            {
                PagePlanner::Section part;
                part.title = part.model = model.first;
                part.strip = ms.strip;
                section.parts.append(part);
            }
            section.parts.last().items.append(ms.slot);
        }
    }

    // A section of one model only needs no frame inside a frame
    QList<PagePlanner::Section> result;
    for (PagePlanner::Section section : ordered)
    {
        if (section.items.isEmpty() && section.parts.size() == 1)
        {
            const QString title = section.title;
            section = section.parts.takeFirst();
            section.title = QString("%1: %2").arg(title, section.model);
        }
        keepSpeedsByButtons(section.items);
        for (PagePlanner::Section &part : section.parts)
            keepSpeedsByButtons(part.items);
        result.append(section);
    }
    return result;
}

/** A Flash button for strobes: every dimmer at full while it is held.
 *  Empty when the fixtures have no dimmer */
QList<ModelSlot> flashSlot(const Doc *doc, const QList<quint32> &fixtureIDs)
{
    ModelSlot ms;
    ms.slot.key = "Intensity/Flash";
    ms.slot.label = QObject::tr("Flash");
    ms.slot.kind = FixtureControls::Presets;
    ms.slot.flash = true;
    PagePlanner::Choice choice { QObject::tr("Flash"), QColor(), QString(), {} };
    for (quint32 id : fixtureIDs)
    {
        const Fixture *fixture = doc->fixture(id);
        for (quint32 i = 0; fixture && i < fixture->channels(); ++i)
        {
            const QLCChannel *ch = fixture->channel(i);
            if (ch->group() == QLCChannel::Intensity && ch->colour() == QLCChannel::NoColour &&
                ch->controlByte() == QLCChannel::MSB)
                choice.values.append({ id, i, uchar(255) });
        }
    }
    if (choice.values.isEmpty())
        return {};
    ms.slot.choices.append(choice);
    return { ms };
}

/** An RGB matrix effect over the heads of a pixel fixture: several colour
 *  mixers and no pan or tilt. The grid is the physical layout when it covers
 *  the heads, else one row */
QList<ModelSlot> matrixSlot(const Fixture *fixture)
{
    const QLCFixtureMode *mode = fixture->fixtureMode();
    int mixers = 0;
    for (const FixtureControls::Control &control : FixtureControls::analyze(mode))
    {
        if (control.kind == FixtureControls::PanTilt)
            return {};
        mixers += control.kind == FixtureControls::ColorMixer && control.head >= 0 ? 1 : 0;
    }
    if (mixers < 2)
        return {};
    ModelSlot ms;
    ms.slot.key = "Matrix";
    ms.slot.label = QObject::tr("Effect");
    ms.slot.kind = FixtureControls::ColorMixer;
    for (int h = 0; h < mode->heads().size(); ++h)
        ms.slot.heads.append({ fixture->id(), h });
    const QSize layout = mode->physical().layoutSize();
    ms.slot.matrixGrid = layout.width() * layout.height() == mode->heads().size()
                       ? layout : QSize(mode->heads().size(), 1);
    return { ms };
}

} // namespace

QString PagePlanner::sectionOf(const QString &key)
{
    const QString base = key.section('@', 0, 0);
    if (base.startsWith("Intensity/"))
        return QObject::tr("Intensity");
    if (base == "Color" || base.startsWith("Colour") || base.startsWith("Color/"))
        return QObject::tr("Color");
    if (base == "Position" || base.startsWith("Pan") || base.startsWith("Tilt") ||
        base.startsWith("Speed/SpeedPan") || base.startsWith("Speed/SpeedTilt"))
        return QObject::tr("Position");
    // a speed goes with what it moves, by the name of its channel
    if (base.startsWith("Speed/"))
    {
        static const QRegularExpression position("\\b(p/?t|pan|tilt)\\b"), gobo("\\bgobo"),
                                        color("\\bcolou?r"), beam("\\b(prism|beam|zoom|focus|iris|frost)");
        const QString name = base.mid(6);
        if (position.match(name).hasMatch())
            return QObject::tr("Position");
        if (gobo.match(name).hasMatch())
            return QObject::tr("Gobo");
        if (color.match(name).hasMatch())
            return QObject::tr("Color");
        if (beam.match(name).hasMatch())
            return QObject::tr("Beam");
    }
    // the shutter decides with the dimmer whether light comes out; the iris shapes the beam
    if (base.startsWith("Shutter") && !base.startsWith("Shutter/ShutterIris"))
        return QObject::tr("Intensity");
    if (base == "Matrix")
        return QObject::tr("Matrix");
    if (base.startsWith("Shutter") || base.startsWith("Beam") || base.startsWith("Prism"))
        return QObject::tr("Beam");
    if (base.startsWith("Gobo"))
        return QObject::tr("Gobo");
    if (base.startsWith("Maintenance"))
        return QObject::tr("Setup");
    return QObject::tr("Effects");
}

QList<PagePlanner::Page> PagePlanner::plan(const Doc *doc, const QList<quint32> &fixtureIDs,
                                           const QString &title, const Options &options)
{
    // Models in order of their first fixture
    QStringList models;
    QHash<QString, QList<quint32>> fixturesOf;
    QHash<QString, QString> nameOf;
    for (quint32 id : fixtureIDs)
    {
        const Fixture *fixture = doc->fixture(id);
        if (fixture == nullptr || fixture->fixtureMode() == nullptr)
            continue;
        const QString key = modelKey(fixture);
        if (!fixturesOf.contains(key))
        {
            models.append(key);
            nameOf.insert(key, modelName(fixture));
        }
        fixturesOf[key].append(id);
    }

    QList<QList<ModelSlot>> perModel;
    for (const QString &model : models)
        perModel.append(modelSlots(doc, fixturesOf.value(model), options));

    // A key is shared when one widget can drive it on every model that has
    // it (values of preset channels mean something else per model) and
    // enough models have it: all of them for Intersection, two otherwise
    QList<ModelSlot> merged;
    QHash<QString, int> mergedIndex;
    QHash<QString, int> modelCount;
    for (const QList<ModelSlot> &items : perModel)
    {
        for (const ModelSlot &slot : items)
        {
            mergeSlot(merged, mergedIndex, slot);
            modelCount[slot.slot.key]++;
        }
    }
    const int needed = options.strategy == Intersection || perModel.size() == 1 ? perModel.size() : 2;
    QList<ModelSlot> shared;
    QSet<QString> sharedKeys;
    for (const ModelSlot &slot : merged)
    {
        if (modelCount.value(slot.slot.key) >= needed && mergesAcrossModels(slot))
        {
            sharedKeys.insert(slot.slot.key);
            shared.append(slot);
        }
    }
    if (models.size() > 1)
    {
        QList<QList<quint32>> modelFixtures;
        for (const QString &model : models)
            modelFixtures.append(fixturesOf.value(model));
        shared.append(semanticSlots(doc, modelFixtures));
    }

    QList<Page> pages;
    Page page;
    page.title = title;
    QList<QPair<QString, QList<ModelSlot>>> own;
    if (models.size() > 1 && options.strategy == Union)
    {
        for (int m = 0; m < models.size(); ++m)
        {
            own.append({ nameOf.value(models.at(m)), {} });
            for (const ModelSlot &slot : perModel.at(m))
                if (!sharedKeys.contains(slot.slot.key))
                    own.last().second.append(slot);
        }
    }
    // def types tell what the structure cannot: a strobe, a hazer
    bool strobes = !models.isEmpty(), atmospheric = !models.isEmpty();
    for (const QString &model : models)
    {
        const QLCFixtureDef *def = doc->fixture(fixturesOf.value(model).first())->fixtureDef();
        const QLCFixtureDef::FixtureType type = def ? def->type() : QLCFixtureDef::Other;
        strobes = strobes && type == QLCFixtureDef::Strobe;
        atmospheric = atmospheric && (type == QLCFixtureDef::Hazer || type == QLCFixtureDef::Smoke ||
                                      type == QLCFixtureDef::Fan);
    }
    if (strobes)
        shared.append(flashSlot(doc, fixtureIDs));
    if (fixtureIDs.size() == 1 && !models.isEmpty())
        shared.append(matrixSlot(doc->fixture(fixtureIDs.first())));
    page.sections = sections(shared, own, atmospheric);
    // one model: per-head strips in the rows and columns of its physical layout
    if (models.size() == 1)
    {
        const QLCFixtureMode *mode = doc->fixture(fixturesOf.value(models.first()).first())->fixtureMode();
        const QSize layout = mode->physical().layoutSize();
        if (layout.width() * layout.height() == mode->heads().size())
            for (Section &section : page.sections)
                if (section.strip)
                    section.columns = layout.width();
    }
    // the group page, then a page per model named after it
    if (models.size() > 1 && options.strategy == MasterAndModels)
    {
        pages.append(page);
        for (const QString &model : models)
            pages.append(plan(doc, fixturesOf.value(model), nameOf.value(model), options).first());
        return pages;
    }
    pages.append(page);
    return pages;
}
