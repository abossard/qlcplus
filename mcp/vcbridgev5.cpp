/*
  Q Light Controller Plus
  vcbridgev5.cpp

  Copyright (C) Massimo Callegari

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

#include "vcbridgev5.h"
#include "app.h"
#include "djmanager.h"
#include "vdjbridge.h"
#include "virtualconsole.h"
#include "vcpage.h"
#include "vcframe.h"
#include "vcsoloframe.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vcxypad.h"
#include "vcxypadpreset.h"
#include "vccuelist.h"
#include "vclabel.h"
#include "vcspeeddial.h"
#include "vcspeeddialpreset.h"
#include "vcaudiotriggers.h"
#include "vcclock.h"
#include "vcrecordpanel.h"
#include "vcanimation.h"
#include "vcwidget.h"
#include "vclayoutadapter.h"
#include "tardis.h"
#include "doc.h"
#include "fixture.h"
#include "function.h"
#include "showcommandrecorder.h"
#include "showcontrolaction.h"
#include "show.h"
#include "scenevalue.h"
#include "grandmaster.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "qlcinputsource.h"
#include "outputpatch.h"
#include "qlcinputfeedback.h"
#include "tools/vc_tools_common.h"

#include <QQmlEngine>
#include <QDebug>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

// --- Source definition table ---
// Maps widget class name → valid input source {name, ID, description}.
// IDs match the qmlui #define INPUT_*_ID constants.

static const QMap<QString, QList<VCBridge::SourceDef>> sourceDefTable = {
    {"VCButton",        {{"default", 0, "Button pressure"}}},
    {"VCSlider",        {{"default", 0, "Slider control"},
                         {"overrideReset", 1, "Reset control"},
                         {"flashButton", 2, "Flash control"}}},
    {"VCCueList",       {{"next", 0, "Next cue"},
                         {"previous", 1, "Previous cue"},
                         {"playback", 2, "Play/stop/pause"},
                         {"stop", 3, "Stop/pause"},
                         {"sideFader", 4, "Side fader"}}},
    {"VCXYPad",         {{"pan", 0, "Pan / Horizontal axis"},
                         {"panFine", 1, "Pan fine"},
                         {"tilt", 2, "Tilt / Vertical axis"},
                         {"tiltFine", 3, "Tilt fine"},
                         {"width", 4, "Width"},
                         {"height", 5, "Height"}}},
    {"VCSpeedDial",     {{"absolute", 0, "Time wheel"},
                         {"tap", 1, "Tap button"},
                         {"mult", 2, "Multiply button"},
                         {"div", 3, "Divide button"},
                         {"multDivReset", 4, "Reset button"},
                         {"apply", 5, "Apply button"},
                         {"1_16x", 6, "1/16x speed"},
                         {"1_8x", 7, "1/8x speed"},
                         {"1_4x", 8, "1/4x speed"},
                         {"1_2x", 9, "1/2x speed"},
                         {"2x", 10, "2x speed"},
                         {"4x", 11, "4x speed"},
                         {"8x", 12, "8x speed"},
                         {"16x", 13, "16x speed"}}},
    {"VCClock",         {{"play", 0, "Play/pause timer"},
                         {"reset", 1, "Reset timer"}}},
    {"VCFrame",         {{"nextPage", 0, "Next page"},
                         {"previousPage", 1, "Previous page"},
                         {"enable", 2, "Enable/disable frame"},
                         {"collapse", 3, "Collapse/expand"}}},
    {"VCSoloFrame",     {{"nextPage", 0, "Next page"},
                         {"previousPage", 1, "Previous page"},
                         {"enable", 2, "Enable/disable frame"},
                         {"collapse", 3, "Collapse/expand"}}},
    {"VCAudioTriggers", {{"default", 0, "Enable/disable mappings (meters can update while paused)"},
                         {"volumeControl", 1, "Volume control"}}},
    {"VCAnimation",     {{"default", 0, "Intensity fader"}}},
    {"VCLabel",         {}},
};

static int resolveControlId(VCWidget *widget, const QString &sourceName)
{
    if (!widget) return -1;

    // Frame/SoloFrame page shortcuts: "shortcut0", "shortcut1", ...
    if (sourceName.startsWith("shortcut"))
    {
        if (!qobject_cast<VCFrame *>(widget)) return -1;
        bool ok = false;
        int idx = sourceName.mid(8).toInt(&ok);
        if (!ok || idx < 0) return -1;
        return INPUT_SHORTCUT_BASE_ID + idx;
    }

    // Frame/SoloFrame legacy page syntax: "page0", "page1", ...
    if (sourceName.startsWith("page"))
    {
        if (!qobject_cast<VCFrame *>(widget)) return -1;
        bool ok = false;
        int idx = sourceName.mid(4).toInt(&ok);
        if (!ok || idx < 0) return -1;
        return INPUT_SHORTCUT_BASE_ID + idx;
    }

    // XYPad/SpeedDial presets: "preset0", "preset1", ...
    if (sourceName.startsWith("preset"))
    {
        if (!qobject_cast<VCXYPad *>(widget) && !qobject_cast<VCSpeedDial *>(widget))
            return -1;
        bool ok = false;
        int idx = sourceName.mid(6).toInt(&ok);
        if (!ok || idx < 0) return -1;
        return 30 + idx; // INPUT_PRESETS_BASE_ID
    }

    // Static table lookup
    QString typeName = QString::fromLatin1(widget->metaObject()->className());
    auto it = sourceDefTable.constFind(typeName);
    if (it == sourceDefTable.constEnd()) return -1;
    for (const auto &def : it.value())
    {
        if (def.name == sourceName) return def.id;
    }
    return -1;
}

static QVariantMap sourceKeyVariant(const VCBridge::AuthoringSourceKey &key)
{
    QVariantMap map;
    if (key.controlSlotID.has_value())
        map.insert(QStringLiteral("controlSlotID"), int(*key.controlSlotID));
    if (key.inputUniverseID.has_value())
        map.insert(QStringLiteral("inputUniverseID"), int(*key.inputUniverseID));
    if (key.inputChannel.has_value())
        map.insert(QStringLiteral("inputChannel"), int(*key.inputChannel));
    if (key.childSubpage.has_value())
        map.insert(QStringLiteral("childSubpage"), int(*key.childSubpage));
    return map;
}

static VCBridge::AuthoringSourceKey sourceKeyFromSource(const QLCInputSource *source)
{
    VCBridge::AuthoringSourceKey key;
    if (!source)
        return key;
    if (source->id() <= std::numeric_limits<quint8>::max())
        key.controlSlotID = quint8(source->id());
    key.inputUniverseID = source->universe();
    key.inputChannel = source->channel() & 0x0000FFFF;
    key.childSubpage = int(source->page());
    return key;
}

static QString fanoutPhysicalKey(quint32 universe, quint32 physicalChannel)
{
    return QStringLiteral("%1:%2").arg(universe).arg(physicalChannel);
}

static bool hasCrossWidgetPhysicalConflict(VirtualConsole *vc,
                                           VCWidget *targetWidget,
                                           quint32 universe,
                                           quint32 physicalChannel,
                                           const QLCInputSource *ignoreSource = nullptr)
{
    if (!vc || !targetWidget)
        return false;

    QSet<quint32> seenWidgets;
    for (int pageIndex = 0; pageIndex < vc->pagesCount(); ++pageIndex)
    {
        VCPage *page = vc->page(pageIndex);
        if (!page)
            continue;

        QList<VCWidget *> widgets;
        widgets.append(page);
        if (VCFrame *pageFrame = qobject_cast<VCFrame *>(page))
            widgets.append(pageFrame->children(true));

        for (VCWidget *widget : widgets)
        {
            if (!widget || seenWidgets.contains(widget->id()))
                continue;
            seenWidgets.insert(widget->id());
            if (widget == targetWidget)
                continue;

            const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
            for (const QSharedPointer<QLCInputSource> &source : sources)
            {
                if (source.isNull())
                    continue;
                if (ignoreSource && source.data() == ignoreSource)
                    continue;
                if (source->universe() != universe)
                    continue;
                if ((source->channel() & 0x0000FFFF) != physicalChannel)
                    continue;
                return true;
            }
        }
    }
    return false;
}

static QString controllerDestinationDigest(InputOutputMap *ioMap, const VCBridge::AuthoringDestination &destination)
{
    auto canonicalizeVariantForDigest = [](const QVariant &value) -> QByteArray {
        QJsonObject wrapper;
        const char *typeName = value.typeName();
        wrapper.insert(QStringLiteral("variantType"),
                       QString::fromLatin1(typeName ? typeName : "<unknown>"));
        const QJsonValue jsonValue = QJsonValue::fromVariant(value);
        if (jsonValue.isUndefined())
            wrapper.insert(QStringLiteral("valueUndefined"), true);
        else
            wrapper.insert(QStringLiteral("value"), jsonValue);
        return QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    };

    if (!ioMap || !destination.inputUniverseID.has_value() || !destination.inputChannel.has_value() ||
        !destination.profileName.has_value())
    {
        return QStringLiteral("invalid-destination");
    }

    const quint32 universe = *destination.inputUniverseID;
    const quint32 channel = *destination.inputChannel;
    const QString profileName = *destination.profileName;

    QByteArray scope;
    scope += "controller-destination:";
    scope += QByteArray::number(universe);
    scope += ':';
    scope += QByteArray::number(channel);
    scope += ':';
    scope += profileName.toUtf8();
    scope += ':';

    InputPatch *patch = ioMap->inputPatch(universe);
    if (!patch || !patch->isPatched())
    {
        scope += "patch:missing";
        return QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
    }

    scope += "plugin:";
    scope += patch->pluginName().toUtf8();
    scope += ',';
    scope += QByteArray::number(patch->input());
    scope += ",profile:";
    scope += patch->profileName().toUtf8();
    scope += ';';

    const QStringList loadedProfiles = ioMap->profileNames();
    if (!loadedProfiles.contains(profileName))
    {
        scope += "profile:unloaded";
        return QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
    }

    QLCInputProfile *registryProfile = ioMap->profile(profileName);
    if (!registryProfile)
    {
        scope += "profile:missing";
        return QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
    }

    QLCInputProfile *attachedProfile = patch->profile();
    if (!attachedProfile)
    {
        scope += "profile:detached";
        return QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
    }
    scope += "profilePtr:";
    scope += QByteArray::number(quintptr(attachedProfile), 16);
    scope += ',';
    scope += (attachedProfile == registryProfile) ? QByteArrayLiteral("match") : QByteArrayLiteral("mismatch");
    scope += ';';

    QLCInputChannel *profileChannel = attachedProfile->channel(channel);
    if (!profileChannel)
    {
        scope += "channel:missing";
        return QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
    }

    scope += "channel:";
    scope += QByteArray::number(int(profileChannel->type()));
    scope += ',';
    scope += QByteArray::number(int(profileChannel->movementType()));
    scope += ',';
    scope += QByteArray::number(profileChannel->movementSensitivity());
    scope += ',';
    scope += QByteArray::number(int(profileChannel->sendExtraPress()));
    scope += ',';
    scope += QByteArray::number(profileChannel->lowerValue());
    scope += ',';
    scope += QByteArray::number(profileChannel->upperValue());
    scope += ",extraParams:";
    const QVariant extraParams = attachedProfile->channelExtraParams(profileChannel);
    const QByteArray extraCanonical = canonicalizeVariantForDigest(extraParams);
    scope += extraCanonical;
    return QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
}

static QString resolveSourceName(VCWidget *widget, quint32 id)
{
    if (!widget) return QString();

    // Frame shortcuts range
    if (id >= INPUT_SHORTCUT_BASE_ID && qobject_cast<VCFrame *>(widget))
        return QStringLiteral("shortcut%1").arg(id - INPUT_SHORTCUT_BASE_ID);

    // XYPad/SpeedDial presets range
    if (id >= 30 && (qobject_cast<VCXYPad *>(widget) || qobject_cast<VCSpeedDial *>(widget)))
        return QStringLiteral("preset%1").arg(id - 30);

    // Static table reverse lookup
    QString typeName = QString::fromLatin1(widget->metaObject()->className());
    auto defs = sourceDefTable.value(typeName);
    for (const auto &def : defs)
    {
        if (def.id == id) return def.name;
    }
    return QString();
}

static bool hasExternalControlSlot(VCWidget *widget, quint8 controlSlotID)
{
    if (!widget)
        return false;
    const QVariant controls = widget->externalControlsList();
    if (!controls.canConvert<QVariantList>())
        return false;
    const QVariantList controlList = controls.toList();
    for (const QVariant &entry : controlList)
    {
        const QVariantMap map = entry.toMap();
        if (!map.contains(QStringLiteral("mValue")) || !map.value(QStringLiteral("mValue")).canConvert<int>())
            continue;
        const int slot = map.value(QStringLiteral("mValue")).toInt();
        if (slot >= 0 && slot <= std::numeric_limits<quint8>::max() && quint8(slot) == controlSlotID)
            return true;
    }
    return false;
}

static std::optional<bool> keyboardAllowedForSlot(VCWidget *widget, quint8 controlSlotID)
{
    if (!widget)
        return std::nullopt;

    const QString className = QString::fromLatin1(widget->metaObject()->className());
    if (className == QStringLiteral("VCButton"))
        return true;
    if (className == QStringLiteral("VCSlider"))
    {
        if (controlSlotID == 0 || controlSlotID == 1)
            return false;
        if (controlSlotID == 2)
            return true;
        return std::nullopt;
    }
    if (className == QStringLiteral("VCXYPad"))
    {
        if (controlSlotID <= 6)
            return false;
        if (controlSlotID >= 30)
            return true;
        return std::nullopt;
    }
    if (className == QStringLiteral("VCSpeedDial"))
        return controlSlotID == 0 ? std::optional<bool>(false) : std::optional<bool>(true);
    if (className == QStringLiteral("VCCueList"))
    {
        if (controlSlotID <= 3)
            return true;
        if (controlSlotID == 4)
            return false;
        return std::nullopt;
    }
    if (className == QStringLiteral("VCFrame") || className == QStringLiteral("VCSoloFrame"))
        return (controlSlotID <= 3 || controlSlotID >= INPUT_SHORTCUT_BASE_ID) ? std::optional<bool>(true) : std::nullopt;
    if (className == QStringLiteral("VCAudioTriggers"))
        return controlSlotID == 0 ? std::optional<bool>(true) : std::optional<bool>(false);
    if (className == QStringLiteral("VCClock"))
        return controlSlotID <= 1 ? std::optional<bool>(true) : std::nullopt;
    if (className == QStringLiteral("VCAnimation"))
        return controlSlotID == 0 ? std::optional<bool>(true) : std::nullopt;
    return std::nullopt;
}

VCBridgeV5::VCBridgeV5(Doc *doc, VirtualConsole *vc, App *authoringApp)
    : m_doc(doc)
    , m_vc(vc)
    , m_authoringApp(authoringApp)
{
    m_authoringMonotonicClock.start();
    if (m_doc)
        m_docDestroyedConn = QObject::connect(m_doc, &QObject::destroyed, [this]() {
            clearAuthoringRecords();
            m_doc = nullptr;
        });
    if (m_vc)
        m_vcDestroyedConn = QObject::connect(m_vc, &QObject::destroyed, [this]() {
            clearAuthoringRecords();
            m_vc = nullptr;
        });
    if (m_authoringApp)
        m_appDestroyedConn = QObject::connect(m_authoringApp, &QObject::destroyed, [this]() {
            clearAuthoringRecords();
            m_authoringApp = nullptr;
        });
}

VCBridgeV5::~VCBridgeV5()
{
    if (m_docDestroyedConn)
        QObject::disconnect(m_docDestroyedConn);
    if (m_vcDestroyedConn)
        QObject::disconnect(m_vcDestroyedConn);
    if (m_appDestroyedConn)
        QObject::disconnect(m_appDestroyedConn);
    clearAuthoringRecords();
}

bool VCBridgeV5::hasMatchingAuthoringContext() const
{
    if (!m_authoringApp || !m_doc || !m_vc)
        return false;
    if (m_authoringApp->doc() != m_doc)
        return false;
    if (m_authoringApp->virtualConsole() != m_vc)
        return false;
    return true;
}

QVariantMap VCBridgeV5::setPerformMode(bool on)
{
    if (!hasMatchingAuthoringContext())
        return {{QStringLiteral("error"), QStringLiteral("DJ control requires the application context")}};
    DjManager *manager = m_authoringApp->findChild<DjManager *>();
    if (!manager)
        return {{QStringLiteral("error"), QStringLiteral("DJ Manager is unavailable")}};
    manager->setPerformMode(on);
    return djState();
}

QVariantMap VCBridgeV5::djState() const
{
    if (!hasMatchingAuthoringContext())
        return {{QStringLiteral("error"), QStringLiteral("DJ control requires the application context")}};
    const VdjBridge *bridge = m_authoringApp->findChild<VdjBridge *>();
    if (!bridge)
        return {{QStringLiteral("error"), QStringLiteral("DJ bridge is unavailable")}};
    const DjFsm *fsm = bridge->djFsm();
    QVariantList decks;
    for (int index = 0; index < fsm->deckCount(); ++index)
    {
        const auto &deck = fsm->deckAt(index);
        decks.append(QVariantMap{
            {QStringLiteral("deck"), deck.number},
            {QStringLiteral("state"), DjFsm::stateName(deck.state)},
            {QStringLiteral("filepath"), deck.song.filepath},
            {QStringLiteral("title"), deck.song.title},
            {QStringLiteral("artist"), deck.song.artist},
            {QStringLiteral("bpm"), deck.song.bpm},
            {QStringLiteral("elapsedMs"), deck.elapsedMs}
        });
    }
    const quint32 showId = bridge->performFsm()->activeShowId();
    return {
        {QStringLiteral("performMode"), bridge->performMode()},
        {QStringLiteral("performState"), PerformFsm::stateToString(bridge->performState())},
        {QStringLiteral("activeShowId"), showId == Function::invalidId() ? QVariant() : QVariant(showId)},
        {QStringLiteral("activeDeck"), fsm->activeDeck()},
        {QStringLiteral("decks"), decks}
    };
}

QVariantMap VCBridgeV5::setRecording(bool on)
{
    if (!hasMatchingAuthoringContext())
        return {{QStringLiteral("error"), QStringLiteral("Recording control requires the application context")}};
    auto *recorder = m_authoringApp->findChild<ShowCommandRecorder *>();
    if (!recorder)
        return {{QStringLiteral("error"), QStringLiteral("Command recorder is unavailable")}};
    if (!recorder->setRecording(on))
    {
        QVariantMap result = recordingState();
        result.insert(QStringLiteral("error"), recorder->lastError());
        return result;
    }
    return recordingState();
}

QVariantMap VCBridgeV5::recordingState() const
{
    if (!hasMatchingAuthoringContext())
        return {{QStringLiteral("error"), QStringLiteral("Recording control requires the application context")}};
    const auto *recorder = m_authoringApp->findChild<ShowCommandRecorder *>();
    if (!recorder)
        return {{QStringLiteral("error"), QStringLiteral("Command recorder is unavailable")}};
    const quint32 showId = recorder->boundShowId();
    return {
        {QStringLiteral("recording"), recorder->isRecording()},
        {QStringLiteral("phase"), recorder->phaseName()},
        {QStringLiteral("boundShowId"), showId == Function::invalidId() ? QVariant() : QVariant(showId)},
        {QStringLiteral("lastError"), recorder->lastError()}
    };
}

QVariantMap VCBridgeV5::pressWidget(int widgetID, const QString &action)
{
    if (!hasMatchingAuthoringContext())
        return {{QStringLiteral("error"), QStringLiteral("Live VC input requires the application context")}};
    auto *button = qobject_cast<VCButton *>(m_vc->widget(widgetID));
    if (!button)
        return {{QStringLiteral("error"), QStringLiteral("widget must identify a VC button")}};
    const bool releasingHold = action == QStringLiteral("release")
                           && button->actionType() == VCButton::FreezeHold;
    if (m_vc->editMode() && !releasingHold)
        return {{QStringLiteral("error"), QStringLiteral("Virtual Console is in edit mode")}};
    if (button->isDisabled() || !button->isVisible())
        return {{QStringLiteral("error"), QStringLiteral("button is disabled or hidden")}};
    if ((button->actionType() == VCButton::Toggle || button->actionType() == VCButton::Flash)
        && !m_doc->function(button->functionID()))
        return {{QStringLiteral("error"), QStringLiteral("button has no bound function")}};

    const bool momentary = button->actionType() == VCButton::Flash
                        || button->actionType() == VCButton::FreezeHold;
    if (action != QStringLiteral("release") || momentary)
        button->requestUserStateChange(momentary && action == QStringLiteral("toggle")
            ? button->state() != VCButton::Active : action != QStringLiteral("release"));
    return {{QStringLiteral("status"), QStringLiteral("accepted")},
            {QStringLiteral("widget"), widgetID},
            {QStringLiteral("action"), action}};
}

QString VCBridgeV5::authoringBasis() const
{
    QByteArray canonical = "vcstruct-v2;";
    canonical += QByteArray::number(m_vc->pagesCount());
    canonical += ';';
    for (int i = 0; i < m_vc->pagesCount(); ++i)
    {
        VCPage *page = m_vc->page(i);
        if (!page)
            continue;
        canonical += "p:";
        canonical += QByteArray::number(i);
        canonical += ':';
        canonical += QByteArray::number(page->id());
        canonical += ':';
        canonical += page->caption().toUtf8();
        canonical += ':';
        canonical += VCPage::externalInputModeToString(page->externalInputMode()).toUtf8();
        canonical += ';';
        const QList<VCWidget *> widgets = page->children(true);
        for (int order = 0; order < widgets.size(); ++order)
        {
            VCWidget *widget = widgets.at(order);
            if (!widget)
                continue;
            VCWidget *parent = qobject_cast<VCWidget *>(widget->parent());
            canonical += "w:";
            canonical += QByteArray::number(order);
            canonical += ':';
            canonical += QByteArray::number(widget->id());
            canonical += ':';
            canonical += QByteArray(widget->metaObject()->className());
            canonical += ':';
            canonical += QByteArray::number(parent ? parent->id() : -1);
            canonical += ':';
            canonical += QByteArray::number(widget->page());
            canonical += ':';
            canonical += widget->caption().toUtf8();
            canonical += ':';
            const QRectF rect = widget->logicalGeometry();
            canonical += QByteArray::number(rect.x(), 'f', 6);
            canonical += ',';
            canonical += QByteArray::number(rect.y(), 'f', 6);
            canonical += ',';
            canonical += QByteArray::number(rect.width(), 'f', 6);
            canonical += ',';
            canonical += QByteArray::number(rect.height(), 'f', 6);
            if (VCFrame *frame = qobject_cast<VCFrame *>(widget))
            {
                canonical += ':';
                canonical += QByteArray::number(frame->totalPagesNumber());
                canonical += ':';
                canonical += frame->pagesLoop() ? "1" : "0";
                canonical += ':';
                const QStringList labels = frame->pageLabels();
                for (int li = 0; li < labels.size(); ++li)
                {
                    if (li > 0)
                        canonical += '|';
                    canonical += labels.at(li).toUtf8();
                }
            }
            canonical += ';';
        }
    }
    return QString::fromLatin1(QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex());
}

QByteArray VCBridgeV5::canonicalizeLayoutRequest(const AuthoringLayoutRequest &request) const
{
    QByteArray canonical;
    canonical.reserve(256 + request.geometryChanges.size() * 96 + request.expectedBasis.size());

    auto appendBytes = [](QByteArray &out, const char *key, const QByteArray &bytes) {
        out += key;
        out += '=';
        out += QByteArray::number(bytes.size());
        out += ':';
        out += bytes;
        out += ';';
    };
    auto appendNumber = [&](const char *key, qlonglong value) {
        appendBytes(canonical, key, QByteArray::number(value));
    };
    auto appendBool = [&](const char *key, bool value) {
        appendBytes(canonical, key, value ? QByteArrayLiteral("1") : QByteArrayLiteral("0"));
    };
    auto appendOptInt = [&](const char *key, const std::optional<qint64> &value) {
        if (!value.has_value())
            appendBytes(canonical, key, QByteArrayLiteral("M"));
        else
            appendBytes(canonical, key, QByteArrayLiteral("P") + QByteArray::number(value.value()));
    };
    auto appendOptString = [&](const char *key, const std::optional<QString> &value) {
        if (!value.has_value())
            appendBytes(canonical, key, QByteArrayLiteral("M"));
        else
            appendBytes(canonical, key, QByteArrayLiteral("P") + value->toUtf8());
    };
    auto appendOptPoint = [&](const char *key, const std::optional<QPointF> &value) {
        if (!value.has_value())
            appendBytes(canonical, key, QByteArrayLiteral("M"));
        else
        {
            QByteArray point;
            point += QByteArray::number(value->x(), 'f', 6);
            point += ',';
            point += QByteArray::number(value->y(), 'f', 6);
            appendBytes(canonical, key, QByteArrayLiteral("P") + point);
        }
    };

    appendBytes(canonical, "family", QByteArrayLiteral("layout"));
    appendNumber("operation", int(request.operation));
    appendBytes(canonical, "expectedBasis", request.expectedBasis.toUtf8());
    appendBool("snapToGrid", request.snapToGrid);
    appendBool("requireAtomic", request.requireAtomic);
    appendBytes(canonical, "diffDetail", request.diffDetail.toUtf8());
    appendOptInt("destinationFrameWidgetID", request.destinationFrameWidgetID.has_value()
        ? std::optional<qint64>(qint64(request.destinationFrameWidgetID.value()))
        : std::nullopt);
    appendOptInt("destinationSubpage", request.destinationSubpage.has_value()
        ? std::optional<qint64>(qint64(request.destinationSubpage.value()))
        : std::nullopt);
    appendOptInt("beforeWidgetID", request.beforeWidgetID.has_value()
        ? std::optional<qint64>(qint64(request.beforeWidgetID.value()))
        : std::nullopt);
    appendOptInt("anchorWidgetID", request.anchorWidgetID.has_value()
        ? std::optional<qint64>(qint64(request.anchorWidgetID.value()))
        : std::nullopt);
    appendOptPoint("logicalAnchor", request.logicalAnchor);
    appendOptInt("frameWidgetID", request.frameWidgetID.has_value()
        ? std::optional<qint64>(qint64(request.frameWidgetID.value()))
        : std::nullopt);
    appendOptInt("pageWidgetID", request.pageWidgetID.has_value()
        ? std::optional<qint64>(qint64(request.pageWidgetID.value()))
        : std::nullopt);
    appendOptInt("columns", request.columns.has_value()
        ? std::optional<qint64>(qint64(request.columns.value()))
        : std::nullopt);
    appendOptInt("padding", request.padding.has_value()
        ? std::optional<qint64>(qint64(request.padding.value()))
        : std::nullopt);
    appendOptString("name", request.name);
    appendOptInt("totalPages", request.totalPages.has_value()
        ? std::optional<qint64>(qint64(request.totalPages.value()))
        : std::nullopt);
    if (!request.pageLabels.has_value())
    {
        appendBytes(canonical, "pageLabels", QByteArrayLiteral("M"));
    }
    else
    {
        QByteArray labelsPayload("P");
        labelsPayload += QByteArray::number(request.pageLabels->size());
        labelsPayload += ':';
        for (const QString &label : request.pageLabels.value())
        {
            const QByteArray bytes = label.toUtf8();
            labelsPayload += QByteArray::number(bytes.size());
            labelsPayload += ':';
            labelsPayload += bytes;
            labelsPayload += ';';
        }
        appendBytes(canonical, "pageLabels", labelsPayload);
    }
    if (!request.pagesLoop.has_value())
        appendBytes(canonical, "pagesLoop", QByteArrayLiteral("M"));
    else
        appendBytes(canonical, "pagesLoop", request.pagesLoop.value() ? QByteArrayLiteral("P1") : QByteArrayLiteral("P0"));

    QSet<quint32> seen;
    bool duplicate = false;
    for (const AuthoringGeometryChange &change : request.geometryChanges)
    {
        if (seen.contains(change.widgetID))
            duplicate = true;
        seen.insert(change.widgetID);
    }
    appendBool("duplicateWidgetIDs", duplicate);
    QList<quint32> sortedIds = seen.values();
    std::sort(sortedIds.begin(), sortedIds.end());
    canonical += "sortedWidgetIDs=";
    for (quint32 id : sortedIds)
    {
        canonical += QByteArray::number(id);
        canonical += ',';
    }
    canonical += ';';
    canonical += "requestWidgetOrder=";
    for (quint32 id : request.widgetIDs)
    {
        canonical += QByteArray::number(id);
        canonical += ',';
    }
    canonical += ';';
    for (const AuthoringGeometryChange &change : request.geometryChanges)
    {
        QByteArray payload;
        payload += QByteArray::number(change.widgetID);
        payload += ':';
        payload += QByteArray::number(change.logicalRect.x(), 'f', 6);
        payload += ',';
        payload += QByteArray::number(change.logicalRect.y(), 'f', 6);
        payload += ',';
        payload += QByteArray::number(change.logicalRect.width(), 'f', 6);
        payload += ',';
        payload += QByteArray::number(change.logicalRect.height(), 'f', 6);
        appendBytes(canonical, "geometryChange", payload);
    }
    return canonical;
}

QByteArray VCBridgeV5::canonicalizeRequest(const AuthoringRequest &request) const
{
    auto appendBytes = [](QByteArray &out, const char *key, const QByteArray &bytes) {
        out += key;
        out += '=';
        out += QByteArray::number(bytes.size());
        out += ':';
        out += bytes;
        out += ';';
    };

    if (const AuthoringLayoutRequest *layout = std::get_if<AuthoringLayoutRequest>(&request))
        return canonicalizeLayoutRequest(*layout);

    if (const AuthoringBindingsRequest *bindings = std::get_if<AuthoringBindingsRequest>(&request))
    {
        QByteArray canonical;
        appendBytes(canonical, "family", QByteArrayLiteral("bindings"));
        appendBytes(canonical, "expectedBasis", bindings->expectedBasis.toUtf8());
        appendBytes(canonical, "allowFanout", bindings->allowFanout ? QByteArrayLiteral("1") : QByteArrayLiteral("0"));
        appendBytes(canonical, "requireAtomic", bindings->requireAtomic ? QByteArrayLiteral("1") : QByteArrayLiteral("0"));
        appendBytes(canonical, "diffDetail", bindings->diffDetail.toUtf8());
        appendBytes(canonical, "count", QByteArray::number(bindings->items.size()));
        for (const AuthoringBindingItem &item : bindings->items)
        {
            QByteArray row;
            row += QByteArray::number(item.widgetID) + ':';
            row += QByteArray::number(int(item.kind)) + ':';
            row += QByteArray::number(int(item.action)) + ':';
            row += item.controlSlotID.has_value()
                ? QByteArrayLiteral("P") + QByteArray::number(*item.controlSlotID) : QByteArrayLiteral("M");
            row += ':';
            row += item.functionID.has_value()
                ? QByteArrayLiteral("P") + QByteArray::number(*item.functionID) : QByteArrayLiteral("M");
            row += ':';
            if (item.sourceKey.has_value())
            {
                const AuthoringSourceKey &key = item.sourceKey.value();
                row += "P(";
                row += key.controlSlotID.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*key.controlSlotID) : QByteArrayLiteral("M");
                row += ',';
                row += key.inputUniverseID.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*key.inputUniverseID) : QByteArrayLiteral("M");
                row += ',';
                row += key.inputChannel.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*key.inputChannel) : QByteArrayLiteral("M");
                row += ',';
                row += key.childSubpage.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*key.childSubpage) : QByteArrayLiteral("M");
                row += ')';
            }
            else
                row += 'M';
            row += ':';
            if (item.destination.has_value())
            {
                const AuthoringDestination &dst = item.destination.value();
                row += "P(";
                row += dst.inputUniverseID.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*dst.inputUniverseID) : QByteArrayLiteral("M");
                row += ',';
                row += dst.inputChannel.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*dst.inputChannel) : QByteArrayLiteral("M");
                row += ',';
                if (dst.profileName.has_value())
                {
                    const QByteArray profile = dst.profileName->toUtf8();
                    row += QByteArrayLiteral("P") + QByteArray::number(profile.size()) + ':' + profile;
                }
                else
                    row += 'M';
                row += ')';
            }
            else
                row += 'M';
            row += ':';
            if (item.keySequence.has_value())
            {
                const QByteArray key = item.keySequence->toUtf8();
                row += QByteArrayLiteral("P") + QByteArray::number(key.size()) + ':' + key;
            }
            else
                row += 'M';
            row += ':';
            row += item.idleValue.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*item.idleValue) : QByteArrayLiteral("M");
            row += ',';
            row += item.activeValue.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*item.activeValue) : QByteArrayLiteral("M");
            row += ',';
            row += item.monitorValue.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*item.monitorValue) : QByteArrayLiteral("M");
            row += ':';
            row += item.idleChannel.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*item.idleChannel) : QByteArrayLiteral("M");
            row += ',';
            row += item.activeChannel.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*item.activeChannel) : QByteArrayLiteral("M");
            row += ',';
            row += item.monitorChannel.has_value() ? QByteArrayLiteral("P") + QByteArray::number(*item.monitorChannel) : QByteArrayLiteral("M");
            appendBytes(canonical, "item", row);
        }
        return canonical;
    }

    const AuthoringRecordPanelRequest *panels = std::get_if<AuthoringRecordPanelRequest>(&request);
    QByteArray canonical;
    appendBytes(canonical, "family", QByteArrayLiteral("record-panel"));
    appendBytes(canonical, "expectedBasis", panels ? panels->expectedBasis.toUtf8() : QByteArray());
    appendBytes(canonical, "requireAtomic", panels && panels->requireAtomic ? QByteArrayLiteral("1") : QByteArrayLiteral("0"));
    appendBytes(canonical, "diffDetail", panels ? panels->diffDetail.toUtf8() : QByteArrayLiteral("summary"));
    appendBytes(canonical, "count", QByteArray::number(panels ? panels->items.size() : 0));
    if (panels)
    {
        for (const AuthoringRecordPanelItem &item : panels->items)
        {
            QByteArray row;
            row += QByteArray::number(item.widgetID);
            row += ':';
            auto appendPanelOptString = [&](const std::optional<QString> &value) {
                if (!value.has_value())
                    row += 'M';
                else
                {
                    const QByteArray bytes = value->toUtf8();
                    row += QByteArrayLiteral("P") + QByteArray::number(bytes.size()) + ':' + bytes;
                }
            };
            auto appendPanelOptUInt = [&](const std::optional<uint> &value) {
                if (!value.has_value())
                    row += 'M';
                else
                    row += QByteArrayLiteral("P") + QByteArray::number(*value);
            };
            appendPanelOptString(item.targetFolder);
            row += ':';
            appendPanelOptString(item.scenePrefix);
            row += ':';
            appendPanelOptString(item.chaserPrefix);
            row += ':';
            appendPanelOptUInt(item.defaultFadeIn);
            row += ',';
            appendPanelOptUInt(item.defaultHold);
            row += ',';
            appendPanelOptUInt(item.defaultFadeOut);
            appendBytes(canonical, "item", row);
        }
    }
    return canonical;
}

QString VCBridgeV5::candidateDigest(const QMap<quint32, QRectF> &rectangles) const
{
    QByteArray canonical;
    canonical.reserve(rectangles.size() * 64);
    for (auto it = rectangles.constBegin(); it != rectangles.constEnd(); ++it)
    {
        canonical += QByteArray::number(it.key());
        canonical += ':';
        canonical += QByteArray::number(it.value().x(), 'f', 6);
        canonical += ',';
        canonical += QByteArray::number(it.value().y(), 'f', 6);
        canonical += ',';
        canonical += QByteArray::number(it.value().width(), 'f', 6);
        canonical += ',';
        canonical += QByteArray::number(it.value().height(), 'f', 6);
        canonical += ';';
    }
    return QString::fromLatin1(QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex());
}

int VCBridgeV5::estimateGuardCount(const QMap<quint32, QRectF> &rectangles) const
{
    if (!m_vc)
        return 0;
    QSet<quint32> guardedIds;
    for (auto it = rectangles.constBegin(); it != rectangles.constEnd(); ++it)
    {
        VCWidget *widget = m_vc->widget(it.key());
        if (!widget)
            continue;
        guardedIds.insert(widget->id());
        for (QObject *parent = widget->parent(); parent; parent = parent->parent())
            if (VCWidget *parentWidget = qobject_cast<VCWidget *>(parent))
                guardedIds.insert(parentWidget->id());
        if (VCFrame *frame = qobject_cast<VCFrame *>(widget))
            for (VCWidget *child : frame->children(true))
                guardedIds.insert(child->id());
    }
    return guardedIds.size();
}

QList<quint32> VCBridgeV5::widgetIdsFromRequest(const AuthoringRequest &request) const
{
    QList<quint32> ids;
    if (const AuthoringLayoutRequest *layout = std::get_if<AuthoringLayoutRequest>(&request))
    {
        for (const AuthoringGeometryChange &change : layout->geometryChanges)
            ids.append(change.widgetID);
        ids.append(layout->widgetIDs);
        if (layout->destinationFrameWidgetID.has_value())
            ids.append(layout->destinationFrameWidgetID.value());
        if (layout->frameWidgetID.has_value())
            ids.append(layout->frameWidgetID.value());
        if (layout->pageWidgetID.has_value())
            ids.append(layout->pageWidgetID.value());
        return ids;
    }

    if (const AuthoringBindingsRequest *bindings = std::get_if<AuthoringBindingsRequest>(&request))
    {
        for (const AuthoringBindingItem &item : bindings->items)
            ids.append(item.widgetID);
        return ids;
    }

    if (const AuthoringRecordPanelRequest *panels = std::get_if<AuthoringRecordPanelRequest>(&request))
    {
        for (const AuthoringRecordPanelItem &item : panels->items)
            ids.append(item.widgetID);
    }
    return ids;
}

int VCBridgeV5::requestItemCount(const AuthoringRequest &request) const
{
    if (const AuthoringLayoutRequest *layout = std::get_if<AuthoringLayoutRequest>(&request))
    {
        switch (layout->operation)
        {
        case AuthoringLayoutRequest::Operation::Geometry:
            return layout->geometryChanges.size();
        case AuthoringLayoutRequest::Operation::Reorder:
        case AuthoringLayoutRequest::Operation::MoveGroup:
            return layout->widgetIDs.size();
        case AuthoringLayoutRequest::Operation::Align:
            return 1;
        case AuthoringLayoutRequest::Operation::PageCreate:
            return 1;
        case AuthoringLayoutRequest::Operation::PageRename:
        case AuthoringLayoutRequest::Operation::PageDelete:
            return 1;
        case AuthoringLayoutRequest::Operation::FramePages:
            return 1;
        }
    }
    if (const AuthoringBindingsRequest *bindings = std::get_if<AuthoringBindingsRequest>(&request))
        return bindings->items.size();
    if (const AuthoringRecordPanelRequest *panels = std::get_if<AuthoringRecordPanelRequest>(&request))
        return panels->items.size();
    return 0;
}

int VCBridgeV5::rootIdentityCount(const AuthoringRequest &request) const
{
    const QList<quint32> ids = widgetIdsFromRequest(request);
    QSet<quint32> unique;
    for (quint32 id : ids)
        unique.insert(id);
    return unique.size();
}

QString VCBridgeV5::scopedConfigurationDigest(VCWidget *widget) const
{
    if (!widget)
        return QString();

    QByteArray scope;
    scope += QByteArray(widget->metaObject()->className());
    scope += ':';
    scope += QByteArray::number(widget->id());
    scope += ':';
    scope += QByteArray::number(widget->page());
    scope += ':';
    VCWidget *parentWidget = qobject_cast<VCWidget *>(widget->parent());
    scope += QByteArray::number(parentWidget ? parentWidget->id() : -1);
    scope += ':';
    scope += widget->caption().toUtf8();
    scope += ':';
    scope += widget->isDisabled() ? "1" : "0";
    scope += ':';

    const QMap<QKeySequence, quint32> keyMap = widget->keySequenceMap();
    scope += QByteArray::number(keyMap.size());
    scope += ';';
    for (auto it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
    {
        const QByteArray key = it.key().toString(QKeySequence::PortableText).toUtf8();
        scope += QByteArray::number(key.size());
        scope += ':';
        scope += key;
        scope += '=';
        scope += QByteArray::number(it.value());
        scope += ';';
    }

    if (std::optional<quint32> functionID = selectedFunctionId(widget))
    {
        scope += "fn:";
        scope += QByteArray::number(*functionID);
        scope += ';';
    }

    if (VCFrame *frame = qobject_cast<VCFrame *>(widget))
    {
        scope += "frame:";
        scope += QByteArray::number(frame->totalPagesNumber());
        scope += ',';
        scope += frame->pagesLoop() ? '1' : '0';
        scope += ',';
        scope += frame->showHeader() ? '1' : '0';
        scope += ',';
        scope += frame->showEnable() ? '1' : '0';
        scope += ';';
        const QStringList labels = frame->pageLabels();
        scope += QByteArray::number(labels.size());
        scope += ':';
        for (const QString &label : labels)
        {
            const QByteArray bytes = label.toUtf8();
            scope += QByteArray::number(bytes.size());
            scope += ':';
            scope += bytes;
            scope += ';';
        }
    }

    if (VCRecordPanel *panel = qobject_cast<VCRecordPanel *>(widget))
    {
        const QByteArray folder = panel->targetFolder().toUtf8();
        const QByteArray scenePrefix = panel->scenePrefix().toUtf8();
        const QByteArray chaserPrefix = panel->chaserPrefix().toUtf8();
        scope += "record:";
        scope += QByteArray::number(folder.size()) + ':' + folder + ';';
        scope += QByteArray::number(scenePrefix.size()) + ':' + scenePrefix + ';';
        scope += QByteArray::number(chaserPrefix.size()) + ':' + chaserPrefix + ';';
        scope += QByteArray::number(panel->defaultFadeIn()) + ',';
        scope += QByteArray::number(panel->defaultHold()) + ',';
        scope += QByteArray::number(panel->defaultFadeOut()) + ';';
    }

    InputOutputMap *ioMap = m_doc ? m_doc->inputOutputMap() : nullptr;
    const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
    scope += "sources:";
    scope += QByteArray::number(sources.size());
    scope += ';';
    for (int i = 0; i < sources.size(); ++i)
    {
        const QSharedPointer<QLCInputSource> &source = sources.at(i);
        if (source.isNull())
        {
            scope += QByteArray::number(i) + ":null;";
            continue;
        }
        scope += QByteArray::number(i);
        scope += ':';
        scope += QByteArray::number(source->id());
        scope += ',';
        scope += QByteArray::number(source->universe());
        scope += ',';
        scope += QByteArray::number(source->channel());
        scope += ',';
        scope += QByteArray::number(source->page());
        scope += ',';
        scope += QByteArray::number(int(source->workingMode()));
        scope += ',';
        scope += QByteArray::number(source->sensitivity());
        scope += ',';
        scope += QByteArray::number(source->feedbackValue(QLCInputFeedback::LowerValue));
        scope += ',';
        scope += QByteArray::number(source->feedbackValue(QLCInputFeedback::UpperValue));
        scope += ',';
        scope += QByteArray::number(source->feedbackValue(QLCInputFeedback::MonitorValue));
        scope += ',';
        scope += QByteArray::number(source->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt());
        scope += ',';
        scope += QByteArray::number(source->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt());
        scope += ',';
        scope += QByteArray::number(source->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt());
        if (ioMap)
        {
            InputPatch *patch = ioMap->inputPatch(source->universe());
            if (patch && patch->isPatched() && !patch->profileName().isEmpty() && patch->profileName() != KInputNone)
            {
                const QByteArray profileName = patch->profileName().toUtf8();
                scope += ",profile:";
                scope += QByteArray::number(profileName.size());
                scope += ':';
                scope += profileName;
            }
        }
        scope += ';';
    }
    return QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
}

QString VCBridgeV5::scopedConfigurationDigest(const AuthoringSourceKey &key, const QLCInputSource *source) const
{
    if (!source)
        return QString();
    QByteArray scope;
    scope += "source:";
    if (key.controlSlotID.has_value())
        scope += QByteArray::number(*key.controlSlotID);
    scope += ':';
    if (key.inputUniverseID.has_value())
        scope += QByteArray::number(*key.inputUniverseID);
    scope += ':';
    if (key.inputChannel.has_value())
        scope += QByteArray::number(*key.inputChannel);
    scope += ':';
    if (key.childSubpage.has_value())
        scope += QByteArray::number(*key.childSubpage);
    scope += '|';
    scope += QByteArray::number(source->id());
    scope += ':';
    scope += QByteArray::number(source->universe());
    scope += ':';
    scope += QByteArray::number(source->channel());
    scope += ':';
    scope += QByteArray::number(source->page());
    scope += ':';
    scope += QByteArray::number(int(source->workingMode()));
    scope += ':';
    scope += QByteArray::number(source->sensitivity());
    scope += ':';
    scope += QByteArray::number(source->feedbackValue(QLCInputFeedback::LowerValue));
    scope += ',';
    scope += QByteArray::number(source->feedbackValue(QLCInputFeedback::UpperValue));
    scope += ',';
    scope += QByteArray::number(source->feedbackValue(QLCInputFeedback::MonitorValue));
    scope += ':';
    scope += QByteArray::number(source->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt());
    scope += ',';
    scope += QByteArray::number(source->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt());
    scope += ',';
    scope += QByteArray::number(source->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt());
    return QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
}

QString VCBridgeV5::scopedConfigurationDigest(const Function *function) const
{
    if (!function)
        return QString();
    QByteArray scope;
    scope += "function:";
    scope += QByteArray::number(function->id());
    scope += ':';
    scope += QByteArray::number(int(function->type()));
    scope += ':';
    scope += function->name().toUtf8();
    scope += ':';
    scope += QByteArray::number(int(function->runOrder()));
    scope += ':';
    scope += QByteArray::number(int(function->tempoType()));
    return QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
}

QObject *VCBridgeV5::resolveGuardObject(const GuardKey &key) const
{
    switch (key.kind)
    {
    case GuardKey::Kind::Widget:
        return (m_vc && key.widgetID.has_value()) ? m_vc->widget(*key.widgetID) : nullptr;
    case GuardKey::Kind::Function:
        return (m_doc && key.functionID.has_value()) ? m_doc->function(*key.functionID) : nullptr;
    case GuardKey::Kind::InputSource:
        break;
    }

    if (!m_vc || !key.widgetID.has_value() || !key.sourceKey.has_value())
        return nullptr;
    VCWidget *widget = m_vc->widget(*key.widgetID);
    if (!widget)
        return nullptr;

    QList<QSharedPointer<QLCInputSource>> matches;
    const AuthoringSourceKey &requested = *key.sourceKey;
    const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
    for (const QSharedPointer<QLCInputSource> &source : sources)
    {
        if (source.isNull())
            continue;
        if (requested.controlSlotID.has_value() && source->id() != *requested.controlSlotID)
            continue;
        if (requested.inputUniverseID.has_value() && source->universe() != *requested.inputUniverseID)
            continue;
        if (requested.inputChannel.has_value() &&
            (source->channel() & 0x0000FFFF) != *requested.inputChannel)
            continue;
        if (requested.childSubpage.has_value() && int(source->page()) != *requested.childSubpage)
            continue;
        matches.append(source);
    }

    if (matches.size() != 1)
        return nullptr;
    return matches.first().data();
}

QString VCBridgeV5::scopedConfigurationDigest(const GuardKey &key) const
{
    QObject *resolved = resolveGuardObject(key);
    if (!resolved)
        return QString();

    switch (key.kind)
    {
    case GuardKey::Kind::Widget:
        return scopedConfigurationDigest(qobject_cast<VCWidget *>(resolved));
    case GuardKey::Kind::Function:
        return scopedConfigurationDigest(qobject_cast<Function *>(resolved));
    case GuardKey::Kind::InputSource:
        return key.sourceKey.has_value()
            ? scopedConfigurationDigest(*key.sourceKey, qobject_cast<QLCInputSource *>(resolved))
            : QString();
    }

    return QString();
}

std::optional<quint32> VCBridgeV5::owningPageWidgetId(VCWidget *widget) const
{
    for (QObject *owner = widget; owner; owner = owner->parent())
    {
        if (VCPage *page = qobject_cast<VCPage *>(owner))
            return page->id();
    }
    return std::nullopt;
}

std::optional<quint32> VCBridgeV5::selectedFunctionId(VCWidget *widget) const
{
    if (!widget)
        return std::nullopt;
    auto valid = [](quint32 id) -> std::optional<quint32> {
        return id == Function::invalidId() ? std::nullopt : std::optional<quint32>(id);
    };
    if (VCButton *button = qobject_cast<VCButton *>(widget))
        return valid(button->functionID());
    if (VCSlider *slider = qobject_cast<VCSlider *>(widget))
        return valid(slider->controlledFunction());
    if (VCCueList *cueList = qobject_cast<VCCueList *>(widget))
        return valid(cueList->chaserID());
    if (VCAnimation *animation = qobject_cast<VCAnimation *>(widget))
        return valid(animation->functionID());
    return std::nullopt;
}

std::optional<VCBridge::AuthoringSourceKey> VCBridgeV5::selectedSourceKey(VCWidget *widget, std::optional<int> *ordinal) const
{
    if (ordinal)
        ordinal->reset();
    if (!widget)
        return std::nullopt;

    const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
    int selectedIndex = -1;
    const QLCInputSource *selected = nullptr;
    for (int i = 0; i < sources.size(); ++i)
    {
        if (sources.at(i).isNull())
            continue;
        if (selected)
            return std::nullopt;
        selected = sources.at(i).data();
        selectedIndex = i;
    }
    if (!selected)
        return std::nullopt;

    AuthoringSourceKey key;
    if (selected->id() <= std::numeric_limits<quint8>::max())
        key.controlSlotID = quint8(selected->id());
    key.inputUniverseID = selected->universe();
    key.inputChannel = selected->channel();
    key.childSubpage = int(selected->page());
    if (ordinal)
        *ordinal = selectedIndex;
    return key;
}

void VCBridgeV5::populateSelectionDetails(AuthoringPreparationItem &item, VCWidget *widget) const
{
    if (!widget)
        return;
    item.selectedPageWidgetID = owningPageWidgetId(widget);
    if (VCWidget *parentWidget = qobject_cast<VCWidget *>(widget->parent()))
        item.selectedParentWidgetID = parentWidget->id();
    else
        item.selectedParentWidgetID = -1;
    item.selectedChildSubpage = widget->page();
    item.configurationDigest = scopedConfigurationDigest(widget);
    item.selectedFunctionID = selectedFunctionId(widget);
    item.selectedFunctionBeforeID = item.selectedFunctionID;
    item.selectedFunctionAfterID = item.selectedFunctionID;
    item.selectedSourceKey = selectedSourceKey(widget, &item.sourceOrdinal);
    if (item.selectedSourceKey.has_value())
        item.selectedControlSlotID = item.selectedSourceKey->controlSlotID;
}

void VCBridgeV5::populateSelectionDetails(AuthoringApplyItem &item, VCWidget *widget) const
{
    if (!widget)
        return;
    item.selectedPageWidgetID = owningPageWidgetId(widget);
    if (VCWidget *parentWidget = qobject_cast<VCWidget *>(widget->parent()))
        item.selectedParentWidgetID = parentWidget->id();
    else
        item.selectedParentWidgetID = -1;
    item.selectedChildSubpage = widget->page();
    item.configurationDigest = scopedConfigurationDigest(widget);
    item.selectedFunctionID = selectedFunctionId(widget);
    item.selectedFunctionBeforeID = item.selectedFunctionID;
    item.selectedFunctionAfterID = item.selectedFunctionID;
    item.selectedSourceKey = selectedSourceKey(widget, &item.sourceOrdinal);
    if (item.selectedSourceKey.has_value())
        item.selectedControlSlotID = item.selectedSourceKey->controlSlotID;
}

void VCBridgeV5::recomputeGeometryDiff(AuthoringPreparationItem &item,
                                       AuthoringPreparationResult::DiffDetail diffDetail) const
{
    item.changes.clear();
    item.outcome = (item.beforeLogicalRect == item.afterLogicalRect)
        ? AuthoringPreparationItem::Outcome::Unchanged
        : AuthoringPreparationItem::Outcome::Prepared;
    item.changedCount = (item.outcome == AuthoringPreparationItem::Outcome::Prepared) ? 1 : 0;
    if (diffDetail == AuthoringPreparationResult::DiffDetail::Full &&
        item.outcome == AuthoringPreparationItem::Outcome::Prepared)
    {
        AuthoringFieldChange geometryChange;
        geometryChange.field = QStringLiteral("logicalGeometry");
        geometryChange.before = item.beforeLogicalRect;
        geometryChange.after = item.afterLogicalRect;
        geometryChange.changed = true;
        item.changes.append(geometryChange);
    }
}

void VCBridgeV5::recomputeGeometryDiff(AuthoringApplyItem &item,
                                       AuthoringPreparationResult::DiffDetail diffDetail) const
{
    item.changes.clear();
    item.changedCount = (item.outcome == AuthoringApplyItem::Outcome::Applied) ? 1 : 0;
    if (diffDetail == AuthoringPreparationResult::DiffDetail::Full &&
        item.outcome == AuthoringApplyItem::Outcome::Applied)
    {
        AuthoringFieldChange geometryChange;
        geometryChange.field = QStringLiteral("logicalGeometry");
        geometryChange.before = item.beforeLogicalRect;
        geometryChange.after = item.afterLogicalRect;
        geometryChange.changed = true;
        item.changes.append(geometryChange);
    }
}

void VCBridgeV5::clearAuthoringRecords()
{
    m_authoringRecords.clear();
}

void VCBridgeV5::cleanupExpiredRecords(qint64 nowMs)
{
    QList<QString> expired;
    expired.reserve(m_authoringRecords.size());
    for (auto it = m_authoringRecords.constBegin(); it != m_authoringRecords.constEnd(); ++it)
    {
        if (it->expiresAtMs <= nowMs)
            expired.append(it.key());
    }
    for (const QString &token : expired)
        m_authoringRecords.remove(token);
}

qint64 VCBridgeV5::authoringNowMs() const
{
    if (m_authoringNowOverrideMs.has_value())
        return *m_authoringNowOverrideMs;
    return m_authoringMonotonicClock.elapsed();
}

QMap<quint32, QRectF> VCBridgeV5::planFromNativePreview(const QVariantMap &preview) const
{
    QMap<quint32, QRectF> plan;
    const QVariantList geometries = preview.value(QStringLiteral("geometries")).toList();
    for (const QVariant &entry : geometries)
    {
        const QVariantMap map = entry.toMap();
        bool ok = false;
        const quint32 id = map.value(QStringLiteral("id")).toUInt(&ok);
        if (!ok)
            continue;
        plan.insert(id, map.value(QStringLiteral("rect")).toRectF());
    }
    return plan;
}

QVector<VCWidget *> VCBridgeV5::affectedClosure(const QMap<quint32, QRectF> &rectangles) const
{
    QVector<VCWidget *> closure;
    QSet<quint32> ids;
    if (!m_vc)
        return closure;

    auto add = [&](VCWidget *widget) {
        if (!widget || ids.contains(widget->id()))
            return;
        ids.insert(widget->id());
        closure.append(widget);
    };

    for (auto it = rectangles.constBegin(); it != rectangles.constEnd(); ++it)
    {
        VCWidget *widget = m_vc->widget(it.key());
        if (!widget)
            continue;
        add(widget);
        for (QObject *parent = widget->parent(); parent; parent = parent->parent())
            add(qobject_cast<VCWidget *>(parent));
        if (VCFrame *frame = qobject_cast<VCFrame *>(widget))
            for (VCWidget *child : frame->children(true))
                add(child);
    }

    return closure;
}

QVector<VCWidget *> VCBridgeV5::affectedClosureForWidgetIds(const QList<quint32> &widgetIds) const
{
    QMap<quint32, QRectF> byId;
    for (quint32 id : widgetIds)
        byId.insert(id, QRectF());
    return affectedClosure(byId);
}

VCBridgeV5::PredicateSelection VCBridgeV5::predicateSelectionForRequest(const AuthoringRequest &request) const
{
    PredicateSelection selection;
    if (const AuthoringLayoutRequest *layout = std::get_if<AuthoringLayoutRequest>(&request))
    {
        switch (layout->operation)
        {
        case AuthoringLayoutRequest::Operation::Geometry:
            selection.requireFeedbackPatchIdle = true;
            selection.requireNoRelativeSources = true;
            break;
        case AuthoringLayoutRequest::Operation::Reorder:
        case AuthoringLayoutRequest::Operation::MoveGroup:
            selection.requireFeedbackPatchIdle = true;
            selection.requireNoRelativeSources = true;
            if (m_vc && layout->destinationFrameWidgetID.has_value())
            {
                bool requiresWriterSafe = false;
                const quint32 destinationFrameId = *layout->destinationFrameWidgetID;
                for (quint32 id : layout->widgetIDs)
                {
                    VCWidget *widget = m_vc->widget(id);
                    if (!widget)
                        continue;
                    VCWidget *parentWidget = qobject_cast<VCWidget *>(widget->parent());
                    if (!parentWidget || parentWidget->id() != destinationFrameId)
                    {
                        requiresWriterSafe = true;
                        break;
                    }
                    if (layout->destinationSubpage.has_value() && widget->page() != *layout->destinationSubpage)
                    {
                        requiresWriterSafe = true;
                        break;
                    }
                }
                selection.requireWriterSafe = requiresWriterSafe;
            }
            break;
        case AuthoringLayoutRequest::Operation::Align:
            selection.requireFeedbackPatchIdle = true;
            selection.requireNoRelativeSources = true;
            break;
        case AuthoringLayoutRequest::Operation::PageDelete:
        case AuthoringLayoutRequest::Operation::FramePages:
            selection.requireFeedbackPatchIdle = true;
            break;
        default:
            break;
        }
        return selection;
    }

    if (std::get_if<AuthoringBindingsRequest>(&request))
        return selection;

    if (std::get_if<AuthoringRecordPanelRequest>(&request))
        return selection;

    return selection;
}

QString VCBridgeV5::preconditionErrorForClosure(
    const QVector<VCWidget *> &closure, QString *code,
    QList<AuthoringPreconditionObservation> *observations,
    const PredicateSelection &selection) const
{
    auto addObs = [&](const QString &name, bool met, const QString &scope, const QString &reason) {
        if (!observations)
            return;
        AuthoringPreconditionObservation obs;
        obs.name = name;
        obs.met = met;
        obs.scope = scope;
        obs.reason = reason;
        observations->append(obs);
    };

    for (VCWidget *widget : closure)
    {
        for (QObject *owner = widget; owner; owner = owner->parent())
        {
            VCPage *page = qobject_cast<VCPage *>(owner);
            if (!page)
                continue;
            if (page->PIN() != 0)
            {
                if (code)
                    *code = QStringLiteral("authoringPrecondition");
                addObs(QStringLiteral("protectedPages"), false,
                       QStringLiteral("page:%1").arg(page->id()),
                       QStringLiteral("Protected page editing is not supported for authoring"));
                return QStringLiteral("Protected page editing is not supported for authoring");
            }
        }
    }
    addObs(QStringLiteral("protectedPages"), true, QStringLiteral("closure"), QString());

    InputOutputMap *ioMap = m_doc ? m_doc->inputOutputMap() : nullptr;
    for (VCWidget *widget : closure)
    {
        const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
        for (const QSharedPointer<QLCInputSource> &source : sources)
        {
            if (source.isNull())
                continue;
            if (selection.requireNoRelativeSources && source->workingMode() == QLCInputSource::Relative)
            {
                if (code)
                    *code = QStringLiteral("unsupportedPreparation");
                addObs(QStringLiteral("relativeSources"), false,
                       QStringLiteral("widget:%1 source:%2/%3")
                           .arg(widget->id()).arg(source->universe()).arg(source->channel()),
                       QStringLiteral("Relative input sources are not supported for geometry authoring"));
                return QStringLiteral("Relative input sources are not supported for geometry authoring");
            }
            if (ioMap && selection.requireFeedbackPatchIdle)
            {
                OutputPatch *patch = ioMap->feedbackPatch(source->universe());
                if (patch && patch->isPatched())
                {
                    if (code)
                        *code = QStringLiteral("authoringPrecondition");
                    addObs(QStringLiteral("feedbackPatchIdle"), false,
                           QStringLiteral("widget:%1 universe:%2").arg(widget->id()).arg(source->universe()),
                           QStringLiteral("Feedback patch must be disabled for affected input sources"));
                    return QStringLiteral("Feedback patch must be disabled for affected input sources");
                }
            }
            if (selection.requireFeedbackNeedsUpdateIdle && source->needsUpdate())
            {
                if (code)
                    *code = QStringLiteral("unsupportedPreparation");
                addObs(QStringLiteral("feedbackNeedsUpdateIdle"), false,
                       QStringLiteral("widget:%1 source:%2/%3")
                           .arg(widget->id()).arg(source->universe()).arg(source->channel()),
                       QStringLiteral("Feedback source update is pending"));
                return QStringLiteral("Feedback source update is pending");
            }
        }
    }
    if (selection.requireNoRelativeSources)
        addObs(QStringLiteral("relativeSources"), true, QStringLiteral("closure"), QString());
    if (selection.requireFeedbackPatchIdle)
        addObs(QStringLiteral("feedbackPatchIdle"), true, QStringLiteral("closure"), QString());
    if (selection.requireFeedbackNeedsUpdateIdle)
        addObs(QStringLiteral("feedbackNeedsUpdateIdle"), true, QStringLiteral("closure"), QString());

    if (selection.requireWriterSafe)
    {
        for (VCWidget *widget : closure)
        {
            if (VCButton *button = qobject_cast<VCButton *>(widget))
            {
                if (button->state() == VCButton::Active)
                {
                    if (code)
                        *code = QStringLiteral("unsupportedPreparation");
                    addObs(QStringLiteral("writerSafe"), false,
                           QStringLiteral("widget:%1").arg(widget->id()),
                           QStringLiteral("Active button writer effects are not certifiable"));
                    return QStringLiteral("Active button writer effects are not certifiable");
                }
            }
            if (VCSlider *slider = qobject_cast<VCSlider *>(widget))
            {
                if (slider->sliderMode() == VCSlider::Level || slider->sliderMode() == VCSlider::Adjust)
                {
                    if (code)
                        *code = QStringLiteral("unsupportedPreparation");
                    addObs(QStringLiteral("writerSafe"), false,
                           QStringLiteral("widget:%1").arg(widget->id()),
                           QStringLiteral("Slider writer effects are not certifiable"));
                    return QStringLiteral("Slider writer effects are not certifiable");
                }
            }
            if (qobject_cast<VCAnimation *>(widget) || qobject_cast<VCAudioTriggers *>(widget) || qobject_cast<VCXYPad *>(widget))
            {
                if (code)
                    *code = QStringLiteral("unsupportedPreparation");
                addObs(QStringLiteral("writerSafe"), false,
                       QStringLiteral("widget:%1").arg(widget->id()),
                       QStringLiteral("Runtime writer effects are not certifiable"));
                return QStringLiteral("Runtime writer effects are not certifiable");
            }
        }
        addObs(QStringLiteral("writerSafe"), true, QStringLiteral("closure"), QString());
    }

    for (VCWidget *widget : closure)
    {
        if (VCRecordPanel *panel = qobject_cast<VCRecordPanel *>(widget))
        {
            if (panel->isRecordingChaser())
            {
                if (code)
                    *code = QStringLiteral("authoringPrecondition");
                addObs(QStringLiteral("recordPanelCapture"), false,
                       QStringLiteral("widget:%1").arg(widget->id()),
                       QStringLiteral("Record panel capture is active"));
                return QStringLiteral("Record panel capture is active");
            }
        }
    }
    addObs(QStringLiteral("recordPanelCapture"), true, QStringLiteral("closure"), QString());
    return {};
}

QString VCBridgeV5::authoringPreconditionError(const QMap<quint32, QRectF> &rectangles, QString *code,
                                               QList<AuthoringPreconditionObservation> *observations,
                                               const PredicateSelection &selection) const
{
    if (!hasMatchingAuthoringContext())
    {
        if (code)
            *code = QStringLiteral("unsupportedBridge");
        if (observations)
            observations->append({QStringLiteral("context"), false, QStringLiteral("global"),
                                  QStringLiteral("Authoring app/doc/vc context mismatch")});
        return QStringLiteral("Authoring operations are not supported by this bridge");
    }
    if (observations)
        observations->append({QStringLiteral("context"), true, QStringLiteral("global"), QString()});
    if (!(m_authoringApp->accessMask() & App::AC_VCEditing))
    {
        if (code)
            *code = QStringLiteral("authoringPrecondition");
        if (observations)
            observations->append({QStringLiteral("access"), false, QStringLiteral("global"),
                                  QStringLiteral("Console editing unavailable in current access profile")});
        return QStringLiteral("Console editing is unavailable in this access profile");
    }
    if (observations)
        observations->append({QStringLiteral("access"), true, QStringLiteral("global"), QString()});
    if (m_doc->mode() != Doc::Design)
    {
        if (code)
            *code = QStringLiteral("authoringPrecondition");
        if (observations)
            observations->append({QStringLiteral("designMode"), false, QStringLiteral("global"),
                                  QStringLiteral("Console authoring requires Design mode")});
        return QStringLiteral("Console authoring requires Design mode");
    }
    if (observations)
        observations->append({QStringLiteral("designMode"), true, QStringLiteral("global"), QString()});
    if (!m_vc->editMode())
    {
        if (code)
            *code = QStringLiteral("authoringPrecondition");
        if (observations)
            observations->append({QStringLiteral("editMode"), false, QStringLiteral("global"),
                                  QStringLiteral("Console authoring requires edit mode")});
        return QStringLiteral("Console authoring requires edit mode");
    }
    if (observations)
        observations->append({QStringLiteral("editMode"), true, QStringLiteral("global"), QString()});

    ShowCommandRecorder *recorder = ShowCommandRecorder::instance();
    if (recorder == nullptr)
    {
        if (code)
            *code = QStringLiteral("authoringPrecondition");
        if (observations)
            observations->append({QStringLiteral("recorderAvailable"), false, QStringLiteral("global"),
                                  QStringLiteral("ShowCommandRecorder is unavailable")});
        return QStringLiteral("Show command recorder is unavailable");
    }
    if (observations)
        observations->append({QStringLiteral("recorderAvailable"), true, QStringLiteral("global"), QString()});
    if (recorder->isRecording())
    {
        if (code)
            *code = QStringLiteral("authoringPrecondition");
        if (observations)
            observations->append({QStringLiteral("recordingIdle"), false, QStringLiteral("global"),
                                  QStringLiteral("Recorder is active")});
        return QStringLiteral("Recorder is active");
    }
    if (observations)
        observations->append({QStringLiteral("recordingIdle"), true, QStringLiteral("global"), QString()});
    if (recorder->pendingControlRuns() > 0)
    {
        if (code)
            *code = QStringLiteral("authoringPrecondition");
        if (observations)
            observations->append({QStringLiteral("pendingControlRuns"), false, QStringLiteral("global"),
                                  QStringLiteral("Recorder has pending control runs")});
        return QStringLiteral("Recorder has pending control runs");
    }
    if (observations)
        observations->append({QStringLiteral("pendingControlRuns"), true, QStringLiteral("global"), QString()});
    if (!recorder->pendingUserRequests().isEmpty())
    {
        if (code)
            *code = QStringLiteral("authoringPrecondition");
        if (observations)
            observations->append({QStringLiteral("pendingUserRequests"), false, QStringLiteral("global"),
                                  QStringLiteral("Recorder has pending user requests")});
        return QStringLiteral("Recorder has pending user requests");
    }
    if (observations)
        observations->append({QStringLiteral("pendingUserRequests"), true, QStringLiteral("global"), QString()});

    if (MasterTimer *timer = m_doc->masterTimer())
    {
        if (!timer->runningFunctionIds().isEmpty())
        {
            if (code)
                *code = QStringLiteral("authoringPrecondition");
            if (observations)
                observations->append({QStringLiteral("runningFunctions"), false, QStringLiteral("global"),
                                      QStringLiteral("Function playback is running")});
            return QStringLiteral("Function playback is running");
        }
        if (observations)
            observations->append({QStringLiteral("runningFunctions"), true, QStringLiteral("global"), QString()});
        const QList<Function *> functions = m_doc->functions();
        for (Function *function : functions)
        {
            if (function && timer->isStartQueued(function))
            {
                if (code)
                    *code = QStringLiteral("authoringPrecondition");
                if (observations)
                    observations->append({QStringLiteral("queuedStarts"), false, QStringLiteral("global"),
                                          QStringLiteral("Function playback has queued starts")});
                return QStringLiteral("Function playback has queued starts");
            }
        }
        if (observations)
            observations->append({QStringLiteral("queuedStarts"), true, QStringLiteral("global"), QString()});
    }
    else
    {
        if (code)
            *code = QStringLiteral("authoringPrecondition");
        if (observations)
            observations->append({QStringLiteral("masterTimerAvailable"), false, QStringLiteral("global"),
                                  QStringLiteral("Master timer is unavailable")});
        return QStringLiteral("Master timer is unavailable");
    }

    const QVector<VCWidget *> closure = affectedClosure(rectangles);
    return preconditionErrorForClosure(closure, code, observations, selection);
}

void VCBridgeV5::setAuthoringNowForTest(qint64 nowMs)
{
    m_authoringNowOverrideMs = nowMs;
}

void VCBridgeV5::clearAuthoringNowForTest()
{
    m_authoringNowOverrideMs.reset();
}

VCBridge::AuthoringReadResult VCBridgeV5::queryAuthoring(const AuthoringQuery &query) const
{
    if (!hasMatchingAuthoringContext())
        return unsupportedAuthoringRead();

    const_cast<VCBridgeV5 *>(this)->cleanupExpiredRecords(authoringNowMs());

    struct CursorState
    {
        int version = 1;
        QString scope;
        QString basis;
        QString selectorHash;
        QString instance;
        int offset = 0;
        qint64 issuedAtMs = 0;
    };

    auto globMatch = [](const QString &pattern, const QString &text) -> bool {
        QString regex = QRegularExpression::wildcardToRegularExpression(
            pattern, QRegularExpression::UnanchoredWildcardConversion);
        if (!regex.startsWith("\\A") && !regex.startsWith("^"))
            regex = "\\A(?:" + regex + ")\\z";
        QRegularExpression re(regex, QRegularExpression::CaseInsensitiveOption);
        return re.match(text).hasMatch();
    };

    auto encodeCursor = [](const CursorState &cursor) -> QString {
        QJsonObject object;
        object.insert(QStringLiteral("v"), cursor.version);
        object.insert(QStringLiteral("scope"), cursor.scope);
        object.insert(QStringLiteral("basis"), cursor.basis);
        object.insert(QStringLiteral("selector"), cursor.selectorHash);
        object.insert(QStringLiteral("instance"), cursor.instance);
        object.insert(QStringLiteral("offset"), cursor.offset);
        object.insert(QStringLiteral("issuedAtMs"), double(cursor.issuedAtMs));
        return QString::fromLatin1(QJsonDocument(object).toJson(QJsonDocument::Compact)
                                       .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    };

    auto decodeCursor = [](const QString &text, CursorState *cursor, QString *error) -> bool {
        const QByteArray decoded = QByteArray::fromBase64(
            text.toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        const QJsonDocument doc = QJsonDocument::fromJson(decoded);
        if (!doc.isObject())
        {
            if (error) *error = QStringLiteral("cursor is malformed");
            return false;
        }
        const QJsonObject object = doc.object();
        if (!object.value(QStringLiteral("v")).isDouble() ||
            !object.value(QStringLiteral("scope")).isString() ||
            !object.value(QStringLiteral("basis")).isString() ||
            !object.value(QStringLiteral("selector")).isString() ||
            !object.value(QStringLiteral("instance")).isString() ||
            !object.value(QStringLiteral("offset")).isDouble() ||
            !object.value(QStringLiteral("issuedAtMs")).isDouble())
        {
            if (error) *error = QStringLiteral("cursor is malformed");
            return false;
        }
        cursor->version = object.value(QStringLiteral("v")).toInt();
        cursor->scope = object.value(QStringLiteral("scope")).toString();
        cursor->basis = object.value(QStringLiteral("basis")).toString();
        cursor->selectorHash = object.value(QStringLiteral("selector")).toString();
        cursor->instance = object.value(QStringLiteral("instance")).toString();
        cursor->offset = object.value(QStringLiteral("offset")).toInt(-1);
        cursor->issuedAtMs = qint64(object.value(QStringLiteral("issuedAtMs")).toDouble(-1));
        if (cursor->offset < 0 || cursor->issuedAtMs < 0)
        {
            if (error) *error = QStringLiteral("cursor is malformed");
            return false;
        }
        return true;
    };

    auto unsupportedQuery = [&](const QString &basisScope, const QString &basis,
                                const QString &scopeMessage) -> AuthoringReadResult {
        AuthoringReadResult unsupported;
        unsupported.basisScope = basisScope;
        unsupported.basis = basis;
        unsupported.complete = false;
        unsupported.code = QStringLiteral("unsupportedPreparation");
        unsupported.error = QStringLiteral("Authoring query family is not implemented at this checkpoint");
        unsupported.limitations.append({QStringLiteral("unsupportedPreparation"),
                                        QStringLiteral("query"),
                                        scopeMessage});
        return unsupported;
    };

    auto stablePayloadBytes = [&](AuthoringReadResult &readResult) -> int {
        int bytes = qMax(0, readResult.work.payloadBytes);
        for (int i = 0; i < 8; ++i)
        {
            readResult.work.payloadBytes = bytes;
            const int measured = int(VCAuthoring::toJson(readResult).dump().size());
            if (measured == bytes)
                return measured;
            bytes = measured;
        }
        readResult.work.payloadBytes = bytes;
        return bytes;
    };

    const qint64 nowMs = authoringNowMs();
    const QString instanceId = QStringLiteral("%1:%2")
        .arg(quintptr(m_doc.data()), 0, 16)
        .arg(quintptr(m_vc.data()), 0, 16);

    if (const AuthoringSetupQuery *setupQuery = std::get_if<AuthoringSetupQuery>(&query))
    {
        AuthoringReadResult result;
        result.basisScope = QStringLiteral("vcstruct-v1");
        result.basis = authoringBasis();

        if (setupQuery->limit < 1 || setupQuery->limit > 250)
        {
            result.code = QStringLiteral("invalidArgument");
            result.error = QStringLiteral("limit must be in range 1..250");
            return result;
        }
        if (setupQuery->cursor.toUtf8().size() > 1024)
        {
            result.code = QStringLiteral("invalidArgument");
            result.error = QStringLiteral("cursor must be at most 1024 UTF-8 bytes");
            return result;
        }

        QByteArray selectorCanonical("setup;");
        selectorCanonical += QByteArray::number(int(setupQuery->kind));
        selectorCanonical += ';';
        selectorCanonical += QByteArray::number(setupQuery->limit);
        selectorCanonical += ';';
        selectorCanonical += QByteArray::number(setupQuery->pageWidgetID);
        selectorCanonical += ';';
        selectorCanonical += QByteArray::number(setupQuery->frameWidgetID);
        selectorCanonical += ';';
        selectorCanonical += QByteArray::number(setupQuery->childSubpage);
        selectorCanonical += ';';
        selectorCanonical += setupQuery->recursive ? "1;" : "0;";
        selectorCanonical += setupQuery->nameFilter.toUtf8();
        selectorCanonical += ';';
        selectorCanonical += setupQuery->typeFilter.toUtf8();
        selectorCanonical += ';';
        for (quint32 id : setupQuery->widgetIDs)
        {
            selectorCanonical += QByteArray::number(id);
            selectorCanonical += ',';
        }
        selectorCanonical += ';';
        for (const QString &field : setupQuery->fields)
        {
            selectorCanonical += field.toUtf8();
            selectorCanonical += ',';
        }
        const QString selectorHash = QString::fromLatin1(
            QCryptographicHash::hash(selectorCanonical, QCryptographicHash::Sha256).toHex());

        int offset = 0;
        if (!setupQuery->cursor.isEmpty())
        {
            CursorState cursor;
            QString cursorError;
            if (!decodeCursor(setupQuery->cursor, &cursor, &cursorError))
            {
                result.code = QStringLiteral("invalidCursor");
                result.error = cursorError;
                return result;
            }
            if (cursor.version != 1 || cursor.scope != QStringLiteral("setup") ||
                cursor.instance != instanceId || cursor.selectorHash != selectorHash)
            {
                result.code = QStringLiteral("invalidCursor");
                result.error = QStringLiteral("cursor does not match this query scope");
                return result;
            }
            if (cursor.basis != result.basis)
            {
                result.code = QStringLiteral("staleBasis");
                result.error = QStringLiteral("Authoring basis mismatch");
                return result;
            }
            if (cursor.issuedAtMs + kAuthoringRecordTtlMs <= nowMs)
            {
                result.code = QStringLiteral("cursorExpired");
                result.error = QStringLiteral("cursor expired");
                return result;
            }
            offset = cursor.offset;
        }

        if (setupQuery->kind == AuthoringSetupQuery::Kind::Pages)
        {
            QList<AuthoringReadRow> allRows;
            for (int i = 0; i < m_vc->pagesCount(); ++i)
            {
                VCPage *page = m_vc->page(i);
                if (!page)
                    continue;
                if (setupQuery->pageWidgetID != quint32(-1) && page->id() != setupQuery->pageWidgetID)
                    continue;
                AuthoringReadRow row;
                AuthoringReadRow::SetupPageRow pageRow;
                pageRow.pageWidgetID = page->id();
                pageRow.pageIndex = i;
                pageRow.caption = page->caption().isEmpty()
                    ? QStringLiteral("Page %1").arg(i + 1)
                    : page->caption();
                pageRow.externalInputMode = VCPage::externalInputModeToString(page->externalInputMode());
                pageRow.configurationDigest = scopedConfigurationDigest(page);
                row.row = pageRow;
                allRows.append(row);
            }
            result.work.scannedSummaryRows = allRows.size();
            if (offset > allRows.size())
            {
                result.code = QStringLiteral("invalidCursor");
                result.error = QStringLiteral("cursor offset is out of range");
                return result;
            }
            const int total = allRows.size();
            int nextOffset = offset;
            result.rows.reserve(setupQuery->limit);
            while (nextOffset < total && int(result.rows.size()) < setupQuery->limit)
            {
                const AuthoringReadRow &candidateRow = allRows.at(nextOffset);
                AuthoringReadResult probe;
                probe.version = result.version;
                probe.basis = result.basis;
                probe.basisScope = result.basisScope;
                probe.complete = (nextOffset + 1) >= total;
                if (!probe.complete)
                {
                    CursorState next;
                    next.scope = QStringLiteral("setup");
                    next.basis = result.basis;
                    next.selectorHash = selectorHash;
                    next.instance = instanceId;
                    next.offset = nextOffset + 1;
                    next.issuedAtMs = nowMs;
                    probe.nextCursor = encodeCursor(next);
                }
                probe.rows = result.rows;
                probe.rows.append(candidateRow);
                probe.work = result.work;
                probe.work.serializedRows = probe.rows.size();
                if (stablePayloadBytes(probe) > VCAuthoring::kMaxToolPayloadBytes)
                {
                    result.code = QStringLiteral("itemTooLarge");
                    result.error = QStringLiteral("A single result row exceeds the payload limit");
                    if (!result.rows.isEmpty())
                    {
                        result.code.clear();
                        result.error.clear();
                        result.complete = false;
                        CursorState next;
                        next.scope = QStringLiteral("setup");
                        next.basis = result.basis;
                        next.selectorHash = selectorHash;
                        next.instance = instanceId;
                        next.offset = nextOffset;
                        next.issuedAtMs = nowMs;
                        result.nextCursor = encodeCursor(next);
                        result.work.serializedRows = result.rows.size();
                        stablePayloadBytes(result);
                    }
                    else
                    {
                        result.rows.clear();
                        result.complete = false;
                        result.nextCursor.clear();
                        result.work.serializedRows = 0;
                        result.work.hydratedRows = 0;
                        stablePayloadBytes(result);
                    }
                    return result;
                }
                result.rows.append(candidateRow);
                ++nextOffset;
            }
            result.complete = nextOffset >= total;
            if (!result.complete)
            {
                CursorState next;
                next.scope = QStringLiteral("setup");
                next.basis = result.basis;
                next.selectorHash = selectorHash;
                next.instance = instanceId;
                next.offset = nextOffset;
                next.issuedAtMs = nowMs;
                result.nextCursor = encodeCursor(next);
            }
            result.work.serializedRows = result.rows.size();
            result.work.hydratedRows = 0;
            stablePayloadBytes(result);
            return result;
        }

        if (setupQuery->kind == AuthoringSetupQuery::Kind::Subpages)
        {
            if (setupQuery->frameWidgetID == quint32(-1))
            {
                result.code = QStringLiteral("invalidArgument");
                result.error = QStringLiteral("subpages query requires frameWidgetID");
                return result;
            }
            VCFrame *frame = qobject_cast<VCFrame *>(m_vc->widget(setupQuery->frameWidgetID));
            if (!frame)
            {
                result.code = QStringLiteral("invalidArgument");
                result.error = QStringLiteral("frameWidgetID is unknown");
                return result;
            }

            const int totalPages = qMax(1, frame->totalPagesNumber());
            const QStringList labels = frame->pageLabels();
            QList<AuthoringReadRow> allRows;
            for (int subpage = 0; subpage < totalPages; ++subpage)
            {
                if (setupQuery->childSubpage >= 0 && subpage != setupQuery->childSubpage)
                    continue;
                AuthoringReadRow row;
                AuthoringReadRow::SetupSubpageRow subpageRow;
                subpageRow.frameWidgetID = frame->id();
                subpageRow.childSubpage = subpage;
                if (subpage < labels.size() && !labels.at(subpage).isEmpty())
                    subpageRow.label = labels.at(subpage);
                else
                    subpageRow.label = QStringLiteral("Page %1").arg(subpage + 1);
                subpageRow.configurationDigest = scopedConfigurationDigest(frame);
                row.row = subpageRow;
                allRows.append(row);
            }
            result.work.scannedSummaryRows = allRows.size();
            if (offset > allRows.size())
            {
                result.code = QStringLiteral("invalidCursor");
                result.error = QStringLiteral("cursor offset is out of range");
                return result;
            }
            const int total = allRows.size();
            int nextOffset = offset;
            result.rows.reserve(setupQuery->limit);
            while (nextOffset < total && int(result.rows.size()) < setupQuery->limit)
            {
                const AuthoringReadRow &candidateRow = allRows.at(nextOffset);
                AuthoringReadResult probe;
                probe.version = result.version;
                probe.basis = result.basis;
                probe.basisScope = result.basisScope;
                probe.complete = (nextOffset + 1) >= total;
                if (!probe.complete)
                {
                    CursorState next;
                    next.scope = QStringLiteral("setup");
                    next.basis = result.basis;
                    next.selectorHash = selectorHash;
                    next.instance = instanceId;
                    next.offset = nextOffset + 1;
                    next.issuedAtMs = nowMs;
                    probe.nextCursor = encodeCursor(next);
                }
                probe.rows = result.rows;
                probe.rows.append(candidateRow);
                probe.work = result.work;
                probe.work.serializedRows = probe.rows.size();
                if (stablePayloadBytes(probe) > VCAuthoring::kMaxToolPayloadBytes)
                {
                    result.code = QStringLiteral("itemTooLarge");
                    result.error = QStringLiteral("A single result row exceeds the payload limit");
                    if (!result.rows.isEmpty())
                    {
                        result.code.clear();
                        result.error.clear();
                        result.complete = false;
                        CursorState next;
                        next.scope = QStringLiteral("setup");
                        next.basis = result.basis;
                        next.selectorHash = selectorHash;
                        next.instance = instanceId;
                        next.offset = nextOffset;
                        next.issuedAtMs = nowMs;
                        result.nextCursor = encodeCursor(next);
                        result.work.serializedRows = result.rows.size();
                        stablePayloadBytes(result);
                    }
                    else
                    {
                        result.rows.clear();
                        result.complete = false;
                        result.nextCursor.clear();
                        result.work.serializedRows = 0;
                        result.work.hydratedRows = 0;
                        stablePayloadBytes(result);
                    }
                    return result;
                }
                result.rows.append(candidateRow);
                ++nextOffset;
            }
            result.complete = nextOffset >= total;
            if (!result.complete)
            {
                CursorState next;
                next.scope = QStringLiteral("setup");
                next.basis = result.basis;
                next.selectorHash = selectorHash;
                next.instance = instanceId;
                next.offset = nextOffset;
                next.issuedAtMs = nowMs;
                result.nextCursor = encodeCursor(next);
            }
            result.work.serializedRows = result.rows.size();
            result.work.hydratedRows = 0;
            stablePayloadBytes(result);
            return result;
        }

        struct SetupCandidate
        {
            VCWidget *widget = nullptr;
            quint32 pageWidgetID = quint32(-1);
            qint32 parentWidgetID = -1;
            int childSubpage = -1;
            int siblingIndex = -1;
        };

        QList<SetupCandidate> allCandidates;
        QSet<quint32> seenWidgets;

        std::function<void(VCFrame *, quint32, int, bool)> appendChildren =
            [&](VCFrame *container, quint32 owningPageID, int subpage, bool recursive)
        {
            if (!container)
                return;
            const QVariantList orderedIds = container->orderedChildIDs(subpage);
            int sibling = 0;
            for (const QVariant &value : orderedIds)
            {
                bool ok = false;
                const quint32 childId = value.toUInt(&ok);
                if (!ok)
                    continue;
                VCWidget *child = m_vc->widget(childId);
                if (!child)
                    continue;
                if (!seenWidgets.contains(child->id()))
                {
                    SetupCandidate candidate;
                    candidate.widget = child;
                    candidate.pageWidgetID = owningPageID;
                    VCWidget *parentWidget = qobject_cast<VCWidget *>(child->parent());
                    candidate.parentWidgetID = parentWidget ? qint32(parentWidget->id()) : -1;
                    candidate.childSubpage = child->page();
                    candidate.siblingIndex = sibling;
                    allCandidates.append(candidate);
                    seenWidgets.insert(child->id());
                }
                ++sibling;
                if (!recursive)
                    continue;
                VCFrame *childFrame = qobject_cast<VCFrame *>(child);
                if (!childFrame)
                    continue;
                if (childFrame->multiPageMode())
                {
                    const int total = qMax(1, childFrame->totalPagesNumber());
                    for (int nestedSubpage = 0; nestedSubpage < total; ++nestedSubpage)
                        appendChildren(childFrame, owningPageID, nestedSubpage, true);
                }
                else
                {
                    appendChildren(childFrame, owningPageID, childFrame->currentPage(), true);
                }
            }
        };

        auto siblingIndexFor = [&](VCWidget *widget) -> int {
            if (!widget)
                return -1;
            VCFrame *parentFrame = qobject_cast<VCFrame *>(widget->parent());
            if (!parentFrame)
                return -1;
            const QVariantList orderedIds = parentFrame->orderedChildIDs(widget->page());
            for (int i = 0; i < orderedIds.size(); ++i)
            {
                bool ok = false;
                const quint32 id = orderedIds.at(i).toUInt(&ok);
                if (ok && id == widget->id())
                    return i;
            }
            return -1;
        };

        auto addSingleWidget = [&](VCWidget *widget) {
            if (!widget || seenWidgets.contains(widget->id()))
                return;
            SetupCandidate candidate;
            candidate.widget = widget;
            candidate.pageWidgetID = owningPageWidgetId(widget).value_or(quint32(-1));
            VCWidget *parentWidget = qobject_cast<VCWidget *>(widget->parent());
            candidate.parentWidgetID = parentWidget ? qint32(parentWidget->id()) : -1;
            candidate.childSubpage = widget->page();
            candidate.siblingIndex = siblingIndexFor(widget);
            allCandidates.append(candidate);
            seenWidgets.insert(widget->id());
        };

        VCFrame *scopeFrame = nullptr;
        if (setupQuery->frameWidgetID != quint32(-1))
        {
            scopeFrame = qobject_cast<VCFrame *>(m_vc->widget(setupQuery->frameWidgetID));
            if (!scopeFrame)
            {
                result.code = QStringLiteral("invalidArgument");
                result.error = QStringLiteral("frameWidgetID is unknown");
                return result;
            }
        }

        auto inFrameScope = [&](VCWidget *widget) -> bool {
            if (!scopeFrame || !widget)
                return true;
            VCWidget *cursor = widget;
            while (cursor)
            {
                VCWidget *parentWidget = qobject_cast<VCWidget *>(cursor->parent());
                if (!parentWidget)
                    return false;
                if (parentWidget == scopeFrame)
                {
                    if (!setupQuery->recursive && cursor != widget)
                        return false;
                    if (setupQuery->childSubpage >= 0 && cursor->page() != setupQuery->childSubpage)
                        return false;
                    return true;
                }
                cursor = parentWidget;
            }
            return false;
        };

        if (!setupQuery->widgetIDs.isEmpty())
        {
            for (quint32 id : setupQuery->widgetIDs)
            {
                VCWidget *widget = m_vc->widget(id);
                if (!widget)
                {
                    result.code = QStringLiteral("invalidArgument");
                    result.error = QStringLiteral("widgetIDs references unknown widget ID %1").arg(id);
                    return result;
                }
                if (!inFrameScope(widget))
                {
                    result.code = QStringLiteral("invalidArgument");
                    result.error = QStringLiteral("widgetIDs contains out-of-scope widget ID %1").arg(id);
                    return result;
                }
                addSingleWidget(widget);
                if (setupQuery->recursive)
                {
                    VCFrame *frame = qobject_cast<VCFrame *>(widget);
                    if (!frame)
                        continue;
                    if (frame->multiPageMode())
                    {
                        const int total = qMax(1, frame->totalPagesNumber());
                        for (int subpage = 0; subpage < total; ++subpage)
                            appendChildren(frame, owningPageWidgetId(widget).value_or(quint32(-1)), subpage, true);
                    }
                    else
                    {
                        appendChildren(frame, owningPageWidgetId(widget).value_or(quint32(-1)),
                                       frame->currentPage(), true);
                    }
                }
            }
        }
        else if (setupQuery->frameWidgetID != quint32(-1))
        {
            VCFrame *frame = scopeFrame;
            const quint32 owningPage = owningPageWidgetId(frame).value_or(quint32(-1));
            if (setupQuery->childSubpage >= 0)
            {
                appendChildren(frame, owningPage, setupQuery->childSubpage, setupQuery->recursive);
            }
            else if (frame->multiPageMode())
            {
                const int total = qMax(1, frame->totalPagesNumber());
                for (int subpage = 0; subpage < total; ++subpage)
                    appendChildren(frame, owningPage, subpage, setupQuery->recursive);
            }
            else
            {
                appendChildren(frame, owningPage, frame->currentPage(), setupQuery->recursive);
            }
        }
        else if (setupQuery->pageWidgetID != quint32(-1))
        {
            VCPage *page = qobject_cast<VCPage *>(m_vc->widget(setupQuery->pageWidgetID));
            if (!page)
            {
                result.code = QStringLiteral("invalidArgument");
                result.error = QStringLiteral("pageWidgetID is unknown");
                return result;
            }
            appendChildren(page, page->id(), 0, setupQuery->recursive);
        }
        else
        {
            for (int i = 0; i < m_vc->pagesCount(); ++i)
            {
                VCPage *page = m_vc->page(i);
                if (!page)
                    continue;
                appendChildren(page, page->id(), 0, setupQuery->recursive);
            }
        }

        result.work.scannedSummaryRows = allCandidates.size();

        QSet<QString> requestedFields;
        for (const QString &field : setupQuery->fields)
            requestedFields.insert(field);
        const bool includeLogicalGeometry = requestedFields.contains(QStringLiteral("logicalGeometry"));
        const bool includeFunctionBinding = requestedFields.contains(QStringLiteral("functionBinding"));
        const bool includeInputBindings = requestedFields.contains(QStringLiteral("inputBindings"));
        const bool includeFeedback = requestedFields.contains(QStringLiteral("feedback"));
        const bool includeRecordPanel = requestedFields.contains(QStringLiteral("recordPanel"));
        const bool includeAppearance = requestedFields.contains(QStringLiteral("appearance"));
        const bool includeFrameConfig = requestedFields.contains(QStringLiteral("frameConfiguration"));
        const bool includeHydratedGroups =
            includeFunctionBinding || includeInputBindings || includeFeedback ||
            includeRecordPanel || includeAppearance || includeFrameConfig;

        QList<SetupCandidate> filteredCandidates;
        filteredCandidates.reserve(allCandidates.size());
        QSet<quint32> matchedRequestedIds;
        for (const SetupCandidate &candidate : allCandidates)
        {
            VCWidget *widget = candidate.widget;
            if (!widget)
                continue;

            if (setupQuery->pageWidgetID != quint32(-1) && candidate.pageWidgetID != setupQuery->pageWidgetID)
                continue;
            if (!setupQuery->typeFilter.isEmpty() &&
                QString::fromStdString(VCType::toString(widget->type())).compare(
                    setupQuery->typeFilter, Qt::CaseInsensitive) != 0)
                continue;
            if (!setupQuery->nameFilter.isEmpty() && !globMatch(setupQuery->nameFilter, widget->caption()))
                continue;
            filteredCandidates.append(candidate);
            matchedRequestedIds.insert(widget->id());
        }

        if (!setupQuery->widgetIDs.isEmpty())
        {
            for (quint32 requestedId : setupQuery->widgetIDs)
            {
                if (matchedRequestedIds.contains(requestedId))
                    continue;
                result.code = QStringLiteral("invalidArgument");
                result.error = QStringLiteral("widgetIDs contains out-of-scope widget ID %1").arg(requestedId);
                return result;
            }
        }

        if (offset > filteredCandidates.size())
        {
            result.code = QStringLiteral("invalidCursor");
            result.error = QStringLiteral("cursor offset is out of range");
            return result;
        }

        auto hydrateCandidate = [&](const SetupCandidate &candidate) -> AuthoringReadRow {
            VCWidget *widget = candidate.widget;
            AuthoringReadRow row;
            AuthoringReadRow::SetupWidgetRow widgetRow;
            widgetRow.widgetID = widget->id();
            widgetRow.pageWidgetID = candidate.pageWidgetID;
            widgetRow.parentWidgetID = candidate.parentWidgetID;
            widgetRow.childSubpage = candidate.childSubpage;
            widgetRow.siblingIndex = candidate.siblingIndex;
            widgetRow.machineType = QString::fromStdString(VCType::toString(widget->type()));
            widgetRow.caption = widget->caption();
            if (includeLogicalGeometry)
                widgetRow.logicalGeometry = widget->logicalGeometry();

            if (includeFunctionBinding)
            {
                AuthoringReadRow::SetupWidgetRow::FunctionBindingGroup group;
                if (std::optional<quint32> functionID = selectedFunctionId(widget))
                    group.functionID = *functionID;
                if (VCButton *button = qobject_cast<VCButton *>(widget))
                    group.action = VCButton::actionToString(button->actionType()).toLower();
                widgetRow.functionBinding = group;
            }

            if (includeInputBindings || includeFeedback)
            {
                QMap<QString, int> pairCounts;
                const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
                for (const QSharedPointer<QLCInputSource> &source : sources)
                {
                    if (source.isNull())
                        continue;
                    const QString pair = QStringLiteral("%1:%2").arg(source->universe()).arg(source->channel());
                    pairCounts[pair] = pairCounts.value(pair) + 1;
                }

                if (includeInputBindings)
                {
                    AuthoringReadRow::SetupWidgetRow::InputBindingGroup group;
                    int ordinal = 0;
                    for (const QSharedPointer<QLCInputSource> &source : sources)
                    {
                        if (source.isNull())
                            continue;
                        AuthoringReadRow::SetupWidgetRow::InputBindingGroup::Entry entry;
                        if (source->id() <= std::numeric_limits<quint8>::max())
                            entry.sourceKey.controlSlotID = quint8(source->id());
                        entry.sourceKey.inputUniverseID = source->universe();
                        entry.sourceKey.inputChannel = source->channel();
                        entry.sourceKey.childSubpage = int(source->page());
                        entry.sourceOrdinal = ordinal++;
                        entry.sourceName = resolveSourceName(widget, source->id());
                        entry.configurationDigest = scopedConfigurationDigest(entry.sourceKey, source.data());
                        const QString pair = QStringLiteral("%1:%2")
                            .arg(source->universe()).arg(source->channel());
                        entry.duplicate = pairCounts.value(pair) > 1;
                        entry.ambiguous = entry.duplicate;
                        group.entries.append(entry);
                    }
                    widgetRow.inputBindings = group;
                }

                if (includeFeedback)
                {
                    AuthoringReadRow::SetupWidgetRow::FeedbackGroup group;
                    int ordinal = 0;
                    for (const QSharedPointer<QLCInputSource> &source : sources)
                    {
                        if (source.isNull())
                            continue;
                        AuthoringReadRow::SetupWidgetRow::FeedbackGroup::Entry entry;
                        if (source->id() <= std::numeric_limits<quint8>::max())
                            entry.sourceKey.controlSlotID = quint8(source->id());
                        entry.sourceKey.inputUniverseID = source->universe();
                        entry.sourceKey.inputChannel = source->channel();
                        entry.sourceKey.childSubpage = int(source->page());
                        entry.sourceOrdinal = ordinal++;
                        entry.idleValue = int(source->feedbackValue(QLCInputFeedback::LowerValue));
                        entry.activeValue = int(source->feedbackValue(QLCInputFeedback::UpperValue));
                        entry.monitorValue = int(source->feedbackValue(QLCInputFeedback::MonitorValue));
                        entry.idleChannel = source->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
                        entry.activeChannel = source->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
                        entry.monitorChannel = source->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
                        entry.configurationDigest = scopedConfigurationDigest(entry.sourceKey, source.data());
                        group.entries.append(entry);
                    }
                    widgetRow.feedback = group;
                }
            }

            if (includeRecordPanel)
            {
                if (VCRecordPanel *panel = qobject_cast<VCRecordPanel *>(widget))
                {
                    AuthoringReadRow::SetupWidgetRow::RecordPanelGroup group;
                    group.targetFolder = panel->targetFolder();
                    group.scenePrefix = panel->scenePrefix();
                    group.chaserPrefix = panel->chaserPrefix();
                    group.defaultFadeIn = panel->defaultFadeIn();
                    group.defaultHold = panel->defaultHold();
                    group.defaultFadeOut = panel->defaultFadeOut();
                    widgetRow.recordPanel = group;
                }
            }

            if (includeAppearance)
            {
                AuthoringReadRow::SetupWidgetRow::AppearanceGroup group;
                group.bgColor = widget->backgroundColor();
                group.fgColor = widget->foregroundColor();
                group.disabled = widget->isDisabled();
                widgetRow.appearance = group;
            }

            if (includeFrameConfig)
            {
                if (VCFrame *frame = qobject_cast<VCFrame *>(widget))
                {
                    AuthoringReadRow::SetupWidgetRow::FrameConfigurationGroup group;
                    group.totalPages = frame->totalPagesNumber();
                    group.currentPage = frame->currentPage();
                    group.pagesLoop = frame->pagesLoop();
                    group.pageLabels = frame->pageLabels();
                    widgetRow.frameConfiguration = group;
                }
            }

            AuthoringReadRow digestRow;
            digestRow.row = widgetRow;
            const QByteArray digestBytes = QByteArray::fromStdString(VCAuthoring::toJson(digestRow).dump());
            widgetRow.configurationDigest = QString::fromLatin1(
                QCryptographicHash::hash(digestBytes, QCryptographicHash::Sha256).toHex());
            row.row = widgetRow;
            return row;
        };

        const int total = filteredCandidates.size();
        int nextOffset = offset;
        result.rows.reserve(setupQuery->limit);
        while (nextOffset < total && int(result.rows.size()) < setupQuery->limit)
        {
            const SetupCandidate &candidate = filteredCandidates.at(nextOffset);
            if (includeHydratedGroups)
                ++result.work.hydratedRows;
            const AuthoringReadRow candidateRow = hydrateCandidate(candidate);
            AuthoringReadResult probe;
            probe.version = result.version;
            probe.basis = result.basis;
            probe.basisScope = result.basisScope;
            probe.complete = (nextOffset + 1) >= total;
            if (!probe.complete)
            {
                CursorState next;
                next.scope = QStringLiteral("setup");
                next.basis = result.basis;
                next.selectorHash = selectorHash;
                next.instance = instanceId;
                next.offset = nextOffset + 1;
                next.issuedAtMs = nowMs;
                probe.nextCursor = encodeCursor(next);
            }
            probe.rows = result.rows;
            probe.rows.append(candidateRow);
            probe.work = result.work;
            probe.work.serializedRows = probe.rows.size();
            if (stablePayloadBytes(probe) > VCAuthoring::kMaxToolPayloadBytes)
            {
                result.code = QStringLiteral("itemTooLarge");
                result.error = QStringLiteral("A single result row exceeds the payload limit");
                if (!result.rows.isEmpty())
                {
                    result.code.clear();
                    result.error.clear();
                    result.complete = false;
                    CursorState next;
                    next.scope = QStringLiteral("setup");
                    next.basis = result.basis;
                    next.selectorHash = selectorHash;
                    next.instance = instanceId;
                    next.offset = nextOffset;
                    next.issuedAtMs = nowMs;
                    result.nextCursor = encodeCursor(next);
                    result.work.serializedRows = result.rows.size();
                    stablePayloadBytes(result);
                }
                else
                {
                    result.rows.clear();
                    result.complete = false;
                    result.nextCursor.clear();
                    result.work.serializedRows = 0;
                    result.work.hydratedRows = 0;
                    stablePayloadBytes(result);
                }
                return result;
            }
            result.rows.append(candidateRow);
            ++nextOffset;
        }

        result.complete = nextOffset >= total;
        if (!result.complete)
        {
            CursorState next;
            next.scope = QStringLiteral("setup");
            next.basis = result.basis;
            next.selectorHash = selectorHash;
            next.instance = instanceId;
            next.offset = nextOffset;
            next.issuedAtMs = nowMs;
            result.nextCursor = encodeCursor(next);
        }
        result.work.serializedRows = result.rows.size();
        if (!includeHydratedGroups)
            result.work.hydratedRows = 0;
        stablePayloadBytes(result);
        return result;
    }

    if (const AuthoringTargetControlQuery *controlsQuery = std::get_if<AuthoringTargetControlQuery>(&query))
    {
        AuthoringReadResult result;
        QString scope;

        if (controlsQuery->kind == AuthoringTargetControlQuery::Kind::Targets)
        {
            result.basisScope = QStringLiteral("vcstruct-v1");
            result.basis = authoringBasis();
            scope = QStringLiteral("controls-targets");
        }
        else if (controlsQuery->kind == AuthoringTargetControlQuery::Kind::Sources)
        {
            result.basisScope = QStringLiteral("catalogsources-v1");
            scope = QStringLiteral("controls-sources");
            QByteArray basisCanonical("catalogsources-v1;");
            InputOutputMap *ioMap = m_doc ? m_doc->inputOutputMap() : nullptr;
            const QList<Universe *> universes = ioMap ? ioMap->universes() : QList<Universe *>();
            basisCanonical += QByteArray::number(universes.size());
            basisCanonical += ';';
            for (Universe *universe : universes)
            {
                if (!universe)
                    continue;
                const quint32 universeId = universe->id();
                basisCanonical += "u:";
                basisCanonical += QByteArray::number(universeId);
                basisCanonical += ';';

                InputPatch *inputPatch = universe->inputPatch();
                if (inputPatch && inputPatch->isPatched() && inputPatch->plugin())
                {
                    basisCanonical += "in:";
                    basisCanonical += inputPatch->pluginName().toUtf8();
                    basisCanonical += ':';
                    basisCanonical += QByteArray::number(inputPatch->input());
                    basisCanonical += ';';
                }

                const int outputsCount = universe->outputPatchesCount();
                basisCanonical += "outN:";
                basisCanonical += QByteArray::number(outputsCount);
                basisCanonical += ';';
                for (int oi = 0; oi < outputsCount; ++oi)
                {
                    OutputPatch *outputPatch = universe->outputPatch(oi);
                    if (!outputPatch || !outputPatch->isPatched() || !outputPatch->plugin())
                        continue;
                    basisCanonical += "out:";
                    basisCanonical += outputPatch->pluginName().toUtf8();
                    basisCanonical += ':';
                    basisCanonical += QByteArray::number(outputPatch->output());
                    basisCanonical += ';';
                }

                OutputPatch *feedbackPatch = universe->feedbackPatch();
                if (feedbackPatch && feedbackPatch->isPatched() && feedbackPatch->plugin())
                {
                    basisCanonical += "fb:";
                    basisCanonical += feedbackPatch->pluginName().toUtf8();
                    basisCanonical += ':';
                    basisCanonical += QByteArray::number(feedbackPatch->output());
                    basisCanonical += ';';
                }
            }
            result.basis = QString::fromLatin1(
                QCryptographicHash::hash(basisCanonical, QCryptographicHash::Sha256).toHex());
        }
        else
        {
            result.basisScope = QStringLiteral("profilescatalog-v1");
            scope = QStringLiteral("controls-profiles");
            QByteArray basisCanonical("profilescatalog-v1;");
            InputOutputMap *ioMap = m_doc ? m_doc->inputOutputMap() : nullptr;
            const QStringList profileNames = ioMap ? ioMap->profileNames() : QStringList();
            basisCanonical += QByteArray::number(profileNames.size());
            basisCanonical += ';';
            for (const QString &profileName : profileNames)
            {
                basisCanonical += QByteArray::number(profileName.toUtf8().size());
                basisCanonical += ':';
                basisCanonical += profileName.toUtf8();
                basisCanonical += ';';
                QLCInputProfile *profile = ioMap ? ioMap->profile(profileName) : nullptr;
                if (!profile)
                    continue;
                basisCanonical += profile->manufacturer().toUtf8();
                basisCanonical += ';';
                basisCanonical += profile->model().toUtf8();
                basisCanonical += ';';
                basisCanonical += QLCInputProfile::typeToString(profile->type()).toUtf8();
                basisCanonical += ';';
                const QMap<quint32, QLCInputChannel *> channels = profile->channels();
                basisCanonical += QByteArray::number(channels.size());
                basisCanonical += ';';
                for (auto it = channels.constBegin(); it != channels.constEnd(); ++it)
                {
                    QLCInputChannel *channel = it.value();
                    if (!channel)
                        continue;
                    basisCanonical += QByteArray::number(it.key());
                    basisCanonical += ':';
                    basisCanonical += channel->name().toUtf8();
                    basisCanonical += ':';
                    basisCanonical += QLCInputChannel::typeToString(channel->type()).toUtf8();
                    basisCanonical += ':';
                    basisCanonical += QByteArray::number(int(channel->movementType()));
                    basisCanonical += ':';
                    basisCanonical += QByteArray::number(channel->movementSensitivity());
                    basisCanonical += ';';
                }
                const auto colorTable = profile->colorTable();
                basisCanonical += "ct:";
                basisCanonical += QByteArray::number(colorTable.size());
                basisCanonical += ';';
                for (auto it = colorTable.constBegin(); it != colorTable.constEnd(); ++it)
                {
                    basisCanonical += QByteArray::number(it.key());
                    basisCanonical += ':';
                    basisCanonical += it.value().first.toUtf8();
                    basisCanonical += ':';
                    basisCanonical += it.value().second.name(QColor::HexRgb).toUtf8();
                    basisCanonical += ';';
                }
                const auto midiTable = profile->midiChannelTable();
                basisCanonical += "mt:";
                basisCanonical += QByteArray::number(midiTable.size());
                basisCanonical += ';';
                for (auto it = midiTable.constBegin(); it != midiTable.constEnd(); ++it)
                {
                    basisCanonical += QByteArray::number(it.key());
                    basisCanonical += ':';
                    basisCanonical += it.value().toUtf8();
                    basisCanonical += ';';
                }
            }
            result.basis = QString::fromLatin1(
                QCryptographicHash::hash(basisCanonical, QCryptographicHash::Sha256).toHex());
        }

        if (controlsQuery->limit < 1 || controlsQuery->limit > 250)
        {
            result.code = QStringLiteral("invalidArgument");
            result.error = QStringLiteral("limit must be in range 1..250");
            return result;
        }
        if (controlsQuery->cursor.toUtf8().size() > 1024)
        {
            result.code = QStringLiteral("invalidArgument");
            result.error = QStringLiteral("cursor must be at most 1024 UTF-8 bytes");
            return result;
        }

        QByteArray selectorCanonical("controls;");
        selectorCanonical += QByteArray::number(int(controlsQuery->kind));
        selectorCanonical += ';';
        selectorCanonical += QByteArray::number(controlsQuery->limit);
        selectorCanonical += ';';
        selectorCanonical += controlsQuery->pluginName.toUtf8();
        selectorCanonical += ';';
        selectorCanonical += controlsQuery->profileName.toUtf8();
        selectorCanonical += ';';
        selectorCanonical += QByteArray::number(controlsQuery->inputUniverseID);
        selectorCanonical += ';';
        for (quint32 id : controlsQuery->widgetIDs)
        {
            selectorCanonical += QByteArray::number(id);
            selectorCanonical += ',';
        }
        selectorCanonical += ';';
        for (const QString &field : controlsQuery->fields)
        {
            selectorCanonical += field.toUtf8();
            selectorCanonical += ',';
        }
        const QString baseSelectorHash = QString::fromLatin1(
            QCryptographicHash::hash(selectorCanonical, QCryptographicHash::Sha256).toHex());
        QString selectorHash = baseSelectorHash;

        int offset = 0;
        std::optional<CursorState> cursorState;
        if (!controlsQuery->cursor.isEmpty())
        {
            CursorState cursor;
            QString cursorError;
            if (!decodeCursor(controlsQuery->cursor, &cursor, &cursorError))
            {
                result.code = QStringLiteral("invalidCursor");
                result.error = cursorError;
                return result;
            }
            if (cursor.version != 1 || cursor.scope != scope || cursor.instance != instanceId)
            {
                result.code = QStringLiteral("invalidCursor");
                result.error = QStringLiteral("cursor does not match this query scope");
                return result;
            }
            if (cursor.basis != result.basis)
            {
                result.code = QStringLiteral("staleBasis");
                result.error = QStringLiteral("Authoring basis mismatch");
                return result;
            }
            if (cursor.issuedAtMs + kAuthoringRecordTtlMs <= nowMs)
            {
                result.code = QStringLiteral("cursorExpired");
                result.error = QStringLiteral("cursor expired");
                return result;
            }
            offset = cursor.offset;
            cursorState = cursor;
        }

        QSet<QString> requestedFields;
        for (const QString &field : controlsQuery->fields)
            requestedFields.insert(field);
        const bool defaultGroups = requestedFields.isEmpty();
        if (controlsQuery->kind != AuthoringTargetControlQuery::Kind::Targets &&
            cursorState.has_value() && cursorState->selectorHash != selectorHash)
        {
            result.code = QStringLiteral("invalidCursor");
            result.error = QStringLiteral("cursor does not match this query scope");
            return result;
        }

        if (controlsQuery->kind == AuthoringTargetControlQuery::Kind::Targets)
        {
            struct TargetCandidate
            {
                VCWidget *widget = nullptr;
                quint32 widgetID = quint32(-1);
                quint8 controlSlotID = 0;
                QString label;
                QString sourceName;
            };

            const bool includeDomains = defaultGroups || requestedFields.contains(QStringLiteral("domains"));
            const bool includeBindings = requestedFields.contains(QStringLiteral("bindings"));
            const bool includeFeedback = requestedFields.contains(QStringLiteral("feedback"));

            QList<TargetCandidate> candidates;
            for (quint32 widgetID : controlsQuery->widgetIDs)
            {
                VCWidget *widget = m_vc->widget(widgetID);
                if (!widget)
                {
                    result.code = QStringLiteral("invalidArgument");
                    result.error = QStringLiteral("widgetIDs contains unknown widget ID %1").arg(widgetID);
                    return result;
                }

                const QVariant controls = widget->externalControlsList();
                if (!controls.canConvert<QVariantList>())
                    continue;

                const QVariantList controlList = controls.toList();
                for (const QVariant &entry : controlList)
                {
                    const QVariantMap map = entry.toMap();
                    if (!map.contains(QStringLiteral("mValue")) || !map.value(QStringLiteral("mValue")).canConvert<int>())
                        continue;
                    const int slot = map.value(QStringLiteral("mValue")).toInt();
                    if (slot < 0 || slot > std::numeric_limits<quint8>::max())
                        continue;
                    TargetCandidate candidate;
                    candidate.widget = widget;
                    candidate.widgetID = widgetID;
                    candidate.controlSlotID = quint8(slot);
                    candidate.label = map.value(QStringLiteral("mLabel")).toString();
                    candidate.sourceName = resolveSourceName(widget, quint32(slot));
                    candidates.append(candidate);
                }
            }

            QByteArray inventoryCanonical("targets;");
            for (const TargetCandidate &candidate : std::as_const(candidates))
            {
                inventoryCanonical += QByteArray::number(candidate.widgetID);
                inventoryCanonical += ':';
                inventoryCanonical += QByteArray::number(candidate.controlSlotID);
                inventoryCanonical += ';';
            }
            selectorHash = QString::fromLatin1(
                QCryptographicHash::hash(baseSelectorHash.toUtf8() + ';' + inventoryCanonical,
                                         QCryptographicHash::Sha256).toHex());
            if (cursorState.has_value() && cursorState->selectorHash != selectorHash)
            {
                result.code = QStringLiteral("invalidCursor");
                result.error = QStringLiteral("cursor does not match this query scope");
                return result;
            }

            result.work.scannedSummaryRows = candidates.size();
            if (offset > candidates.size())
            {
                result.code = QStringLiteral("invalidCursor");
                result.error = QStringLiteral("cursor offset is out of range");
                return result;
            }

            auto classifyTarget = [&](const TargetCandidate &candidate,
                                      AuthoringReadRow *row) {
                AuthoringReadRow::TargetRow target;
                target.widgetID = candidate.widgetID;
                target.controlSlotID = candidate.controlSlotID;
                target.label = candidate.label;
                target.sourceName = candidate.sourceName;

                const QString className = QString::fromLatin1(candidate.widget->metaObject()->className());
                if (includeDomains)
                {
                    target.validationLevel = QStringLiteral("nativeUnknown");
                    target.assignmentSupported = false;
                }

                auto known = [&](const QString &valueDomain, bool keyboardAllowed) {
                    if (!includeDomains)
                        return;
                    target.validationLevel = QStringLiteral("nativeKnown");
                    target.assignmentSupported = true;
                    target.valueDomain = valueDomain;
                    target.keyboardAllowed = keyboardAllowed;
                };

                if (className == QStringLiteral("VCButton"))
                {
                    known(QStringLiteral("triggerNonZero"), true);
                    if (includeDomains)
                    {
                        row->limitations.append({
                            QStringLiteral("behaviorConditional"),
                            QStringLiteral("widget:%1/slot:%2").arg(candidate.widgetID).arg(candidate.controlSlotID),
                            QStringLiteral("Button hold/latch behavior depends on action type")
                        });
                    }
                }
                else if (className == QStringLiteral("VCSlider"))
                {
                    if (candidate.controlSlotID == 0)
                        known(QStringLiteral("continuousByte"), false);
                    else if (candidate.controlSlotID == 1)
                        known(QStringLiteral("triggerNonZero"), false);
                    else if (candidate.controlSlotID == 2)
                        known(QStringLiteral("holdByte"), true);
                }
                else if (className == QStringLiteral("VCXYPad"))
                {
                    if (candidate.controlSlotID <= 6)
                        known(QStringLiteral("continuousByte"), false);
                    else if (candidate.controlSlotID >= 30)
                        known(QStringLiteral("trigger255"), true);
                }
                else if (className == QStringLiteral("VCSpeedDial"))
                {
                    if (candidate.controlSlotID == 0)
                        known(QStringLiteral("continuousByte"), false);
                    else if (candidate.controlSlotID >= 1)
                        known(QStringLiteral("trigger255"), true);
                }
                else if (className == QStringLiteral("VCCueList"))
                {
                    if (candidate.controlSlotID <= 3)
                        known(QStringLiteral("triggerNonZero"), true);
                    else if (candidate.controlSlotID == 4)
                        known(QStringLiteral("continuousByte"), false);
                }
                else if (className == QStringLiteral("VCFrame") || className == QStringLiteral("VCSoloFrame"))
                {
                    if (candidate.controlSlotID <= 3 || candidate.controlSlotID >= INPUT_SHORTCUT_BASE_ID)
                        known(QStringLiteral("trigger255"), true);
                }
                else if (className == QStringLiteral("VCAudioTriggers"))
                {
                    if (candidate.controlSlotID == 0)
                        known(QStringLiteral("triggerNonZero"), true);
                    else if (candidate.controlSlotID == 1)
                        known(QStringLiteral("continuousByte"), false);
                }
                else if (className == QStringLiteral("VCClock"))
                {
                    if (candidate.controlSlotID <= 1)
                        known(QStringLiteral("trigger255"), true);
                }
                else if (className == QStringLiteral("VCAnimation"))
                {
                    if (candidate.controlSlotID == 0)
                        known(QStringLiteral("continuousByte"), true);
                }

                const QList<QSharedPointer<QLCInputSource>> sources = candidate.widget->inputSources();
                QMap<QString, int> pairCounts;
                for (const QSharedPointer<QLCInputSource> &source : sources)
                {
                    if (source.isNull())
                        continue;
                    const QString pair = QStringLiteral("%1:%2").arg(source->universe()).arg(source->channel());
                    pairCounts[pair] = pairCounts.value(pair) + 1;
                }

                if (includeBindings)
                {
                    AuthoringReadRow::SetupWidgetRow::InputBindingGroup group;
                    for (int i = 0; i < sources.size(); ++i)
                    {
                        const QSharedPointer<QLCInputSource> &source = sources.at(i);
                        if (source.isNull() || source->id() != candidate.controlSlotID)
                            continue;
                        AuthoringReadRow::SetupWidgetRow::InputBindingGroup::Entry entry;
                        entry.sourceKey.controlSlotID = candidate.controlSlotID;
                        entry.sourceKey.inputUniverseID = source->universe();
                        entry.sourceKey.inputChannel = source->channel();
                        entry.sourceKey.childSubpage = int(source->page());
                        entry.sourceOrdinal = i;
                        entry.sourceName = resolveSourceName(candidate.widget, source->id());
                        entry.configurationDigest = scopedConfigurationDigest(entry.sourceKey, source.data());
                        const QString pair = QStringLiteral("%1:%2").arg(source->universe()).arg(source->channel());
                        entry.duplicate = pairCounts.value(pair) > 1;
                        entry.ambiguous = entry.duplicate;
                        group.entries.append(entry);
                    }
                    target.inputBindings = group;
                    target.configurationDigest = scopedConfigurationDigest(candidate.widget);
                }

                if (includeFeedback)
                {
                    AuthoringReadRow::SetupWidgetRow::FeedbackGroup group;
                    for (int i = 0; i < sources.size(); ++i)
                    {
                        const QSharedPointer<QLCInputSource> &source = sources.at(i);
                        if (source.isNull() || source->id() != candidate.controlSlotID)
                            continue;
                        AuthoringReadRow::SetupWidgetRow::FeedbackGroup::Entry entry;
                        entry.sourceKey.controlSlotID = candidate.controlSlotID;
                        entry.sourceKey.inputUniverseID = source->universe();
                        entry.sourceKey.inputChannel = source->channel();
                        entry.sourceKey.childSubpage = int(source->page());
                        entry.sourceOrdinal = i;
                        entry.idleValue = int(source->feedbackValue(QLCInputFeedback::LowerValue));
                        entry.activeValue = int(source->feedbackValue(QLCInputFeedback::UpperValue));
                        entry.monitorValue = int(source->feedbackValue(QLCInputFeedback::MonitorValue));
                        entry.idleChannel = source->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
                        entry.activeChannel = source->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
                        entry.monitorChannel = source->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
                        entry.configurationDigest = scopedConfigurationDigest(entry.sourceKey, source.data());
                        group.entries.append(entry);
                    }
                    target.feedback = group;
                }

                if (includeDomains && target.assignmentSupported.has_value() && !target.assignmentSupported.value())
                {
                    row->limitations.append({
                        QStringLiteral("unsupportedProjection"),
                        QStringLiteral("widget:%1/slot:%2").arg(candidate.widgetID).arg(candidate.controlSlotID),
                        QStringLiteral("Native target slot semantics are not fully typed for this control")
                    });
                }
                row->row = target;
            };

            const int total = candidates.size();
            int nextOffset = offset;
            result.rows.reserve(controlsQuery->limit);
            while (nextOffset < total && int(result.rows.size()) < controlsQuery->limit)
            {
                AuthoringReadRow candidateRow;
                classifyTarget(candidates.at(nextOffset), &candidateRow);
                ++result.work.hydratedRows;

                AuthoringReadResult probe;
                probe.version = result.version;
                probe.basis = result.basis;
                probe.basisScope = result.basisScope;
                probe.complete = (nextOffset + 1) >= total;
                if (!probe.complete)
                {
                    CursorState next;
                    next.scope = scope;
                    next.basis = result.basis;
                    next.selectorHash = selectorHash;
                    next.instance = instanceId;
                    next.offset = nextOffset + 1;
                    next.issuedAtMs = nowMs;
                    probe.nextCursor = encodeCursor(next);
                }
                probe.rows = result.rows;
                probe.rows.append(candidateRow);
                probe.work = result.work;
                probe.work.serializedRows = probe.rows.size();
                if (stablePayloadBytes(probe) > VCAuthoring::kMaxToolPayloadBytes)
                {
                    result.code = QStringLiteral("itemTooLarge");
                    result.error = QStringLiteral("A single result row exceeds the payload limit");
                    if (!result.rows.isEmpty())
                    {
                        result.code.clear();
                        result.error.clear();
                        result.complete = false;
                        CursorState next;
                        next.scope = scope;
                        next.basis = result.basis;
                        next.selectorHash = selectorHash;
                        next.instance = instanceId;
                        next.offset = nextOffset;
                        next.issuedAtMs = nowMs;
                        result.nextCursor = encodeCursor(next);
                        result.work.serializedRows = result.rows.size();
                        stablePayloadBytes(result);
                    }
                    else
                    {
                        result.rows.clear();
                        result.complete = false;
                        result.nextCursor.clear();
                        result.work.serializedRows = 0;
                        result.work.hydratedRows = 0;
                        stablePayloadBytes(result);
                    }
                    return result;
                }
                result.rows.append(candidateRow);
                ++nextOffset;
            }

            result.complete = nextOffset >= total;
            if (!result.complete)
            {
                CursorState next;
                next.scope = scope;
                next.basis = result.basis;
                next.selectorHash = selectorHash;
                next.instance = instanceId;
                next.offset = nextOffset;
                next.issuedAtMs = nowMs;
                result.nextCursor = encodeCursor(next);
            }
            result.work.serializedRows = result.rows.size();
            stablePayloadBytes(result);
            return result;
        }

        if (controlsQuery->kind == AuthoringTargetControlQuery::Kind::Sources)
        {
            struct SourceCandidate
            {
                QString pluginName;
                QString direction;
                std::optional<quint32> inputLine;
                std::optional<quint32> outputLine;
                std::optional<QString> inputUID;
                std::optional<QString> outputUID;
                std::optional<quint32> inputUniverseID;
                std::optional<QString> status;
                std::optional<QString> reason;
                QString configurationDigest;
            };

            const bool includePatches = requestedFields.contains(QStringLiteral("patches"));
            const bool includeStatus = requestedFields.contains(QStringLiteral("status"));

            QList<SourceCandidate> candidates;
            InputOutputMap *ioMap = m_doc ? m_doc->inputOutputMap() : nullptr;
            const QList<Universe *> universes = ioMap ? ioMap->universes() : QList<Universe *>();
            for (Universe *universe : universes)
            {
                if (!universe)
                    continue;
                const quint32 universeId = universe->id();
                if (controlsQuery->inputUniverseID != quint32(-1) &&
                    controlsQuery->inputUniverseID != universeId)
                    continue;

                InputPatch *inputPatch = universe->inputPatch();
                if (inputPatch && inputPatch->isPatched() && inputPatch->plugin())
                {
                    SourceCandidate candidate;
                    candidate.pluginName = inputPatch->pluginName();
                    candidate.direction = QStringLiteral("input");
                    candidate.inputLine = inputPatch->input();
                    candidate.inputUniverseID = universeId;
                    if (includeStatus)
                        candidate.reason = QStringLiteral("passive status is unavailable for this source");
                    candidate.configurationDigest = QStringLiteral("%1|%2|%3|%4")
                        .arg(candidate.pluginName)
                        .arg(*candidate.inputLine)
                        .arg(QString())
                        .arg(candidate.status.value_or(QString()));
                    if (controlsQuery->pluginName.isEmpty() || candidate.pluginName == controlsQuery->pluginName)
                        candidates.append(candidate);
                }

                const int outputsCount = universe->outputPatchesCount();
                for (int oi = 0; oi < outputsCount; ++oi)
                {
                    OutputPatch *outputPatch = universe->outputPatch(oi);
                    if (!outputPatch || !outputPatch->isPatched() || !outputPatch->plugin())
                        continue;
                    SourceCandidate candidate;
                    candidate.pluginName = outputPatch->pluginName();
                    candidate.direction = QStringLiteral("output");
                    candidate.outputLine = outputPatch->output();
                    candidate.inputUniverseID = universeId;
                    if (includeStatus)
                        candidate.reason = QStringLiteral("passive status is unavailable for this source");
                    candidate.configurationDigest = QStringLiteral("%1|%2|%3")
                        .arg(candidate.pluginName)
                        .arg(*candidate.outputLine)
                        .arg(QString());
                    if (controlsQuery->pluginName.isEmpty() || candidate.pluginName == controlsQuery->pluginName)
                        candidates.append(candidate);
                }

                OutputPatch *feedbackPatch = universe->feedbackPatch();
                if (feedbackPatch && feedbackPatch->isPatched() && feedbackPatch->plugin())
                {
                    SourceCandidate candidate;
                    candidate.pluginName = feedbackPatch->pluginName();
                    candidate.direction = QStringLiteral("feedback");
                    candidate.outputLine = feedbackPatch->output();
                    candidate.inputUniverseID = universeId;
                    if (includeStatus)
                        candidate.reason = QStringLiteral("passive status is unavailable for this source");
                    candidate.configurationDigest = QStringLiteral("%1|%2|%3")
                        .arg(candidate.pluginName)
                        .arg(*candidate.outputLine)
                        .arg(QString());
                    if (controlsQuery->pluginName.isEmpty() || candidate.pluginName == controlsQuery->pluginName)
                        candidates.append(candidate);
                }
            }

            result.work.scannedSummaryRows = candidates.size();
            if (offset > candidates.size())
            {
                result.code = QStringLiteral("invalidCursor");
                result.error = QStringLiteral("cursor offset is out of range");
                return result;
            }

            const int total = candidates.size();
            int nextOffset = offset;
            result.rows.reserve(controlsQuery->limit);
            while (nextOffset < total && int(result.rows.size()) < controlsQuery->limit)
            {
                const SourceCandidate &candidate = candidates.at(nextOffset);
                AuthoringReadRow row;
                AuthoringReadRow::SourceRow sourceRow;
                sourceRow.pluginName = candidate.pluginName;
                sourceRow.direction = candidate.direction;
                sourceRow.sourceOrdinal = nextOffset;
                sourceRow.inputUniverseID = candidate.inputUniverseID;
                if (includePatches)
                {
                    sourceRow.inputLine = candidate.inputLine;
                    sourceRow.outputLine = candidate.outputLine;
                    sourceRow.inputUID = candidate.inputUID;
                    sourceRow.outputUID = candidate.outputUID;
                    const QByteArray digestBytes = QCryptographicHash::hash(
                        candidate.configurationDigest.toUtf8(), QCryptographicHash::Sha256).toHex();
                    sourceRow.configurationDigest = QString::fromLatin1(digestBytes);
                }
                if (includeStatus)
                {
                    if (candidate.status.has_value())
                        sourceRow.status = candidate.status;
                    if (candidate.reason.has_value())
                        sourceRow.reason = candidate.reason;
                }
                row.row = sourceRow;
                if (includePatches)
                {
                    const bool missingUid = (candidate.direction == QStringLiteral("input") && !candidate.inputUID.has_value()) ||
                        ((candidate.direction == QStringLiteral("output") || candidate.direction == QStringLiteral("feedback")) &&
                         !candidate.outputUID.has_value());
                    if (missingUid)
                    {
                        row.limitations.append({
                            QStringLiteral("uidUnavailable"),
                            QStringLiteral("plugin:%1/universe:%2/direction:%3")
                                .arg(candidate.pluginName)
                                .arg(candidate.inputUniverseID.value_or(quint32(-1)))
                                .arg(candidate.direction),
                            QStringLiteral("Native source UID is unavailable for this patch")
                        });
                    }
                }
                if (includeStatus && !candidate.status.has_value())
                {
                    row.limitations.append({
                        QStringLiteral("statusUnavailable"),
                        QStringLiteral("plugin:%1/universe:%2").arg(candidate.pluginName).arg(candidate.inputUniverseID.value_or(quint32(-1))),
                        QStringLiteral("Native status is unavailable for this source direction")
                    });
                }

                ++result.work.hydratedRows;
                AuthoringReadResult probe;
                probe.version = result.version;
                probe.basis = result.basis;
                probe.basisScope = result.basisScope;
                probe.complete = (nextOffset + 1) >= total;
                if (!probe.complete)
                {
                    CursorState next;
                    next.scope = scope;
                    next.basis = result.basis;
                    next.selectorHash = selectorHash;
                    next.instance = instanceId;
                    next.offset = nextOffset + 1;
                    next.issuedAtMs = nowMs;
                    probe.nextCursor = encodeCursor(next);
                }
                probe.rows = result.rows;
                probe.rows.append(row);
                probe.work = result.work;
                probe.work.serializedRows = probe.rows.size();
                if (stablePayloadBytes(probe) > VCAuthoring::kMaxToolPayloadBytes)
                {
                    result.code = QStringLiteral("itemTooLarge");
                    result.error = QStringLiteral("A single result row exceeds the payload limit");
                    if (!result.rows.isEmpty())
                    {
                        result.code.clear();
                        result.error.clear();
                        result.complete = false;
                        CursorState next;
                        next.scope = scope;
                        next.basis = result.basis;
                        next.selectorHash = selectorHash;
                        next.instance = instanceId;
                        next.offset = nextOffset;
                        next.issuedAtMs = nowMs;
                        result.nextCursor = encodeCursor(next);
                        result.work.serializedRows = result.rows.size();
                        stablePayloadBytes(result);
                    }
                    else
                    {
                        result.rows.clear();
                        result.complete = false;
                        result.nextCursor.clear();
                        result.work.serializedRows = 0;
                        result.work.hydratedRows = 0;
                        stablePayloadBytes(result);
                    }
                    return result;
                }
                result.rows.append(row);
                ++nextOffset;
            }

            result.complete = nextOffset >= total;
            if (!result.complete)
            {
                CursorState next;
                next.scope = scope;
                next.basis = result.basis;
                next.selectorHash = selectorHash;
                next.instance = instanceId;
                next.offset = nextOffset;
                next.issuedAtMs = nowMs;
                result.nextCursor = encodeCursor(next);
            }
            result.work.serializedRows = result.rows.size();
            stablePayloadBytes(result);
            return result;
        }

        // Profiles
        struct ProfileCatalogEntry
        {
            QString profileName;
            QLCInputProfile *profile = nullptr;
        };

        const bool includeChannelMap = requestedFields.contains(QStringLiteral("channelMap"));
        const bool includeFeedbackTables = requestedFields.contains(QStringLiteral("feedbackTables"));

        QList<ProfileCatalogEntry> profiles;
        InputOutputMap *ioMap = m_doc ? m_doc->inputOutputMap() : nullptr;
        const QStringList profileNames = ioMap ? ioMap->profileNames() : QStringList();
        for (const QString &profileName : profileNames)
        {
            if (!controlsQuery->profileName.isEmpty() && profileName != controlsQuery->profileName)
                continue;
            QLCInputProfile *profile = ioMap ? ioMap->profile(profileName) : nullptr;
            if (!profile)
                continue;
            profiles.append({profileName, profile});
        }

        QList<AuthoringReadRow> candidates;
        for (const ProfileCatalogEntry &entry : profiles)
        {
            AuthoringReadRow baseRow;
            AuthoringReadRow::ProfileRow profileRow;
            profileRow.profileName = entry.profileName;
            profileRow.manufacturer = entry.profile->manufacturer();
            profileRow.model = entry.profile->model();
            profileRow.type = QLCInputProfile::typeToString(entry.profile->type());
            const QByteArray digestBytes = QCryptographicHash::hash(
                QStringLiteral("%1|%2|%3|%4")
                    .arg(profileRow.profileName, profileRow.manufacturer, profileRow.model, profileRow.type)
                    .toUtf8(),
                QCryptographicHash::Sha256).toHex();
            profileRow.configurationDigest = QString::fromLatin1(digestBytes);
            baseRow.row = profileRow;
            candidates.append(baseRow);

            if (includeChannelMap)
            {
                const QMap<quint32, QLCInputChannel *> channels = entry.profile->channels();
                for (auto it = channels.constBegin(); it != channels.constEnd(); ++it)
                {
                    QLCInputChannel *channel = it.value();
                    if (!channel)
                        continue;
                    AuthoringReadRow channelRow;
                    AuthoringReadRow::ProfileChannelRow detail;
                    detail.profileName = entry.profileName;
                    detail.profileChannelNumber = int(it.key()) + 1;
                    detail.inputChannel = it.key();
                    detail.name = channel->name();
                    detail.type = QLCInputChannel::typeToString(channel->type());
                    detail.movement = channel->movementType() == QLCInputChannel::Relative
                        ? AuthoringReadRow::ProfileChannelRow::Movement::Relative
                        : AuthoringReadRow::ProfileChannelRow::Movement::Absolute;
                    detail.sensitivity = channel->movementSensitivity();
                    const QByteArray rowDigest = QCryptographicHash::hash(
                        QStringLiteral("%1|%2|%3|%4|%5|%6")
                            .arg(detail.profileName)
                            .arg(detail.profileChannelNumber)
                            .arg(detail.inputChannel)
                            .arg(detail.name)
                            .arg(detail.type)
                            .arg(detail.sensitivity)
                            .toUtf8(),
                        QCryptographicHash::Sha256).toHex();
                    detail.configurationDigest = QString::fromLatin1(rowDigest);
                    channelRow.row = detail;
                    candidates.append(channelRow);
                }
            }

            if (includeFeedbackTables)
            {
                const auto colorTable = entry.profile->colorTable();
                for (auto it = colorTable.constBegin(); it != colorTable.constEnd(); ++it)
                {
                    AuthoringReadRow tableRow;
                    AuthoringReadRow::ProfileFeedbackTableRow detail;
                    detail.profileName = entry.profileName;
                    detail.table = QStringLiteral("color");
                    detail.key = int(it.key());
                    detail.label = it.value().first;
                    detail.color = it.value().second.name(QColor::HexRgb);
                    const QByteArray rowDigest = QCryptographicHash::hash(
                        QStringLiteral("%1|%2|%3|%4|%5")
                            .arg(detail.profileName)
                            .arg(detail.table)
                            .arg(detail.key)
                            .arg(detail.label.value_or(QString()))
                            .arg(detail.color.value_or(QString()))
                            .toUtf8(),
                        QCryptographicHash::Sha256).toHex();
                    detail.configurationDigest = QString::fromLatin1(rowDigest);
                    tableRow.row = detail;
                    candidates.append(tableRow);
                }

                const auto midiTable = entry.profile->midiChannelTable();
                for (auto it = midiTable.constBegin(); it != midiTable.constEnd(); ++it)
                {
                    AuthoringReadRow tableRow;
                    AuthoringReadRow::ProfileFeedbackTableRow detail;
                    detail.profileName = entry.profileName;
                    detail.table = QStringLiteral("midiChannel");
                    detail.key = int(it.key());
                    detail.label = it.value();
                    const QByteArray rowDigest = QCryptographicHash::hash(
                        QStringLiteral("%1|%2|%3|%4")
                            .arg(detail.profileName)
                            .arg(detail.table)
                            .arg(detail.key)
                            .arg(detail.label.value_or(QString()))
                            .toUtf8(),
                        QCryptographicHash::Sha256).toHex();
                    detail.configurationDigest = QString::fromLatin1(rowDigest);
                    tableRow.row = detail;
                    candidates.append(tableRow);
                }
            }
        }

        result.work.scannedSummaryRows = candidates.size();
        if (offset > candidates.size())
        {
            result.code = QStringLiteral("invalidCursor");
            result.error = QStringLiteral("cursor offset is out of range");
            return result;
        }

        const int total = candidates.size();
        int nextOffset = offset;
        result.rows.reserve(controlsQuery->limit);
        while (nextOffset < total && int(result.rows.size()) < controlsQuery->limit)
        {
            const AuthoringReadRow candidateRow = candidates.at(nextOffset);
            ++result.work.hydratedRows;
            AuthoringReadResult probe;
            probe.version = result.version;
            probe.basis = result.basis;
            probe.basisScope = result.basisScope;
            probe.complete = (nextOffset + 1) >= total;
            if (!probe.complete)
            {
                CursorState next;
                next.scope = scope;
                next.basis = result.basis;
                next.selectorHash = selectorHash;
                next.instance = instanceId;
                next.offset = nextOffset + 1;
                next.issuedAtMs = nowMs;
                probe.nextCursor = encodeCursor(next);
            }
            probe.rows = result.rows;
            probe.rows.append(candidateRow);
            probe.work = result.work;
            probe.work.serializedRows = probe.rows.size();
            if (stablePayloadBytes(probe) > VCAuthoring::kMaxToolPayloadBytes)
            {
                result.code = QStringLiteral("itemTooLarge");
                result.error = QStringLiteral("A single result row exceeds the payload limit");
                if (!result.rows.isEmpty())
                {
                    result.code.clear();
                    result.error.clear();
                    result.complete = false;
                    CursorState next;
                    next.scope = scope;
                    next.basis = result.basis;
                    next.selectorHash = selectorHash;
                    next.instance = instanceId;
                    next.offset = nextOffset;
                    next.issuedAtMs = nowMs;
                    result.nextCursor = encodeCursor(next);
                    result.work.serializedRows = result.rows.size();
                    stablePayloadBytes(result);
                }
                else
                {
                    result.rows.clear();
                    result.complete = false;
                    result.nextCursor.clear();
                    result.work.serializedRows = 0;
                    result.work.hydratedRows = 0;
                    stablePayloadBytes(result);
                }
                return result;
            }
            result.rows.append(candidateRow);
            ++nextOffset;
        }

        result.complete = nextOffset >= total;
        if (!result.complete)
        {
            CursorState next;
            next.scope = scope;
            next.basis = result.basis;
            next.selectorHash = selectorHash;
            next.instance = instanceId;
            next.offset = nextOffset;
            next.issuedAtMs = nowMs;
            result.nextCursor = encodeCursor(next);
        }
        result.work.serializedRows = result.rows.size();
        stablePayloadBytes(result);
        return result;
    }

    if (const AuthoringRecordingQuery *recordingQuery = std::get_if<AuthoringRecordingQuery>(&query))
    {
        AuthoringReadResult result;
        result.basisScope = QStringLiteral("showtrack-v1");

        if (recordingQuery->limit < 1 || recordingQuery->limit > 250)
        {
            result.code = QStringLiteral("invalidArgument");
            result.error = QStringLiteral("limit must be in range 1..250");
            return result;
        }
        if (recordingQuery->cursor.toUtf8().size() > 1024)
        {
            result.code = QStringLiteral("invalidArgument");
            result.error = QStringLiteral("cursor must be at most 1024 UTF-8 bytes");
            return result;
        }
        if (recordingQuery->fromTimeMs >= 0 && recordingQuery->toTimeMs >= 0 &&
            recordingQuery->toTimeMs < recordingQuery->fromTimeMs)
        {
            result.code = QStringLiteral("invalidArgument");
            result.error = QStringLiteral("toTimeMs must be greater than or equal to fromTimeMs");
            return result;
        }

        Show *show = qobject_cast<Show *>(m_doc->function(recordingQuery->showFunctionID));
        if (!show)
        {
            result.code = QStringLiteral("invalidArgument");
            result.error = QStringLiteral("showFunctionID is unknown");
            return result;
        }

        const ShowCommandTrack track = show->commandTrack();
        QByteArray basisCanonical("showtrack-v1;");
        basisCanonical += QByteArray::number(show->id());
        basisCanonical += ';';
        basisCanonical += QByteArray::number(track.count());
        basisCanonical += ';';
        basisCanonical += QByteArray::number(track.extent());
        basisCanonical += ';';
        basisCanonical += QByteArray::number(m_vc->pagesCount());
        basisCanonical += ';';
        for (int i = 0; i < m_vc->pagesCount(); ++i)
        {
            VCPage *page = m_vc->page(i);
            if (!page)
                continue;
            basisCanonical += "p:";
            basisCanonical += QByteArray::number(i);
            basisCanonical += ':';
            basisCanonical += QByteArray::number(page->id());
            basisCanonical += ':';
            basisCanonical += page->caption().toUtf8();
            basisCanonical += ':';
            basisCanonical += VCPage::externalInputModeToString(page->externalInputMode()).toUtf8();
            basisCanonical += ';';
            const QList<VCWidget *> widgets = page->children(true);
            for (VCWidget *widget : widgets)
            {
                VCFrame *frame = qobject_cast<VCFrame *>(widget);
                if (!frame)
                    continue;
                basisCanonical += "f:";
                basisCanonical += QByteArray::number(frame->id());
                basisCanonical += ':';
                basisCanonical += QByteArray::number(frame->totalPagesNumber());
                basisCanonical += ':';
                basisCanonical += frame->pagesLoop() ? "1" : "0";
                basisCanonical += ':';
                const QStringList labels = frame->pageLabels();
                for (int li = 0; li < labels.size(); ++li)
                {
                    if (li > 0)
                        basisCanonical += '|';
                    basisCanonical += labels.at(li).toUtf8();
                }
                basisCanonical += ';';
            }
        }
        const QVector<ShowCommand> authored = track.commands();
        for (const ShowCommand &cmd : authored)
        {
            basisCanonical += QByteArray::number(cmd.id);
            basisCanonical += ';';
            basisCanonical += QByteArray::number(cmd.time);
            basisCanonical += ';';
            basisCanonical += QByteArray::number(cmd.order);
            basisCanonical += ';';
            basisCanonical += QByteArray::number(cmd.functionId);
            basisCanonical += ';';
            basisCanonical += ShowCommand::actionToString(cmd.action).toUtf8();
            basisCanonical += ';';
            basisCanonical += cmd.controlId.toString(QUuid::WithoutBraces).toUtf8();
            basisCanonical += ';';
            basisCanonical += ShowCommand::roleToString(cmd.role).toUtf8();
            basisCanonical += ';';
            basisCanonical += cmd.attribute.toUtf8();
            basisCanonical += ';';
            basisCanonical += (cmd.on ? "1" : "0");
            basisCanonical += ';';
            basisCanonical += QByteArray::number(cmd.position, 'g', 17);
            basisCanonical += ';';
            basisCanonical += QByteArray::number(cmd.intensity, 'g', 17);
            basisCanonical += ';';
            basisCanonical += QByteArray::number(cmd.pairId);
            basisCanonical += ';';
            basisCanonical += cmd.payload.encode(cmd.action).toUtf8();
            basisCanonical += ';';
        }
        result.basis = QString::fromLatin1(
            QCryptographicHash::hash(basisCanonical, QCryptographicHash::Sha256).toHex());

        QByteArray selectorCanonical("recording;");
        selectorCanonical += QByteArray::number(int(recordingQuery->kind));
        selectorCanonical += ';';
        selectorCanonical += QByteArray::number(recordingQuery->showFunctionID);
        selectorCanonical += ';';
        selectorCanonical += recordingQuery->controlRecordingUUID.toUtf8();
        selectorCanonical += ';';
        selectorCanonical += QByteArray::number(recordingQuery->fromTimeMs);
        selectorCanonical += ';';
        selectorCanonical += QByteArray::number(recordingQuery->toTimeMs);
        selectorCanonical += ';';
        for (const QString &field : recordingQuery->fields)
        {
            selectorCanonical += field.toUtf8();
            selectorCanonical += ',';
        }
        const QString selectorHash = QString::fromLatin1(
            QCryptographicHash::hash(selectorCanonical, QCryptographicHash::Sha256).toHex());

        int offset = 0;
        if (!recordingQuery->cursor.isEmpty())
        {
            CursorState cursor;
            QString cursorError;
            if (!decodeCursor(recordingQuery->cursor, &cursor, &cursorError))
            {
                result.code = QStringLiteral("invalidCursor");
                result.error = cursorError;
                return result;
            }
            if (cursor.version != 1 || cursor.scope != QStringLiteral("recordings") ||
                cursor.instance != instanceId || cursor.selectorHash != selectorHash)
            {
                result.code = QStringLiteral("invalidCursor");
                result.error = QStringLiteral("cursor does not match this query scope");
                return result;
            }
            if (cursor.basis != result.basis)
            {
                result.code = QStringLiteral("staleBasis");
                result.error = QStringLiteral("Authoring basis mismatch");
                return result;
            }
            if (cursor.issuedAtMs + kAuthoringRecordTtlMs <= nowMs)
            {
                result.code = QStringLiteral("cursorExpired");
                result.error = QStringLiteral("cursor expired");
                return result;
            }
            offset = cursor.offset;
        }

        const QString controlFilter = recordingQuery->controlRecordingUUID.trimmed().toLower();
        auto matchesControlFilter = [&](const QUuid &controlId) -> bool {
            if (controlFilter.isEmpty())
                return true;
            return controlId.toString(QUuid::WithoutBraces).toLower() == controlFilter;
        };
        auto withinWindow = [&](qint64 timeMs) -> bool {
            if (recordingQuery->fromTimeMs >= 0 && timeMs < recordingQuery->fromTimeMs)
                return false;
            if (recordingQuery->toTimeMs >= 0 && timeMs > recordingQuery->toTimeMs)
                return false;
            return true;
        };
        auto statusKey = [](ShowControlStatus status) -> QString {
            switch (status)
            {
            case ShowControlStatus::Ready: return QStringLiteral("ready");
            case ShowControlStatus::Missing: return QStringLiteral("missing");
            case ShowControlStatus::Ambiguous: return QStringLiteral("ambiguous");
            case ShowControlStatus::Incompatible: return QStringLiteral("incompatible");
            case ShowControlStatus::Disabled: return QStringLiteral("disabled");
            case ShowControlStatus::Unbound: return QStringLiteral("unbound");
            }
            return QStringLiteral("missing");
        };

        QSet<QString> recordingFieldSet;
        for (const QString &field : recordingQuery->fields)
            recordingFieldSet.insert(field);
        const bool defaultProjection = recordingQuery->fields.isEmpty();
        const bool includeTiming = defaultProjection || recordingFieldSet.contains(QStringLiteral("timing"));
        const bool includeSemanticPayload = recordingFieldSet.contains(QStringLiteral("semanticPayload"));
        const bool includeReadiness = recordingFieldSet.contains(QStringLiteral("readiness"));

        auto payloadForCommand = [&](const ShowCommand &command) -> QVariantMap {
            QVariantMap payload;
            switch (command.action)
            {
            case ShowCommandAction::SetButtonState:
                payload.insert(QStringLiteral("on"), command.on);
                break;
            case ShowCommandAction::SetSliderPosition:
            case ShowCommandAction::SetSliderColors:
            case ShowCommandAction::SetAnimationFader:
                payload.insert(QStringLiteral("position"), command.position);
                if (command.payload.value.index() != 0)
                    payload.insert(QStringLiteral("argument"), command.nativeArgument());
                break;
            case ShowCommandAction::SetIntensity:
                payload.insert(QStringLiteral("intensity"), command.intensity);
                break;
            case ShowCommandAction::SetXYPadPosition:
            case ShowCommandAction::SetXYPadFloor:
            case ShowCommandAction::SetXYPadRanges:
            case ShowCommandAction::SetXYPadPositionPreset:
            case ShowCommandAction::SetXYPadFunctionPreset:
            case ShowCommandAction::SetXYPadGroupPreset:
            case ShowCommandAction::SetAnimationColor:
            case ShowCommandAction::SetAnimationContent:
            case ShowCommandAction::SetSliderChannel:
                payload.insert(QStringLiteral("encoded"), command.payload.encode(command.action));
                break;
            case ShowCommandAction::Start:
            case ShowCommandAction::Stop:
            case ShowCommandAction::SetSliderReset:
                break;
            }
            return payload;
        };

        QList<AuthoringReadRow> filteredRows;
        if (recordingQuery->kind == AuthoringRecordingQuery::Kind::Commands)
        {
            const QVector<ShowCommand> commands = track.commands();
            result.work.scannedSummaryRows = commands.size();
            for (const ShowCommand &command : commands)
            {
                if (!matchesControlFilter(command.controlId))
                    continue;
                if (!withinWindow(command.time))
                    continue;

                AuthoringReadRow row;
                AuthoringReadRow::RecordingCommandRow commandRow;
                commandRow.showFunctionID = show->id();
                commandRow.eventID = command.id;
                if (includeTiming)
                {
                    commandRow.timeMs = command.time;
                    commandRow.order = int(command.order);
                    if (command.pairId != ShowCommand::InvalidId)
                        commandRow.pairID = command.pairId;
                }
                if (includeSemanticPayload)
                    commandRow.action = ShowCommand::actionToString(command.action);
                commandRow.controlRecordingUUID = command.controlId.isNull()
                    ? QString()
                    : command.controlId.toString(QUuid::WithoutBraces).toLower();
                if (includeSemanticPayload)
                {
                    commandRow.role = ShowCommand::roleToString(command.role);
                    commandRow.attribute = command.attribute;
                    commandRow.payload = payloadForCommand(command);
                }
                row.row = commandRow;
                filteredRows.append(row);
            }
        }
        else
        {
            const QVector<ShowControlReference> references = track.referencedControls();
            result.work.scannedSummaryRows = references.size();
            for (const ShowControlReference &reference : references)
            {
                if (!matchesControlFilter(reference.controlId))
                    continue;

                AuthoringReadRow row;
                AuthoringReadRow::RecordingReferenceRow referenceRow;
                referenceRow.showFunctionID = show->id();
                referenceRow.controlRecordingUUID = reference.controlId.isNull()
                    ? QString()
                    : reference.controlId.toString(QUuid::WithoutBraces).toLower();
                referenceRow.role = ShowCommand::roleToString(reference.role);
                referenceRow.attribute = reference.attribute;
                referenceRow.count = reference.count;
                row.row = referenceRow;
                filteredRows.append(row);
            }
        }

        if (offset > filteredRows.size())
        {
            result.code = QStringLiteral("invalidCursor");
            result.error = QStringLiteral("cursor offset is out of range");
            return result;
        }

        const int total = filteredRows.size();
        int nextOffset = offset;
        result.rows.reserve(recordingQuery->limit);
        while (nextOffset < total && int(result.rows.size()) < recordingQuery->limit)
        {
            AuthoringReadRow candidateRow = filteredRows.at(nextOffset);
            if (recordingQuery->kind == AuthoringRecordingQuery::Kind::References && includeReadiness &&
                std::holds_alternative<AuthoringReadRow::RecordingReferenceRow>(candidateRow.row))
            {
                auto &referenceRow = std::get<AuthoringReadRow::RecordingReferenceRow>(candidateRow.row);
                const QUuid controlId = referenceRow.controlRecordingUUID.isEmpty()
                    ? QUuid()
                    : ShowCommand::controlIdFromString(referenceRow.controlRecordingUUID);

                auto statusRank = [](ShowControlStatus status) -> int {
                    switch (status)
                    {
                    case ShowControlStatus::Ready: return 0;
                    case ShowControlStatus::Disabled: return 1;
                    case ShowControlStatus::Unbound: return 2;
                    case ShowControlStatus::Missing: return 3;
                    case ShowControlStatus::Ambiguous: return 4;
                    case ShowControlStatus::Incompatible: return 5;
                    }
                    return 6;
                };

                std::optional<ShowControlAction::Resolution> worst;
                for (const ShowCommand &command : authored)
                {
                    if (command.controlId != controlId)
                        continue;
                    if (ShowCommand::roleToString(command.role) != referenceRow.role)
                        continue;
                    if (command.attribute != referenceRow.attribute)
                        continue;
                    const ShowControlAction::Resolution resolved =
                        ShowControlAction::resolve(m_doc, m_vc, command);
                    if (!worst.has_value() ||
                        statusRank(resolved.status) > statusRank(worst->status))
                        worst = resolved;
                }
                if (worst.has_value())
                {
                    referenceRow.status = statusKey(worst->status);
                    referenceRow.reason = worst->reason;
                }
                else
                {
                    referenceRow.status = statusKey(ShowControlStatus::Missing);
                    referenceRow.reason = QStringLiteral("No authored commands matched reference");
                }
                ++result.work.hydratedRows;
            }

            AuthoringReadResult probe;
            probe.version = result.version;
            probe.basis = result.basis;
            probe.basisScope = result.basisScope;
            probe.complete = (nextOffset + 1) >= total;
            if (!probe.complete)
            {
                CursorState next;
                next.scope = QStringLiteral("recordings");
                next.basis = result.basis;
                next.selectorHash = selectorHash;
                next.instance = instanceId;
                next.offset = nextOffset + 1;
                next.issuedAtMs = nowMs;
                probe.nextCursor = encodeCursor(next);
            }
            probe.rows = result.rows;
            probe.rows.append(candidateRow);
            probe.work = result.work;
            probe.work.serializedRows = probe.rows.size();
            if (stablePayloadBytes(probe) > VCAuthoring::kMaxToolPayloadBytes)
            {
                result.code = QStringLiteral("itemTooLarge");
                result.error = QStringLiteral("A single result row exceeds the payload limit");
                if (!result.rows.isEmpty())
                {
                    result.code.clear();
                    result.error.clear();
                    result.complete = false;
                    CursorState next;
                    next.scope = QStringLiteral("recordings");
                    next.basis = result.basis;
                    next.selectorHash = selectorHash;
                    next.instance = instanceId;
                    next.offset = nextOffset;
                    next.issuedAtMs = nowMs;
                    result.nextCursor = encodeCursor(next);
                    result.work.serializedRows = result.rows.size();
                    stablePayloadBytes(result);
                }
                else
                {
                    result.rows.clear();
                    result.complete = false;
                    result.nextCursor.clear();
                    result.work.serializedRows = 0;
                    result.work.hydratedRows = 0;
                    stablePayloadBytes(result);
                }
                return result;
            }
            result.rows.append(candidateRow);
            ++nextOffset;
        }

        result.complete = nextOffset >= total;
        if (!result.complete)
        {
            CursorState next;
            next.scope = QStringLiteral("recordings");
            next.basis = result.basis;
            next.selectorHash = selectorHash;
            next.instance = instanceId;
            next.offset = nextOffset;
            next.issuedAtMs = nowMs;
            result.nextCursor = encodeCursor(next);
        }
        result.work.serializedRows = result.rows.size();
        if (!includeReadiness)
            result.work.hydratedRows = 0;
        stablePayloadBytes(result);
        return result;
    }

    return unsupportedQuery(QStringLiteral("vcstruct-v1"),
                            authoringBasis(),
                            QStringLiteral("Authoring query family is recognized but not implemented at this checkpoint"));
}

VCBridge::AuthoringPreparationResult VCBridgeV5::prepareAuthoring(const AuthoringRequest &request)
{
    const bool independentRequest =
        std::holds_alternative<AuthoringBindingsRequest>(request) ||
        std::holds_alternative<AuthoringRecordPanelRequest>(request);
    if (!hasMatchingAuthoringContext())
    {
        AuthoringPreparationResult unsupported = unsupportedAuthoringPreparation();
        if (independentRequest)
            unsupported.atomicScope = AuthoringPreparationResult::AtomicScope::IndependentItems;
        return unsupported;
    }

    const qint64 nowMs = authoringNowMs();
    cleanupExpiredRecords(nowMs);

    const QByteArray canonicalRequest = canonicalizeRequest(request);
    if (canonicalRequest.size() > kAuthoringMaxCanonicalRequestBytes)
    {
        AuthoringPreparationResult limit;
        if (independentRequest)
            limit.atomicScope = AuthoringPreparationResult::AtomicScope::IndependentItems;
        limit.code = QStringLiteral("preparationLimit");
        limit.error = QStringLiteral("Canonical request exceeds size limit");
        return limit;
    }

    const int requestItems = this->requestItemCount(request);
    if (requestItems <= 0)
    {
        AuthoringPreparationResult invalid;
        if (independentRequest)
            invalid.atomicScope = AuthoringPreparationResult::AtomicScope::IndependentItems;
        invalid.code = QStringLiteral("invalidArgument");
        invalid.error = QStringLiteral("Request must include at least one item");
        return invalid;
    }
    if (requestItems > kAuthoringMaxItemsPerRequest || this->rootIdentityCount(request) > kAuthoringMaxItemsPerRequest)
    {
        AuthoringPreparationResult limit;
        if (independentRequest)
            limit.atomicScope = AuthoringPreparationResult::AtomicScope::IndependentItems;
        limit.code = QStringLiteral("preparationLimit");
        limit.error = QStringLiteral("Too many request items/root identities");
        return limit;
    }

    auto requestedDiffDetail = [&](const AuthoringRequest &req) -> QString {
        if (const AuthoringLayoutRequest *layout = std::get_if<AuthoringLayoutRequest>(&req))
            return layout->diffDetail;
        if (const AuthoringBindingsRequest *bindings = std::get_if<AuthoringBindingsRequest>(&req))
            return bindings->diffDetail;
        if (const AuthoringRecordPanelRequest *panels = std::get_if<AuthoringRecordPanelRequest>(&req))
            return panels->diffDetail;
        return QStringLiteral("summary");
    };

    const QString diffDetailText = requestedDiffDetail(request).trimmed().toLower();
    if (!(diffDetailText == QStringLiteral("summary") || diffDetailText == QStringLiteral("full")))
    {
        AuthoringPreparationResult invalid;
        if (independentRequest)
            invalid.atomicScope = AuthoringPreparationResult::AtomicScope::IndependentItems;
        invalid.code = QStringLiteral("invalidArgument");
        invalid.error = QStringLiteral("diffDetail must be 'summary' or 'full'");
        return invalid;
    }

    AuthoringPreparationResult result;
    result.basis = authoringBasis();
    result.atomicScope = AuthoringPreparationResult::AtomicScope::NativeGroup;
    if (std::get_if<AuthoringBindingsRequest>(&request) || std::get_if<AuthoringRecordPanelRequest>(&request))
        result.atomicScope = AuthoringPreparationResult::AtomicScope::IndependentItems;
    result.diffComplete = true;
    result.diffDetail = AuthoringPreparationResult::DiffDetail::Summary;
    result.work.payloadBytes = canonicalRequest.size();

    auto rejectEarly = [&](const QString &code, const QString &error) {
        result.code = code;
        result.error = error;
        result.preparationToken.clear();
        result.expiresInMs = 0;
        result.preconditions.clear();
    };
    QVector<QSharedPointer<QLCInputSource>> resolvedBindingSources;

    if (const AuthoringLayoutRequest *layout = std::get_if<AuthoringLayoutRequest>(&request))
    {
        auto requireWidget = [&](quint32 id, const QString &fieldName) -> VCWidget * {
            VCWidget *widget = m_vc ? m_vc->widget(id) : nullptr;
            if (!widget)
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("%1 references an unknown widget ID %2").arg(fieldName).arg(id));
            return widget;
        };
        auto requireFrameOnly = [&](quint32 id, const QString &fieldName) -> VCFrame * {
            VCWidget *widget = requireWidget(id, fieldName);
            if (!widget)
                return nullptr;
            if (qobject_cast<VCPage *>(widget))
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("%1 must reference a frame widget ID %2").arg(fieldName).arg(id));
                return nullptr;
            }
            VCFrame *frame = qobject_cast<VCFrame *>(widget);
            if (!frame)
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("%1 must reference a frame widget ID %2").arg(fieldName).arg(id));
            return frame;
        };
        auto requireFrameOrPage = [&](quint32 id, const QString &fieldName) -> bool {
            VCWidget *widget = requireWidget(id, fieldName);
            if (!widget)
                return false;
            if (qobject_cast<VCFrame *>(widget) || qobject_cast<VCPage *>(widget))
                return true;
            rejectEarly(QStringLiteral("invalidArgument"),
                        QStringLiteral("%1 must reference a frame or page widget ID %2").arg(fieldName).arg(id));
            return false;
        };
        auto requirePage = [&](quint32 id, const QString &fieldName) -> VCPage * {
            VCWidget *widget = requireWidget(id, fieldName);
            if (!widget)
                return nullptr;
            VCPage *page = qobject_cast<VCPage *>(widget);
            if (!page)
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("%1 must reference a page widget ID %2").arg(fieldName).arg(id));
            return page;
        };

        switch (layout->operation)
        {
        case AuthoringLayoutRequest::Operation::Reorder:
        case AuthoringLayoutRequest::Operation::MoveGroup:
        {
            if (!layout->destinationFrameWidgetID.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("destinationFrameWidgetID is required"));
                return result;
            }
            if (layout->widgetIDs.isEmpty())
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("widgetIDs must not be empty"));
                return result;
            }
            if (!layout->destinationSubpage.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("destinationSubpage is required"));
                return result;
            }
            for (quint32 widgetID : layout->widgetIDs)
            {
                if (!requireWidget(widgetID, QStringLiteral("widgetIDs")))
                    return result;
            }
            if (!requireFrameOrPage(*layout->destinationFrameWidgetID, QStringLiteral("destinationFrameWidgetID")))
                return result;
            if (layout->operation == AuthoringLayoutRequest::Operation::MoveGroup)
            {
                if (!layout->anchorWidgetID.has_value())
                {
                    rejectEarly(QStringLiteral("invalidArgument"),
                                QStringLiteral("anchorWidgetID is required"));
                    return result;
                }
                if (!layout->logicalAnchor.has_value())
                {
                    rejectEarly(QStringLiteral("invalidArgument"),
                                QStringLiteral("logicalAnchor is required"));
                    return result;
                }
                if (!requireWidget(*layout->anchorWidgetID, QStringLiteral("anchorWidgetID")))
                    return result;
            }
            if (layout->beforeWidgetID.has_value() &&
                !requireWidget(*layout->beforeWidgetID, QStringLiteral("beforeWidgetID")))
                return result;
            break;
        }
        case AuthoringLayoutRequest::Operation::Align:
        {
            const bool hasFrame = layout->frameWidgetID.has_value();
            const bool hasPage = layout->pageWidgetID.has_value();
            if (!hasFrame && !hasPage)
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("frameWidgetID or pageWidgetID is required"));
                return result;
            }
            if (hasFrame && hasPage)
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("align accepts either frameWidgetID or pageWidgetID, not both"));
                return result;
            }
            if (hasFrame && !requireFrameOnly(*layout->frameWidgetID, QStringLiteral("frameWidgetID")))
                return result;
            if (hasPage && !requirePage(*layout->pageWidgetID, QStringLiteral("pageWidgetID")))
                return result;
            if (!layout->columns.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"), QStringLiteral("columns is required"));
                return result;
            }
            if (!layout->padding.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"), QStringLiteral("padding is required"));
                return result;
            }
            break;
        }
        case AuthoringLayoutRequest::Operation::PageRename:
            if (!layout->name.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"), QStringLiteral("name is required"));
                return result;
            }
            [[fallthrough]];
        case AuthoringLayoutRequest::Operation::PageDelete:
            if (!layout->pageWidgetID.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("pageWidgetID is required"));
                return result;
            }
            if (!requirePage(*layout->pageWidgetID, QStringLiteral("pageWidgetID")))
                return result;
            break;
        case AuthoringLayoutRequest::Operation::FramePages:
            if (!layout->frameWidgetID.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("frameWidgetID is required"));
                return result;
            }
            if (!requireFrameOnly(*layout->frameWidgetID, QStringLiteral("frameWidgetID")))
                return result;
            if (!layout->totalPages.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"), QStringLiteral("totalPages is required"));
                return result;
            }
            if (!layout->pageLabels.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"), QStringLiteral("pageLabels is required"));
                return result;
            }
            if (!layout->pagesLoop.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"), QStringLiteral("pagesLoop is required"));
                return result;
            }
            break;
        case AuthoringLayoutRequest::Operation::PageCreate:
            if (!layout->name.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"), QStringLiteral("name is required"));
                return result;
            }
            break;
        case AuthoringLayoutRequest::Operation::Geometry:
            break;
        }
    }

    if (const AuthoringRecordPanelRequest *panels = std::get_if<AuthoringRecordPanelRequest>(&request))
    {
        for (const AuthoringRecordPanelItem &item : panels->items)
        {
            VCWidget *widget = m_vc ? m_vc->widget(item.widgetID) : nullptr;
            if (!widget)
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("Record panel target widget ID %1 is unknown").arg(item.widgetID));
                return result;
            }
            if (!qobject_cast<VCRecordPanel *>(widget))
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("Record panel target widget ID %1 has an invalid role").arg(item.widgetID));
                return result;
            }
        }
    }

    if (const AuthoringBindingsRequest *bindings = std::get_if<AuthoringBindingsRequest>(&request))
    {
        resolvedBindingSources = QVector<QSharedPointer<QLCInputSource>>(bindings->items.size());
        auto sourceMatchesFor = [](VCWidget *widget, const AuthoringSourceKey &key) {
            QList<QSharedPointer<QLCInputSource>> matches;
            const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
            for (const QSharedPointer<QLCInputSource> &source : sources)
            {
                if (source.isNull())
                    continue;
                if (source->id() != *key.controlSlotID)
                    continue;
                if (source->universe() != *key.inputUniverseID)
                    continue;
                if ((source->channel() & 0x0000FFFF) != *key.inputChannel)
                    continue;
                if (int(source->page()) != *key.childSubpage)
                    continue;
                matches.append(source);
            }
            return matches;
        };
        auto hasDuplicateNativePair = [](VCWidget *widget, const QSharedPointer<QLCInputSource> &target) {
            if (target.isNull())
                return false;
            int pairCount = 0;
            const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
            for (const QSharedPointer<QLCInputSource> &source : sources)
            {
                if (source.isNull())
                    continue;
                if (source->universe() == target->universe() && source->channel() == target->channel())
                    ++pairCount;
            }
            return pairCount > 1;
        };

        for (int itemIndex = 0; itemIndex < bindings->items.size(); ++itemIndex)
        {
            const AuthoringBindingItem &item = bindings->items.at(itemIndex);
            VCWidget *widget = m_vc ? m_vc->widget(item.widgetID) : nullptr;
            if (!widget)
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("Binding target widget ID %1 is unknown").arg(item.widgetID));
                return result;
            }

            if ((item.kind == AuthoringBindingItem::Kind::Function) &&
                !(item.action == AuthoringBindingItem::Action::Set ||
                  item.action == AuthoringBindingItem::Action::Clear))
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("Function action is not supported"));
                return result;
            }
            if ((item.kind == AuthoringBindingItem::Kind::Controller) &&
                !(item.action == AuthoringBindingItem::Action::Append ||
                  item.action == AuthoringBindingItem::Action::Move ||
                  item.action == AuthoringBindingItem::Action::Remove))
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("Controller action is not supported"));
                return result;
            }
            if ((item.kind == AuthoringBindingItem::Kind::Key) &&
                !(item.action == AuthoringBindingItem::Action::Set ||
                  item.action == AuthoringBindingItem::Action::Remove))
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("Key action is not supported"));
                return result;
            }
            if ((item.kind == AuthoringBindingItem::Kind::Feedback) &&
                item.action != AuthoringBindingItem::Action::Set)
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("Feedback action is not supported"));
                return result;
            }

            if ((item.kind == AuthoringBindingItem::Kind::Controller ||
                 item.kind == AuthoringBindingItem::Kind::Key) &&
                !item.controlSlotID.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("controlSlotID is required"));
                return result;
            }

            if (item.kind == AuthoringBindingItem::Kind::Controller &&
                (item.action == AuthoringBindingItem::Action::Append ||
                 item.action == AuthoringBindingItem::Action::Move))
            {
                if (!item.destination.has_value() ||
                    !item.destination->inputUniverseID.has_value() ||
                    !item.destination->inputChannel.has_value() ||
                    !item.destination->profileName.has_value())
                {
                    rejectEarly(QStringLiteral("invalidArgument"),
                                QStringLiteral("Controller destination requires inputUniverseID, inputChannel and profileName"));
                    return result;
                }
            }

            if (item.kind == AuthoringBindingItem::Kind::Function)
            {
                if (!qobject_cast<VCButton *>(widget))
                {
                    rejectEarly(QStringLiteral("invalidArgument"),
                                QStringLiteral("Function binding requires a VCButton target"));
                    return result;
                }
            }
            if (item.kind == AuthoringBindingItem::Kind::Function &&
                item.action == AuthoringBindingItem::Action::Set)
            {
                if (!item.functionID.has_value() || *item.functionID == Function::invalidId())
                {
                    rejectEarly(QStringLiteral("invalidArgument"),
                                QStringLiteral("Function set requires a valid functionID"));
                    return result;
                }
                if (!m_doc || !m_doc->function(*item.functionID))
                {
                    rejectEarly(QStringLiteral("invalidArgument"),
                                QStringLiteral("Function set references an unknown function ID %1")
                                    .arg(*item.functionID));
                    return result;
                }
            }

            if (item.kind == AuthoringBindingItem::Kind::Key &&
                item.action == AuthoringBindingItem::Action::Set &&
                (!item.keySequence.has_value() || item.keySequence->trimmed().isEmpty()))
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("Key set requires keySequence"));
                return result;
            }
            if (item.kind == AuthoringBindingItem::Kind::Key &&
                item.action == AuthoringBindingItem::Action::Remove &&
                (!item.keySequence.has_value() || item.keySequence->trimmed().isEmpty()))
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("Key remove requires keySequence"));
                return result;
            }

            if (item.kind == AuthoringBindingItem::Kind::Feedback)
            {
                const bool hasIdleValue = item.idleValue.has_value();
                const bool hasActiveValue = item.activeValue.has_value();
                const bool hasMonitorValue = item.monitorValue.has_value();
                const bool hasIdleChannel = item.idleChannel.has_value();
                const bool hasActiveChannel = item.activeChannel.has_value();
                const bool hasMonitorChannel = item.monitorChannel.has_value();
                const int valueCount = int(hasIdleValue) + int(hasActiveValue) + int(hasMonitorValue);
                const int channelCount = int(hasIdleChannel) + int(hasActiveChannel) + int(hasMonitorChannel);
                const bool valuesComplete = (valueCount == 3);
                const bool channelsComplete = (channelCount == 3);
                if (!valuesComplete && valueCount > 0)
                {
                    rejectEarly(QStringLiteral("invalidArgument"),
                                QStringLiteral("Feedback values group must include idleValue, activeValue and monitorValue"));
                    return result;
                }
                if (!channelsComplete && channelCount > 0)
                {
                    rejectEarly(QStringLiteral("invalidArgument"),
                                QStringLiteral("Feedback channels group must include idleChannel, activeChannel and monitorChannel"));
                    return result;
                }
                if (!valuesComplete && !channelsComplete)
                {
                    rejectEarly(QStringLiteral("invalidArgument"),
                                QStringLiteral("Feedback requires at least one complete values or channels group"));
                    return result;
                }
                if (valuesComplete)
                {
                    if (*item.idleValue < 0 || *item.idleValue > 255 ||
                        *item.activeValue < 0 || *item.activeValue > 255 ||
                        *item.monitorValue < 0 || *item.monitorValue > 255)
                    {
                        rejectEarly(QStringLiteral("invalidArgument"),
                                    QStringLiteral("Feedback values must be in range 0..255"));
                        return result;
                    }
                }
                if (channelsComplete)
                {
                    if (*item.idleChannel < -1 || *item.idleChannel > 15 ||
                        *item.activeChannel < -1 || *item.activeChannel > 15 ||
                        *item.monitorChannel < -1 || *item.monitorChannel > 15)
                    {
                        rejectEarly(QStringLiteral("invalidArgument"),
                                    QStringLiteral("Feedback channels must be in range -1..15"));
                        return result;
                    }
                }
            }

            const bool requiresSourceSelection =
                ((item.kind == AuthoringBindingItem::Kind::Controller) &&
                 (item.action == AuthoringBindingItem::Action::Move ||
                  item.action == AuthoringBindingItem::Action::Remove)) ||
                (item.kind == AuthoringBindingItem::Kind::Feedback);
            if (!requiresSourceSelection)
                continue;

            if (!item.sourceKey.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("Source key is required"));
                return result;
            }
            if (!item.sourceKey->controlSlotID.has_value() ||
                !item.sourceKey->inputUniverseID.has_value() ||
                !item.sourceKey->inputChannel.has_value() ||
                !item.sourceKey->childSubpage.has_value())
            {
                rejectEarly(QStringLiteral("invalidArgument"),
                            QStringLiteral("Source key requires controlSlotID, inputUniverseID, inputChannel and childSubpage"));
                return result;
            }

            const QList<QSharedPointer<QLCInputSource>> matches = sourceMatchesFor(widget, *item.sourceKey);
            if (matches.size() != 1)
            {
                rejectEarly(QStringLiteral("ambiguousSource"),
                            QStringLiteral("Source key is ambiguous or unresolved"));
                return result;
            }
            if (hasDuplicateNativePair(widget, matches.first()))
            {
                rejectEarly(QStringLiteral("ambiguousSource"),
                            QStringLiteral("Source key maps to duplicate native universe/channel pair"));
                return result;
            }
            resolvedBindingSources[itemIndex] = matches.first();
        }
    }

    const PredicateSelection predicates = predicateSelectionForRequest(request);
    const AuthoringBindingsRequest *bindingsRequest = std::get_if<AuthoringBindingsRequest>(&request);
    const AuthoringLayoutRequest *layoutRequestForPredicates = std::get_if<AuthoringLayoutRequest>(&request);
    QMap<quint32, QRectF> preconditionTargets;
    for (quint32 id : this->widgetIdsFromRequest(request))
        preconditionTargets.insert(id, QRectF());

    QSet<quint32> writerSafeIds;
    if (layoutRequestForPredicates &&
        (layoutRequestForPredicates->operation == AuthoringLayoutRequest::Operation::Reorder ||
         layoutRequestForPredicates->operation == AuthoringLayoutRequest::Operation::MoveGroup) &&
        predicates.requireWriterSafe && m_vc)
    {
        auto addWithDescendants = [&](VCWidget *root) {
            if (!root)
                return;
            writerSafeIds.insert(root->id());
            if (VCFrame *frame = qobject_cast<VCFrame *>(root))
                for (VCWidget *child : frame->children(true))
                    if (child)
                        writerSafeIds.insert(child->id());
        };

        bool hasMovedSubmaster = false;
        QSet<VCFrame *> sourceFrames;
        for (quint32 movedId : layoutRequestForPredicates->widgetIDs)
        {
            VCWidget *moved = m_vc->widget(movedId);
            if (!moved)
                continue;
            addWithDescendants(moved);
            if (VCFrame *parentFrame = qobject_cast<VCFrame *>(moved->parent()))
                sourceFrames.insert(parentFrame);
            if (VCSlider *slider = qobject_cast<VCSlider *>(moved))
                hasMovedSubmaster = hasMovedSubmaster || slider->sliderMode() == VCSlider::Submaster;
        }

        if (hasMovedSubmaster)
        {
            QSet<VCFrame *> affectedFrames = sourceFrames;
            if (layoutRequestForPredicates->destinationFrameWidgetID.has_value())
                if (VCFrame *destinationFrame =
                        qobject_cast<VCFrame *>(m_vc->widget(*layoutRequestForPredicates->destinationFrameWidgetID)))
                    affectedFrames.insert(destinationFrame);
            for (VCFrame *frame : affectedFrames)
                for (VCWidget *child : frame->children(false))
                    if (child)
                        addWithDescendants(child);
        }
    }
    QString preconditionCode;
    PredicateSelection commonPredicates = predicates;
    commonPredicates.requireWriterSafe = false;
    const QString preconditionError = authoringPreconditionError(
        preconditionTargets, &preconditionCode, &result.preconditions, commonPredicates);
    if (!preconditionError.isEmpty())
    {
        result.code = preconditionCode;
        result.error = preconditionError;
        result.preparationToken.clear();
        result.expiresInMs = 0;
        return result;
    }
    auto addPreparationObservation = [&](const QString &name, bool met, const QString &scope, const QString &reason) {
        AuthoringPreconditionObservation obs;
        obs.name = name;
        obs.met = met;
        obs.scope = scope;
        obs.reason = reason;
        result.preconditions.append(obs);
    };
    if (predicates.requireWriterSafe)
    {
        QVector<VCWidget *> writerSafeTargets;
        QList<quint32> sortedIds = writerSafeIds.values();
        std::sort(sortedIds.begin(), sortedIds.end());
        for (quint32 id : sortedIds)
        {
            VCWidget *widget = m_vc ? m_vc->widget(id) : nullptr;
            if (widget)
                writerSafeTargets.append(widget);
        }

        for (VCWidget *widget : writerSafeTargets)
        {
            if (VCButton *button = qobject_cast<VCButton *>(widget))
            {
                if (button->state() == VCButton::Active)
                {
                    result.code = QStringLiteral("unsupportedPreparation");
                    result.error = QStringLiteral("Active button writer effects are not certifiable");
                    addPreparationObservation(QStringLiteral("writerSafe"), false,
                                              QStringLiteral("widget:%1").arg(widget->id()),
                                              result.error);
                    result.preparationToken.clear();
                    result.expiresInMs = 0;
                    return result;
                }
            }
            if (VCSlider *slider = qobject_cast<VCSlider *>(widget))
            {
                if (slider->sliderMode() == VCSlider::Level || slider->sliderMode() == VCSlider::Adjust)
                {
                    result.code = QStringLiteral("unsupportedPreparation");
                    result.error = QStringLiteral("Slider writer effects are not certifiable");
                    addPreparationObservation(QStringLiteral("writerSafe"), false,
                                              QStringLiteral("widget:%1").arg(widget->id()),
                                              result.error);
                    result.preparationToken.clear();
                    result.expiresInMs = 0;
                    return result;
                }
            }
            if (qobject_cast<VCAnimation *>(widget) || qobject_cast<VCAudioTriggers *>(widget) || qobject_cast<VCXYPad *>(widget))
            {
                result.code = QStringLiteral("unsupportedPreparation");
                result.error = QStringLiteral("Runtime writer effects are not certifiable");
                addPreparationObservation(QStringLiteral("writerSafe"), false,
                                          QStringLiteral("widget:%1").arg(widget->id()),
                                          result.error);
                result.preparationToken.clear();
                result.expiresInMs = 0;
                return result;
            }
        }
        addPreparationObservation(QStringLiteral("writerSafe"), true, QStringLiteral("closure"), QString());
    }

    if (bindingsRequest)
    {
        auto addObs = [&](const QString &name, bool met, const QString &scope, const QString &reason) {
            AuthoringPreconditionObservation obs;
            obs.name = name;
            obs.met = met;
            obs.scope = scope;
            obs.reason = reason;
            result.preconditions.append(obs);
        };
        auto reject = [&](const QString &name, const QString &scope, const QString &reason) {
            result.code = QStringLiteral("unsupportedPreparation");
            result.error = reason;
            addObs(name, false, scope, reason);
            result.preparationToken.clear();
            result.expiresInMs = 0;
        };
        InputOutputMap *ioMap = m_doc ? m_doc->inputOutputMap() : nullptr;
        for (int itemIndex = 0; itemIndex < bindingsRequest->items.size(); ++itemIndex)
        {
            const AuthoringBindingItem &item = bindingsRequest->items.at(itemIndex);
            VCWidget *widget = m_vc ? m_vc->widget(item.widgetID) : nullptr;
            if (!widget)
            {
                reject(QStringLiteral("bindingTarget"), QStringLiteral("widget:%1").arg(item.widgetID),
                       QStringLiteral("Target widget is unavailable"));
                return result;
            }

            if (item.kind == AuthoringBindingItem::Kind::Controller &&
                (item.action == AuthoringBindingItem::Action::Append ||
                 item.action == AuthoringBindingItem::Action::Move))
            {
                if (!ioMap || !item.destination.has_value() ||
                    !item.destination->inputUniverseID.has_value() ||
                    !item.destination->inputChannel.has_value() ||
                    !item.destination->profileName.has_value())
                {
                    reject(QStringLiteral("profileRoutingReady"),
                           QStringLiteral("widget:%1").arg(item.widgetID),
                           QStringLiteral("Destination input patch/profile is not ready"));
                    return result;
                }

                const quint32 universe = *item.destination->inputUniverseID;
                const quint32 channel = *item.destination->inputChannel;
                const QString expectedProfile = *item.destination->profileName;
                InputPatch *patch = ioMap->inputPatch(universe);
                if (!patch || !patch->isPatched() || patch->profileName() != expectedProfile)
                {
                    reject(QStringLiteral("profileRoutingReady"),
                           QStringLiteral("widget:%1 universe:%2").arg(item.widgetID).arg(universe),
                           QStringLiteral("Destination input patch/profile is not ready"));
                    return result;
                }
                const QStringList loadedProfiles = ioMap->profileNames();
                if (!loadedProfiles.contains(expectedProfile))
                {
                    reject(QStringLiteral("profileRoutingReady"),
                           QStringLiteral("widget:%1 universe:%2").arg(item.widgetID).arg(universe),
                           QStringLiteral("Destination profile is not loaded"));
                    return result;
                }
                QLCInputProfile *profile = ioMap->profile(expectedProfile);
                if (!profile)
                {
                    reject(QStringLiteral("profileRoutingReady"),
                           QStringLiteral("widget:%1 universe:%2").arg(item.widgetID).arg(universe),
                           QStringLiteral("Destination profile is not loaded"));
                    return result;
                }
                QLCInputProfile *attachedProfile = patch->profile();
                if (!attachedProfile || attachedProfile != profile)
                {
                    reject(QStringLiteral("profileRoutingReady"),
                           QStringLiteral("widget:%1 universe:%2").arg(item.widgetID).arg(universe),
                           QStringLiteral("Destination patch/profile identity mismatch"));
                    return result;
                }
                QLCInputChannel *profileChannel = attachedProfile->channel(channel);
                if (!profileChannel)
                {
                    reject(QStringLiteral("profileRoutingReady"),
                           QStringLiteral("widget:%1 universe:%2").arg(item.widgetID).arg(universe),
                           QStringLiteral("Destination profile channel is unavailable"));
                    return result;
                }
                if (profileChannel->movementType() != QLCInputChannel::Absolute)
                {
                    reject(QStringLiteral("profileRoutingReady"),
                           QStringLiteral("widget:%1 universe:%2").arg(item.widgetID).arg(universe),
                           QStringLiteral("Relative destination profile channels are not supported"));
                    return result;
                }
                addObs(QStringLiteral("profileRoutingReady"), true,
                       QStringLiteral("widget:%1 universe:%2").arg(item.widgetID).arg(universe), QString());
            }

            const bool requiresSourceSelection =
                ((item.kind == AuthoringBindingItem::Kind::Controller) &&
                 (item.action == AuthoringBindingItem::Action::Move ||
                  item.action == AuthoringBindingItem::Action::Remove)) ||
                (item.kind == AuthoringBindingItem::Kind::Feedback);
            const QSharedPointer<QLCInputSource> selectedSource =
                (itemIndex < resolvedBindingSources.size()) ? resolvedBindingSources.at(itemIndex)
                                                            : QSharedPointer<QLCInputSource>();
            if (requiresSourceSelection && selectedSource.isNull())
            {
                reject(QStringLiteral("sourceSelection"),
                       QStringLiteral("widget:%1").arg(item.widgetID),
                       QStringLiteral("Source selection is unavailable"));
                return result;
            }

            if (item.kind == AuthoringBindingItem::Kind::Controller &&
                (item.action == AuthoringBindingItem::Action::Move ||
                 item.action == AuthoringBindingItem::Action::Remove))
            {
                if (selectedSource && selectedSource->workingMode() == QLCInputSource::Relative)
                {
                    reject(QStringLiteral("relativeSources"),
                           QStringLiteral("widget:%1 source:%2/%3").arg(item.widgetID)
                               .arg(selectedSource->universe())
                               .arg(selectedSource->channel()),
                           QStringLiteral("Relative input sources are not supported for this controller action"));
                    return result;
                }
                addObs(QStringLiteral("relativeSources"), true,
                       QStringLiteral("widget:%1").arg(item.widgetID), QString());
            }

            if (item.kind == AuthoringBindingItem::Kind::Function)
            {
                const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
                for (const QSharedPointer<QLCInputSource> &source : sources)
                {
                    if (source.isNull())
                        continue;
                    if (ioMap)
                    {
                        OutputPatch *patch = ioMap->feedbackPatch(source->universe());
                        if (patch && patch->isPatched())
                        {
                            result.code = QStringLiteral("authoringPrecondition");
                            result.error = QStringLiteral("Feedback patch must be disabled for affected input sources");
                            addObs(QStringLiteral("feedbackPatchIdle"), false,
                                   QStringLiteral("widget:%1 universe:%2").arg(item.widgetID).arg(source->universe()),
                                   result.error);
                            result.preparationToken.clear();
                            result.expiresInMs = 0;
                            return result;
                        }
                    }
                }
                addObs(QStringLiteral("feedbackPatchIdle"), true,
                       QStringLiteral("widget:%1").arg(item.widgetID), QString());
            }

            if (item.kind == AuthoringBindingItem::Kind::Feedback)
            {
                if (!selectedSource)
                {
                    reject(QStringLiteral("sourceSelection"),
                           QStringLiteral("widget:%1").arg(item.widgetID),
                           QStringLiteral("Source key is ambiguous or unresolved"));
                    return result;
                }
                QList<QSharedPointer<QLCInputSource>> affectedSources;
                auto appendWidgetSources = [&](VCWidget *targetWidget) {
                    if (!targetWidget)
                        return;
                    const QList<QSharedPointer<QLCInputSource>> widgetSources = targetWidget->inputSources();
                    for (const QSharedPointer<QLCInputSource> &source : widgetSources)
                        if (!source.isNull())
                            affectedSources.append(source);
                };
                std::function<void(VCWidget *)> collectFeedbackClosure = [&](VCWidget *targetWidget) {
                    if (!targetWidget)
                        return;
                    appendWidgetSources(targetWidget);
                    VCFrame *frameWidget = qobject_cast<VCFrame *>(targetWidget);
                    if (!frameWidget)
                        return;
                    const QList<VCWidget *> descendants = frameWidget->findChildren<VCWidget *>();
                    for (VCWidget *child : descendants)
                    {
                        if (!child || child->parent() != frameWidget)
                            continue;
                        if (child->page() != frameWidget->currentPage())
                            continue;
                        collectFeedbackClosure(child);
                    }
                };
                collectFeedbackClosure(widget);

                if (ioMap)
                {
                    for (const QSharedPointer<QLCInputSource> &source : affectedSources)
                    {
                        OutputPatch *patch = ioMap->feedbackPatch(source->universe());
                        if (patch && patch->isPatched())
                        {
                            result.code = QStringLiteral("authoringPrecondition");
                            result.error = QStringLiteral("Feedback patch must be disabled for affected input sources");
                            addObs(QStringLiteral("feedbackPatchIdle"), false,
                                   QStringLiteral("widget:%1 universe:%2").arg(item.widgetID).arg(source->universe()),
                                   result.error);
                            result.preparationToken.clear();
                            result.expiresInMs = 0;
                            return result;
                        }
                    }
                }
                addObs(QStringLiteral("feedbackPatchIdle"), true, QStringLiteral("widget:%1").arg(item.widgetID), QString());
                for (const QSharedPointer<QLCInputSource> &source : affectedSources)
                {
                    if (source->needsUpdate())
                    {
                        reject(QStringLiteral("feedbackNeedsUpdateIdle"),
                               QStringLiteral("widget:%1 source:%2/%3").arg(item.widgetID)
                                   .arg(source->universe()).arg(source->channel()),
                               QStringLiteral("Feedback source update is pending"));
                        return result;
                    }
                }
                addObs(QStringLiteral("feedbackNeedsUpdateIdle"), true, QStringLiteral("widget:%1").arg(item.widgetID), QString());
            }
        }
    }

    if (const AuthoringBindingsRequest *bindingsRequest = std::get_if<AuthoringBindingsRequest>(&request))
    {
        InputOutputMap *ioMap = m_doc ? m_doc->inputOutputMap() : nullptr;
        result.atomicScope = AuthoringPreparationResult::AtomicScope::IndependentItems;
        result.diffDetail = (diffDetailText == QStringLiteral("full"))
            ? AuthoringPreparationResult::DiffDetail::Full
            : AuthoringPreparationResult::DiffDetail::Summary;
        result.diffComplete = (result.diffDetail == AuthoringPreparationResult::DiffDetail::Full);
        result.items.reserve(bindingsRequest->items.size());
        QHash<QString, QSet<quint32>> plannedFanoutOwners;

        if (!bindingsRequest->expectedBasis.isEmpty() && bindingsRequest->expectedBasis != result.basis)
        {
            result.code = QStringLiteral("staleBasis");
            result.error = QStringLiteral("Authoring basis mismatch");
            return result;
        }

        if (bindingsRequest->requireAtomic)
        {
            result.code = QStringLiteral("unsupportedPreparation");
            result.error = QStringLiteral("Binding updates apply as independent items");
            result.limitations.append({QStringLiteral("unsupportedConfiguration"),
                                       QStringLiteral("request"),
                                       QStringLiteral("requireAtomic is not supported for binding updates")});
            return result;
        }

        if (m_authoringRecords.size() >= kAuthoringMaxLiveRecords)
        {
            AuthoringPreparationResult capacity;
            capacity.atomicScope = AuthoringPreparationResult::AtomicScope::IndependentItems;
            capacity.code = QStringLiteral("preparationCapacity");
            capacity.error = QStringLiteral("No free preparation record slots");
            return capacity;
        }

        bool hasErrors = false;
        for (int i = 0; i < bindingsRequest->items.size(); ++i)
        {
            const AuthoringBindingItem &binding = bindingsRequest->items.at(i);
            AuthoringPreparationItem item;
            item.index = i;
            item.targetWidgetID = binding.widgetID;
            item.status = AuthoringPreparationItem::Status::Ok;
            item.outcome = AuthoringPreparationItem::Outcome::Prepared;

            VCWidget *widget = m_vc ? m_vc->widget(binding.widgetID) : nullptr;
            if (!widget)
            {
                item.status = AuthoringPreparationItem::Status::Error;
                item.outcome = AuthoringPreparationItem::Outcome::Refused;
                item.code = QStringLiteral("invalidArgument");
                item.error = QStringLiteral("Binding target widget ID %1 is unknown").arg(binding.widgetID);
                hasErrors = true;
                result.items.append(item);
                continue;
            }

            populateSelectionDetails(item, widget);
            item.beforeLogicalRect = widget->logicalGeometry();
            item.afterLogicalRect = item.beforeLogicalRect;
            item.changedCount = 0;

            auto refuse = [&](const QString &code, const QString &error, const QString &limitationCode = QString()) {
                item.status = AuthoringPreparationItem::Status::Error;
                item.outcome = AuthoringPreparationItem::Outcome::Refused;
                item.code = code;
                item.error = error;
                if (!limitationCode.isEmpty())
                    item.limitations.append({limitationCode,
                                             QStringLiteral("widget:%1").arg(binding.widgetID),
                                             error});
                hasErrors = true;
            };

            if (binding.kind == AuthoringBindingItem::Kind::Feedback)
            {
                const QSharedPointer<QLCInputSource> selectedSource =
                    (i < resolvedBindingSources.size())
                        ? resolvedBindingSources.at(i)
                        : QSharedPointer<QLCInputSource>();
                if (selectedSource.isNull())
                {
                    refuse(QStringLiteral("ambiguousSource"),
                           QStringLiteral("Source key is ambiguous or unresolved"));
                    result.items.append(item);
                    continue;
                }

                item.selectedSourceKey = sourceKeyFromSource(selectedSource.data());
                auto addFeedbackChange = [&](const QString &field, const QVariant &before, const QVariant &after) {
                    if (before == after)
                        return;
                    AuthoringFieldChange change;
                    change.field = field;
                    change.before = before;
                    change.after = after;
                    change.changed = true;
                    change.note = QStringLiteral("prepared");
                    item.changes.append(change);
                };

                if (binding.idleValue.has_value())
                {
                    const int before = int(selectedSource->feedbackValue(QLCInputFeedback::LowerValue));
                    addFeedbackChange(QStringLiteral("idleValue"), before, *binding.idleValue);
                }
                if (binding.activeValue.has_value())
                {
                    const int before = int(selectedSource->feedbackValue(QLCInputFeedback::UpperValue));
                    addFeedbackChange(QStringLiteral("activeValue"), before, *binding.activeValue);
                }
                if (binding.monitorValue.has_value())
                {
                    const int before = int(selectedSource->feedbackValue(QLCInputFeedback::MonitorValue));
                    addFeedbackChange(QStringLiteral("monitorValue"), before, *binding.monitorValue);
                }
                if (binding.idleChannel.has_value())
                {
                    const int before = selectedSource->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
                    addFeedbackChange(QStringLiteral("idleChannel"), before, *binding.idleChannel);
                }
                if (binding.activeChannel.has_value())
                {
                    const int before = selectedSource->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
                    addFeedbackChange(QStringLiteral("activeChannel"), before, *binding.activeChannel);
                }
                if (binding.monitorChannel.has_value())
                {
                    const int before = selectedSource->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
                    addFeedbackChange(QStringLiteral("monitorChannel"), before, *binding.monitorChannel);
                }

                item.changedCount = int(item.changes.size());
                if (item.changedCount == 0)
                    item.outcome = AuthoringPreparationItem::Outcome::Unchanged;
                result.items.append(item);
                continue;
            }

            if (binding.kind == AuthoringBindingItem::Kind::Function)
            {
                VCButton *button = qobject_cast<VCButton *>(widget);
                if (!button)
                {
                    refuse(QStringLiteral("invalidArgument"),
                           QStringLiteral("Function binding requires a VCButton target"));
                    result.items.append(item);
                    continue;
                }
                const quint32 beforeFunction = button->functionID();
                const quint32 afterFunction =
                    (binding.action == AuthoringBindingItem::Action::Clear)
                        ? quint32(Function::invalidId())
                        : binding.functionID.value_or(quint32(Function::invalidId()));
                item.selectedFunctionBeforeID = beforeFunction;
                item.selectedFunctionAfterID = afterFunction;
                item.selectedFunctionID = afterFunction;
                const QString beforeCaption = button->caption();
                QString afterCaption = beforeCaption;
                Function *targetFunction = m_doc ? m_doc->function(afterFunction) : nullptr;
                if (targetFunction &&
                    ((button->isEditing() && beforeCaption.isEmpty()) ||
                     beforeCaption == button->defaultCaption()))
                {
                    afterCaption = targetFunction->name();
                }
                if (beforeFunction != afterFunction)
                {
                    AuthoringFieldChange change;
                    change.field = QStringLiteral("functionID");
                    change.before = qulonglong(beforeFunction);
                    change.after = qulonglong(afterFunction);
                    change.changed = true;
                    change.note = QStringLiteral("prepared");
                    item.changes.append(change);
                    if (beforeCaption != afterCaption)
                    {
                        AuthoringFieldChange captionChange;
                        captionChange.field = QStringLiteral("caption");
                        captionChange.before = beforeCaption;
                        captionChange.after = afterCaption;
                        captionChange.changed = true;
                        captionChange.note = QStringLiteral("prepared");
                        item.changes.append(captionChange);
                    }
                    item.changedCount = int(item.changes.size());
                }
                else
                {
                    item.outcome = AuthoringPreparationItem::Outcome::Unchanged;
                }
                result.items.append(item);
                continue;
            }

            if (binding.kind == AuthoringBindingItem::Kind::Controller)
            {
                if (!binding.controlSlotID.has_value())
                {
                    refuse(QStringLiteral("invalidArgument"), QStringLiteral("controlSlotID is required"));
                    result.items.append(item);
                    continue;
                }
                const quint8 slot = *binding.controlSlotID;
                item.selectedControlSlotID = slot;
                if (!hasExternalControlSlot(widget, slot))
                {
                    refuse(QStringLiteral("invalidArgument"),
                           QStringLiteral("controlSlotID %1 is not registered for widget %2")
                               .arg(slot).arg(binding.widgetID));
                    result.items.append(item);
                    continue;
                }

                const bool requiresSourceSelection =
                    (binding.action == AuthoringBindingItem::Action::Move ||
                     binding.action == AuthoringBindingItem::Action::Remove);
                const QSharedPointer<QLCInputSource> selectedSource =
                    (requiresSourceSelection && i < resolvedBindingSources.size())
                        ? resolvedBindingSources.at(i)
                        : QSharedPointer<QLCInputSource>();
                if (requiresSourceSelection && selectedSource.isNull())
                {
                    refuse(QStringLiteral("ambiguousSource"),
                           QStringLiteral("Source key is ambiguous or unresolved"));
                    result.items.append(item);
                    continue;
                }

                QVariant beforeValue = QVariant(QString());
                QVariant afterValue = QVariant(QString());
                bool willChange = false;

                if (binding.action == AuthoringBindingItem::Action::Append)
                {
                    if (!binding.destination.has_value() ||
                        !binding.destination->inputUniverseID.has_value() ||
                        !binding.destination->inputChannel.has_value())
                    {
                        refuse(QStringLiteral("invalidArgument"),
                               QStringLiteral("Controller destination requires inputUniverseID and inputChannel"));
                        result.items.append(item);
                        continue;
                    }
                    const quint32 destUniverse = *binding.destination->inputUniverseID;
                    const quint32 destChannel = *binding.destination->inputChannel;
                    const int destSubpage = widget->page();
                    bool exists = false;
                    const QList<QSharedPointer<QLCInputSource>> currentSources = widget->inputSources();
                    for (const QSharedPointer<QLCInputSource> &candidate : currentSources)
                    {
                        if (candidate.isNull())
                            continue;
                        if (candidate->id() != slot)
                            continue;
                        if (candidate->universe() != destUniverse)
                            continue;
                        if ((candidate->channel() & 0x0000FFFF) != destChannel)
                            continue;
                        if (int(candidate->page()) != destSubpage)
                            continue;
                        exists = true;
                        break;
                    }
                    if (!exists)
                    {
                        if (!bindingsRequest->allowFanout)
                        {
                            if (hasCrossWidgetPhysicalConflict(m_vc, widget, destUniverse, destChannel))
                            {
                                refuse(QStringLiteral("ambiguousSource"),
                                       QStringLiteral("Destination source would fan out across widgets; set allowFanout=true to permit reuse"));
                                result.items.append(item);
                                continue;
                            }
                            const QString destKey = fanoutPhysicalKey(destUniverse, destChannel);
                            const QSet<quint32> scheduled = plannedFanoutOwners.value(destKey);
                            bool conflictsWithPlanned = false;
                            for (quint32 ownerId : scheduled)
                            {
                                if (ownerId != binding.widgetID)
                                {
                                    conflictsWithPlanned = true;
                                    break;
                                }
                            }
                            if (conflictsWithPlanned)
                            {
                                refuse(QStringLiteral("ambiguousSource"),
                                       QStringLiteral("Destination source would fan out across widgets; set allowFanout=true to permit reuse"));
                                result.items.append(item);
                                continue;
                            }
                            plannedFanoutOwners[destKey].insert(binding.widgetID);
                        }
                        willChange = true;
                        AuthoringSourceKey afterKey;
                        afterKey.controlSlotID = slot;
                        afterKey.inputUniverseID = destUniverse;
                        afterKey.inputChannel = destChannel;
                        afterKey.childSubpage = destSubpage;
                        afterValue = sourceKeyVariant(afterKey);
                    }
                }
                else if (binding.action == AuthoringBindingItem::Action::Move)
                {
                    if (!binding.destination.has_value() ||
                        !binding.destination->inputUniverseID.has_value() ||
                        !binding.destination->inputChannel.has_value())
                    {
                        refuse(QStringLiteral("invalidArgument"),
                               QStringLiteral("Controller destination requires inputUniverseID and inputChannel"));
                        result.items.append(item);
                        continue;
                    }
                    if (!selectedSource.isNull())
                    {
                        const AuthoringSourceKey selectedKey = sourceKeyFromSource(selectedSource.data());
                        item.selectedSourceKey = selectedKey;
                        beforeValue = sourceKeyVariant(selectedKey);
                    }
                    const quint32 destUniverse = *binding.destination->inputUniverseID;
                    const quint32 destChannel = *binding.destination->inputChannel;
                    const int destSubpage = !selectedSource.isNull() ? int(selectedSource->page()) : widget->page();
                    const bool sameDestination =
                        !selectedSource.isNull() &&
                        selectedSource->id() == slot &&
                        selectedSource->universe() == destUniverse &&
                        (selectedSource->channel() & 0x0000FFFF) == destChannel &&
                        int(selectedSource->page()) == destSubpage;
                    if (!sameDestination)
                    {
                        if (!bindingsRequest->allowFanout)
                        {
                            if (hasCrossWidgetPhysicalConflict(m_vc, widget, destUniverse, destChannel, selectedSource.data()))
                            {
                                refuse(QStringLiteral("ambiguousSource"),
                                       QStringLiteral("Destination source would fan out across widgets; set allowFanout=true to permit reuse"));
                                result.items.append(item);
                                continue;
                            }
                            const QString destKey = fanoutPhysicalKey(destUniverse, destChannel);
                            const QSet<quint32> scheduled = plannedFanoutOwners.value(destKey);
                            bool conflictsWithPlanned = false;
                            for (quint32 ownerId : scheduled)
                            {
                                if (ownerId != binding.widgetID)
                                {
                                    conflictsWithPlanned = true;
                                    break;
                                }
                            }
                            if (conflictsWithPlanned)
                            {
                                refuse(QStringLiteral("ambiguousSource"),
                                       QStringLiteral("Destination source would fan out across widgets; set allowFanout=true to permit reuse"));
                                result.items.append(item);
                                continue;
                            }
                            plannedFanoutOwners[destKey].insert(binding.widgetID);
                        }
                        willChange = true;
                        AuthoringSourceKey afterKey;
                        afterKey.controlSlotID = slot;
                        afterKey.inputUniverseID = destUniverse;
                        afterKey.inputChannel = destChannel;
                        afterKey.childSubpage = destSubpage;
                        afterValue = sourceKeyVariant(afterKey);
                    }
                }
                else if (binding.action == AuthoringBindingItem::Action::Remove)
                {
                    if (!selectedSource.isNull())
                    {
                        const AuthoringSourceKey selectedKey = sourceKeyFromSource(selectedSource.data());
                        item.selectedSourceKey = selectedKey;
                        beforeValue = sourceKeyVariant(selectedKey);
                    }
                    willChange = true;
                }
                else
                {
                    refuse(QStringLiteral("invalidArgument"),
                           QStringLiteral("Controller action is not supported"));
                    result.items.append(item);
                    continue;
                }

                if (willChange)
                {
                    AuthoringFieldChange change;
                    change.field = QStringLiteral("sourceKey");
                    change.before = beforeValue;
                    change.after = afterValue;
                    change.changed = true;
                    change.note = QStringLiteral("prepared");
                    item.changes.append(change);
                    item.changedCount = int(item.changes.size());
                }
                else
                {
                    item.outcome = AuthoringPreparationItem::Outcome::Unchanged;
                }
                result.items.append(item);
                continue;
            }

            if (!binding.controlSlotID.has_value())
            {
                refuse(QStringLiteral("invalidArgument"), QStringLiteral("controlSlotID is required"));
                result.items.append(item);
                continue;
            }
            const quint8 slot = *binding.controlSlotID;
            item.selectedControlSlotID = slot;
            if (!hasExternalControlSlot(widget, slot))
            {
                refuse(QStringLiteral("invalidArgument"),
                       QStringLiteral("controlSlotID %1 is not registered for widget %2")
                           .arg(slot).arg(binding.widgetID));
                result.items.append(item);
                continue;
            }

            const std::optional<bool> keyboardAllowed = keyboardAllowedForSlot(widget, slot);
            if (!keyboardAllowed.has_value())
            {
                refuse(QStringLiteral("unsupportedPreparation"),
                       QStringLiteral("Target slot keyboard semantics are not fully typed"),
                       QStringLiteral("unsupportedConfiguration"));
                result.items.append(item);
                continue;
            }
            if (!*keyboardAllowed)
            {
                refuse(QStringLiteral("invalidArgument"),
                       QStringLiteral("controlSlotID %1 does not support keyboard assignments").arg(slot));
                result.items.append(item);
                continue;
            }

            const QString keyText = binding.keySequence.value_or(QString()).trimmed();
            const QKeySequence keySequence = QKeySequence::fromString(keyText, QKeySequence::PortableText);
            if (keyText.isEmpty() || keySequence.isEmpty())
            {
                refuse(QStringLiteral("invalidArgument"), QStringLiteral("keySequence is required"));
                result.items.append(item);
                continue;
            }

            const QMap<QKeySequence, quint32> keyMap = widget->keySequenceMap();
            std::optional<QKeySequence> currentSlotKey;
            for (auto it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
            {
                if (it.value() == slot)
                {
                    currentSlotKey = it.key();
                    break;
                }
            }
            const auto existing = keyMap.constFind(keySequence);
            if (existing != keyMap.constEnd() && existing.value() != slot)
            {
                refuse(QStringLiteral("ambiguousSource"),
                       QStringLiteral("keySequence is already assigned to another control slot"));
                result.items.append(item);
                continue;
            }

            const bool willChange = (binding.action == AuthoringBindingItem::Action::Set)
                ? (!currentSlotKey.has_value() || currentSlotKey.value() != keySequence)
                : (existing != keyMap.constEnd() && existing.value() == slot);
            if (willChange)
            {
                AuthoringFieldChange change;
                change.field = QStringLiteral("keySequence");
                change.before = currentSlotKey.has_value()
                    ? currentSlotKey->toString(QKeySequence::PortableText)
                    : QVariant(QString());
                change.after = (binding.action == AuthoringBindingItem::Action::Set)
                    ? QVariant(keySequence.toString(QKeySequence::PortableText))
                    : QVariant(QString());
                change.changed = true;
                change.note = QStringLiteral("prepared");
                item.changes.append(change);
                item.changedCount = 1;
            }
            else
            {
                item.outcome = AuthoringPreparationItem::Outcome::Unchanged;
            }
            result.items.append(item);
        }

        if (hasErrors)
        {
            for (const AuthoringPreparationItem &item : std::as_const(result.items))
            {
                if (!item.code.isEmpty())
                {
                    result.code = item.code;
                    result.error = item.error;
                    break;
                }
            }
            result.preparationToken.clear();
            result.expiresInMs = 0;
            return result;
        }

        PreparationRecord record;
        record.basis = result.basis;
        record.normalizedRequest = request;
        record.candidateDigest = QString::fromLatin1(
            QCryptographicHash::hash(canonicalRequest, QCryptographicHash::Sha256).toHex());
        record.predicates = predicates;
        for (const AuthoringBindingItem &item : std::as_const(bindingsRequest->items))
        {
            if (item.kind == AuthoringBindingItem::Kind::Function)
            {
                record.predicates.requireFeedbackPatchIdle = true;
                break;
            }
        }
        PreparedBindingPayload payload;
        payload.items = bindingsRequest->items;
        payload.allowFanout = bindingsRequest->allowFanout;
        for (int itemIndex = 0; itemIndex < bindingsRequest->items.size(); ++itemIndex)
        {
            const AuthoringBindingItem &item = bindingsRequest->items.at(itemIndex);
            payload.requestOrder.append(item.widgetID);
            if (item.kind == AuthoringBindingItem::Kind::Controller &&
                (item.action == AuthoringBindingItem::Action::Append ||
                 item.action == AuthoringBindingItem::Action::Move) &&
                item.destination.has_value())
            {
                payload.destinationDigests.insert(itemIndex,
                                                  controllerDestinationDigest(ioMap, *item.destination));
            }
        }
        record.operationPayload = payload;

        auto guardKeySignature = [](const GuardKey &key) -> QString {
            QString sig = QString::number(int(key.kind)) + ':';
            sig += key.widgetID.has_value() ? QString::number(*key.widgetID) : QStringLiteral("-");
            sig += ':';
            sig += key.functionID.has_value() ? QString::number(*key.functionID) : QStringLiteral("-");
            sig += ':';
            if (!key.sourceKey.has_value())
                return sig + QStringLiteral("-");
            const AuthoringSourceKey &src = *key.sourceKey;
            sig += src.controlSlotID.has_value() ? QString::number(*src.controlSlotID) : QStringLiteral("-");
            sig += ',';
            sig += src.inputUniverseID.has_value() ? QString::number(*src.inputUniverseID) : QStringLiteral("-");
            sig += ',';
            sig += src.inputChannel.has_value() ? QString::number(*src.inputChannel) : QStringLiteral("-");
            sig += ',';
            sig += src.childSubpage.has_value() ? QString::number(*src.childSubpage) : QStringLiteral("-");
            return sig;
        };
        QSet<QString> guardedKeys;
        auto addGuard = [&](const GuardKey &key) {
            const QString signature = guardKeySignature(key);
            if (guardedKeys.contains(signature))
                return;
            QObject *resolved = resolveGuardObject(key);
            if (!resolved)
                return;
            if (record.geometryGuards.size() >= kAuthoringMaxGuardsPerRecord)
                return;
            GeometryGuard guard;
            guard.key = key;
            guard.identity = resolved;
            guard.scopedConfigurationDigest = scopedConfigurationDigest(key);
            record.geometryGuards.append(guard);
            guardedKeys.insert(signature);
        };

        for (const AuthoringBindingItem &binding : std::as_const(bindingsRequest->items))
        {
            addGuard(GuardKey{GuardKey::Kind::Widget, binding.widgetID, std::nullopt, std::nullopt});
            if (binding.kind == AuthoringBindingItem::Kind::Function &&
                binding.action == AuthoringBindingItem::Action::Set &&
                binding.functionID.has_value())
            {
                addGuard(GuardKey{GuardKey::Kind::Function, std::nullopt, *binding.functionID, std::nullopt});
            }
            if (binding.kind == AuthoringBindingItem::Kind::Controller &&
                (binding.action == AuthoringBindingItem::Action::Move ||
                 binding.action == AuthoringBindingItem::Action::Remove) &&
                binding.sourceKey.has_value())
            {
                addGuard(GuardKey{GuardKey::Kind::InputSource, binding.widgetID, std::nullopt, binding.sourceKey});
            }
            if (binding.kind == AuthoringBindingItem::Kind::Feedback &&
                binding.sourceKey.has_value())
            {
                addGuard(GuardKey{GuardKey::Kind::InputSource, binding.widgetID, std::nullopt, binding.sourceKey});
            }
        }

        if (record.geometryGuards.size() > kAuthoringMaxGuardsPerRecord)
        {
            result.code = QStringLiteral("preparationLimit");
            result.error = QStringLiteral("Guard closure exceeds limit");
            result.preparationToken.clear();
            result.expiresInMs = 0;
            return result;
        }

        record.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        record.expiresAtMs = nowMs + kAuthoringRecordTtlMs;
        result.candidateDigest = record.candidateDigest;
        result.preparationToken = record.token;
        result.expiresInMs = int(kAuthoringRecordTtlMs);
        result.work.payloadBytes = int(VCAuthoring::toJson(result).dump().size());
        if (result.work.payloadBytes > VCAuthoring::kMaxToolPayloadBytes)
        {
            result.code = QStringLiteral("itemTooLarge");
            result.error = QStringLiteral("Serialized preparation payload exceeds 262144 bytes");
            result.preparationToken.clear();
            result.expiresInMs = 0;
            result.candidateDigest.clear();
            result.diffComplete = false;
            return result;
        }
        m_authoringRecords.insert(record.token, record);
        return result;
    }

    if (const AuthoringRecordPanelRequest *panelsRequest = std::get_if<AuthoringRecordPanelRequest>(&request))
    {
        result.atomicScope = AuthoringPreparationResult::AtomicScope::IndependentItems;
        result.diffDetail = (diffDetailText == QStringLiteral("full"))
            ? AuthoringPreparationResult::DiffDetail::Full
            : AuthoringPreparationResult::DiffDetail::Summary;
        result.diffComplete = (result.diffDetail == AuthoringPreparationResult::DiffDetail::Full);

        if (!panelsRequest->expectedBasis.isEmpty() && panelsRequest->expectedBasis != result.basis)
        {
            result.code = QStringLiteral("staleBasis");
            result.error = QStringLiteral("Authoring basis mismatch");
            return result;
        }

        if (panelsRequest->requireAtomic)
        {
            result.code = QStringLiteral("unsupportedPreparation");
            result.error = QStringLiteral("Record panel updates apply as independent items");
            result.limitations.append({QStringLiteral("unsupportedConfiguration"),
                                       QStringLiteral("request"),
                                       QStringLiteral("requireAtomic is not supported for record panel updates")});
            return result;
        }

        if (m_authoringRecords.size() >= kAuthoringMaxLiveRecords)
        {
            result.code = QStringLiteral("preparationLimit");
            result.error = QStringLiteral("Too many active preparation records");
            return result;
        }

        result.items.reserve(panelsRequest->items.size());
        bool hasErrors = false;
        for (int itemIndex = 0; itemIndex < panelsRequest->items.size(); ++itemIndex)
        {
            const AuthoringRecordPanelItem &panel = panelsRequest->items.at(itemIndex);
            AuthoringPreparationItem item;
            item.index = itemIndex;
            item.targetWidgetID = panel.widgetID;
            item.status = AuthoringPreparationItem::Status::Ok;
            item.outcome = AuthoringPreparationItem::Outcome::Prepared;

            VCWidget *widget = m_vc ? m_vc->widget(panel.widgetID) : nullptr;
            VCRecordPanel *recordPanel = qobject_cast<VCRecordPanel *>(widget);
            if (!recordPanel)
            {
                item.status = AuthoringPreparationItem::Status::Error;
                item.outcome = AuthoringPreparationItem::Outcome::Refused;
                item.code = QStringLiteral("invalidArgument");
                item.error = QStringLiteral("Record panel target widget ID %1 is invalid").arg(panel.widgetID);
                result.items.append(item);
                hasErrors = true;
                continue;
            }

            item.beforeLogicalRect = widget->logicalGeometry();
            item.afterLogicalRect = item.beforeLogicalRect;
            populateSelectionDetails(item, widget);

            auto appendChange = [&](const QString &field, const QVariant &before, const QVariant &after) {
                if (before == after)
                    return;
                AuthoringFieldChange change;
                change.field = field;
                change.before = before;
                change.after = after;
                change.changed = true;
                change.note = QStringLiteral("prepared");
                item.changes.append(change);
            };

            if (panel.targetFolder.has_value())
                appendChange(QStringLiteral("targetFolder"), recordPanel->targetFolder(), panel.targetFolder.value());
            if (panel.scenePrefix.has_value())
                appendChange(QStringLiteral("scenePrefix"), recordPanel->scenePrefix(), panel.scenePrefix.value());
            if (panel.chaserPrefix.has_value())
                appendChange(QStringLiteral("chaserPrefix"), recordPanel->chaserPrefix(), panel.chaserPrefix.value());
            if (panel.defaultFadeIn.has_value())
                appendChange(QStringLiteral("defaultFadeIn"), uint(recordPanel->defaultFadeIn()), panel.defaultFadeIn.value());
            if (panel.defaultHold.has_value())
                appendChange(QStringLiteral("defaultHold"), uint(recordPanel->defaultHold()), panel.defaultHold.value());
            if (panel.defaultFadeOut.has_value())
                appendChange(QStringLiteral("defaultFadeOut"), uint(recordPanel->defaultFadeOut()), panel.defaultFadeOut.value());

            item.changedCount = int(item.changes.size());
            if (item.changedCount == 0)
                item.outcome = AuthoringPreparationItem::Outcome::Unchanged;
            result.items.append(item);
        }

        if (hasErrors)
        {
            for (const AuthoringPreparationItem &item : std::as_const(result.items))
            {
                if (!item.code.isEmpty())
                {
                    result.code = item.code;
                    result.error = item.error;
                    break;
                }
            }
            result.preparationToken.clear();
            result.expiresInMs = 0;
            return result;
        }

        PreparationRecord record;
        record.basis = result.basis;
        record.normalizedRequest = request;
        record.candidateDigest = QString::fromLatin1(
            QCryptographicHash::hash(canonicalRequest, QCryptographicHash::Sha256).toHex());
        record.predicates = predicates;
        PreparedRecordPanelPayload payload;
        payload.items = panelsRequest->items;
        for (const AuthoringRecordPanelItem &item : std::as_const(panelsRequest->items))
            payload.requestOrder.append(item.widgetID);
        record.operationPayload = payload;

        QSet<quint32> guardedWidgets;
        auto addWidgetGuard = [&](quint32 widgetID) {
            if (guardedWidgets.contains(widgetID))
                return;
            const GuardKey key{GuardKey::Kind::Widget, widgetID, std::nullopt, std::nullopt};
            QObject *resolved = resolveGuardObject(key);
            if (!resolved)
                return;
            if (record.geometryGuards.size() >= kAuthoringMaxGuardsPerRecord)
                return;
            GeometryGuard guard;
            guard.key = key;
            guard.identity = resolved;
            guard.scopedConfigurationDigest = scopedConfigurationDigest(key);
            record.geometryGuards.append(guard);
            guardedWidgets.insert(widgetID);
        };
        for (const AuthoringRecordPanelItem &item : std::as_const(panelsRequest->items))
            addWidgetGuard(item.widgetID);

        if (record.geometryGuards.size() > kAuthoringMaxGuardsPerRecord)
        {
            result.code = QStringLiteral("preparationLimit");
            result.error = QStringLiteral("Guard closure exceeds limit");
            result.preparationToken.clear();
            result.expiresInMs = 0;
            return result;
        }

        record.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        record.expiresAtMs = nowMs + kAuthoringRecordTtlMs;
        result.candidateDigest = record.candidateDigest;
        result.preparationToken = record.token;
        result.expiresInMs = int(kAuthoringRecordTtlMs);
        result.work.payloadBytes = int(VCAuthoring::toJson(result).dump().size());
        if (result.work.payloadBytes > VCAuthoring::kMaxToolPayloadBytes)
        {
            result.code = QStringLiteral("itemTooLarge");
            result.error = QStringLiteral("Serialized preparation payload exceeds 262144 bytes");
            result.preparationToken.clear();
            result.expiresInMs = 0;
            result.candidateDigest.clear();
            result.diffComplete = false;
            return result;
        }
        m_authoringRecords.insert(record.token, record);
        return result;
    }

    const AuthoringLayoutRequest *layoutRequest = std::get_if<AuthoringLayoutRequest>(&request);
    if (!layoutRequest)
    {
        result.code = QStringLiteral("unsupportedPreparation");
        result.error = QStringLiteral("This bridge supports geometry preparation only at this checkpoint");
        result.limitations.append({QStringLiteral("unsupportedPreparation"),
                                   QStringLiteral("request"),
                                   QStringLiteral("Recognized authoring family is not implemented at this checkpoint")});
        return result;
    }

    if (layoutRequest->operation != AuthoringLayoutRequest::Operation::Geometry)
    {
        const bool supportsCheckpointLayoutOp =
            (layoutRequest->operation == AuthoringLayoutRequest::Operation::PageCreate ||
             layoutRequest->operation == AuthoringLayoutRequest::Operation::PageRename ||
             layoutRequest->operation == AuthoringLayoutRequest::Operation::PageDelete ||
             layoutRequest->operation == AuthoringLayoutRequest::Operation::FramePages);
        if (!supportsCheckpointLayoutOp)
        {
            result.code = QStringLiteral("unsupportedPreparation");
            result.error = QStringLiteral("This bridge supports geometry preparation only at this checkpoint");
            result.limitations.append({QStringLiteral("unsupportedPreparation"),
                                       QStringLiteral("request"),
                                       QStringLiteral("Recognized authoring family is not implemented at this checkpoint")});
            return result;
        }

        if (m_authoringRecords.size() >= kAuthoringMaxLiveRecords)
        {
            AuthoringPreparationResult capacity;
            capacity.code = QStringLiteral("preparationCapacity");
            capacity.error = QStringLiteral("No free preparation record slots");
            return capacity;
        }

        result.atomicScope = AuthoringPreparationResult::AtomicScope::NativeGroup;
        result.diffDetail = (diffDetailText == QStringLiteral("full"))
            ? AuthoringPreparationResult::DiffDetail::Full
            : AuthoringPreparationResult::DiffDetail::Summary;
        result.diffComplete = (result.diffDetail == AuthoringPreparationResult::DiffDetail::Full);
        result.items.reserve(1);

        if (!layoutRequest->expectedBasis.isEmpty() && layoutRequest->expectedBasis != result.basis)
        {
            result.code = QStringLiteral("staleBasis");
            result.error = QStringLiteral("Authoring basis mismatch");
            return result;
        }

        AuthoringPreparationItem item;
        item.index = 0;
        item.status = AuthoringPreparationItem::Status::Ok;
        item.outcome = AuthoringPreparationItem::Outcome::Prepared;
        item.beforeLogicalRect = QRectF();
        item.afterLogicalRect = QRectF();

        auto appendChange = [&](const QString &field, const QVariant &before, const QVariant &after) {
            if (before == after)
                return;
            AuthoringFieldChange change;
            change.field = field;
            change.before = before;
            change.after = after;
            change.changed = true;
            change.note = QStringLiteral("prepared");
            item.changes.append(change);
        };

        auto resolvePageByWidgetId = [&](quint32 pageWidgetID, int *pageIndexOut) -> VCPage * {
            for (int pageIndex = 0; pageIndex < m_vc->pagesCount(); ++pageIndex)
            {
                VCPage *page = m_vc->page(pageIndex);
                if (page && page->id() == pageWidgetID)
                {
                    if (pageIndexOut)
                        *pageIndexOut = pageIndex;
                    return page;
                }
            }
            return nullptr;
        };

        switch (layoutRequest->operation)
        {
        case AuthoringLayoutRequest::Operation::PageCreate:
        {
            item.targetWidgetID = VCWidget::invalidId();
            appendChange(QStringLiteral("pagesCount"), m_vc->pagesCount(), m_vc->pagesCount() + 1);
            appendChange(QStringLiteral("name"), QVariant(), layoutRequest->name.value_or(QString()));
            break;
        }
        case AuthoringLayoutRequest::Operation::PageRename:
        {
            int pageIndex = -1;
            VCPage *page = resolvePageByWidgetId(*layoutRequest->pageWidgetID, &pageIndex);
            if (!page || pageIndex < 0)
            {
                item.status = AuthoringPreparationItem::Status::Error;
                item.outcome = AuthoringPreparationItem::Outcome::Refused;
                item.code = QStringLiteral("invalidArgument");
                item.error = QStringLiteral("pageWidgetID references an unknown page");
                break;
            }
            item.targetWidgetID = page->id();
            populateSelectionDetails(item, page);
            appendChange(QStringLiteral("name"), page->caption(), layoutRequest->name.value_or(QString()));
            break;
        }
        case AuthoringLayoutRequest::Operation::PageDelete:
        {
            int pageIndex = -1;
            VCPage *page = resolvePageByWidgetId(*layoutRequest->pageWidgetID, &pageIndex);
            if (!page || pageIndex < 0)
            {
                item.status = AuthoringPreparationItem::Status::Error;
                item.outcome = AuthoringPreparationItem::Outcome::Refused;
                item.code = QStringLiteral("invalidArgument");
                item.error = QStringLiteral("pageWidgetID references an unknown page");
                break;
            }
            if (m_vc->pagesCount() <= 1)
            {
                item.status = AuthoringPreparationItem::Status::Error;
                item.outcome = AuthoringPreparationItem::Outcome::Refused;
                item.code = QStringLiteral("invalidArgument");
                item.error = QStringLiteral("cannot delete the last remaining page");
                break;
            }
            item.targetWidgetID = page->id();
            populateSelectionDetails(item, page);
            appendChange(QStringLiteral("pagesCount"), m_vc->pagesCount(), m_vc->pagesCount() - 1);
            break;
        }
        case AuthoringLayoutRequest::Operation::FramePages:
        {
            VCWidget *widget = m_vc->widget(*layoutRequest->frameWidgetID);
            VCFrame *frame = qobject_cast<VCFrame *>(widget);
            if (!frame || qobject_cast<VCPage *>(frame))
            {
                item.status = AuthoringPreparationItem::Status::Error;
                item.outcome = AuthoringPreparationItem::Outcome::Refused;
                item.code = QStringLiteral("invalidArgument");
                item.error = QStringLiteral("frameWidgetID references an unknown frame");
                break;
            }
            item.targetWidgetID = frame->id();
            item.beforeLogicalRect = frame->logicalGeometry();
            item.afterLogicalRect = item.beforeLogicalRect;
            populateSelectionDetails(item, frame);
            appendChange(QStringLiteral("totalPages"), frame->totalPagesNumber(), layoutRequest->totalPages.value());
            appendChange(QStringLiteral("pageLabels"), frame->pageLabels(), layoutRequest->pageLabels.value());
            appendChange(QStringLiteral("pagesLoop"), frame->pagesLoop(), layoutRequest->pagesLoop.value());
            break;
        }
        default:
            break;
        }

        item.changedCount = item.changes.size();
        if (item.status == AuthoringPreparationItem::Status::Error)
        {
            result.code = item.code;
            result.error = item.error;
            result.items.append(item);
            result.preparationToken.clear();
            result.expiresInMs = 0;
            return result;
        }
        if (item.changedCount == 0)
            item.outcome = AuthoringPreparationItem::Outcome::Unchanged;
        result.items.append(item);

        PreparationRecord record;
        record.basis = result.basis;
        record.normalizedRequest = request;
        record.candidateDigest = QString::fromLatin1(
            QCryptographicHash::hash(canonicalRequest, QCryptographicHash::Sha256).toHex());
        record.predicates = predicates;
        PreparedLayoutPayload payload;
        payload.request = *layoutRequest;
        record.operationPayload = payload;

        QSet<quint32> guardedWidgets;
        auto addWidgetGuard = [&](quint32 widgetID) {
            if (guardedWidgets.contains(widgetID))
                return;
            const GuardKey key{GuardKey::Kind::Widget, widgetID, std::nullopt, std::nullopt};
            QObject *resolved = resolveGuardObject(key);
            if (!resolved)
                return;
            if (record.geometryGuards.size() >= kAuthoringMaxGuardsPerRecord)
                return;
            GeometryGuard guard;
            guard.key = key;
            guard.identity = resolved;
            guard.scopedConfigurationDigest = scopedConfigurationDigest(key);
            record.geometryGuards.append(guard);
            guardedWidgets.insert(widgetID);
        };

        switch (layoutRequest->operation)
        {
        case AuthoringLayoutRequest::Operation::PageCreate:
            for (int pageIndex = 0; pageIndex < m_vc->pagesCount(); ++pageIndex)
            {
                VCPage *page = m_vc->page(pageIndex);
                if (page)
                    addWidgetGuard(page->id());
            }
            break;
        case AuthoringLayoutRequest::Operation::PageRename:
        case AuthoringLayoutRequest::Operation::PageDelete:
            addWidgetGuard(*layoutRequest->pageWidgetID);
            break;
        case AuthoringLayoutRequest::Operation::FramePages:
            addWidgetGuard(*layoutRequest->frameWidgetID);
            break;
        default:
            break;
        }

        if (record.geometryGuards.size() > kAuthoringMaxGuardsPerRecord)
        {
            result.code = QStringLiteral("preparationLimit");
            result.error = QStringLiteral("Guard closure exceeds limit");
            result.preparationToken.clear();
            result.expiresInMs = 0;
            return result;
        }

        record.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        record.expiresAtMs = nowMs + kAuthoringRecordTtlMs;
        result.candidateDigest = record.candidateDigest;
        result.preparationToken = record.token;
        result.expiresInMs = int(kAuthoringRecordTtlMs);
        result.work.payloadBytes = int(VCAuthoring::toJson(result).dump().size());
        if (result.work.payloadBytes > VCAuthoring::kMaxToolPayloadBytes)
        {
            result.code = QStringLiteral("itemTooLarge");
            result.error = QStringLiteral("Serialized preparation payload exceeds 262144 bytes");
            result.preparationToken.clear();
            result.expiresInMs = 0;
            result.candidateDigest.clear();
            result.diffComplete = false;
            return result;
        }
        m_authoringRecords.insert(record.token, record);
        return result;
    }

    if (layoutRequest->geometryChanges.isEmpty())
    {
        AuthoringPreparationResult invalid;
        invalid.code = QStringLiteral("invalidArgument");
        invalid.error = QStringLiteral("geometryChanges must not be empty");
        return invalid;
    }

    if (layoutRequest->geometryChanges.size() > kAuthoringMaxItemsPerRequest)
    {
        AuthoringPreparationResult limit;
        limit.code = QStringLiteral("preparationLimit");
        limit.error = QStringLiteral("Too many request items");
        return limit;
    }

    if (m_authoringRecords.size() >= kAuthoringMaxLiveRecords)
    {
        AuthoringPreparationResult capacity;
        capacity.code = QStringLiteral("preparationCapacity");
        capacity.error = QStringLiteral("No free preparation record slots");
        return capacity;
    }

    result.atomicScope = AuthoringPreparationResult::AtomicScope::NativeGroup;
    result.diffDetail = (diffDetailText == QStringLiteral("full"))
        ? AuthoringPreparationResult::DiffDetail::Full
        : AuthoringPreparationResult::DiffDetail::Summary;
    result.diffComplete = (result.diffDetail == AuthoringPreparationResult::DiffDetail::Full);
    result.items.reserve(layoutRequest->geometryChanges.size());

    if (!layoutRequest->expectedBasis.isEmpty() && layoutRequest->expectedBasis != result.basis)
    {
        result.code = QStringLiteral("staleBasis");
        result.error = QStringLiteral("Authoring basis mismatch");
        return result;
    }

    QMap<quint32, QRectF> candidates;
    QSet<quint32> seen;
    bool hasErrors = false;

    for (int i = 0; i < layoutRequest->geometryChanges.size(); ++i)
    {
        const AuthoringGeometryChange &change = layoutRequest->geometryChanges.at(i);
        AuthoringPreparationItem item;
        item.index = i;
        item.targetWidgetID = change.widgetID;
        item.status = AuthoringPreparationItem::Status::Ok;
        item.outcome = AuthoringPreparationItem::Outcome::Prepared;

        if (seen.contains(change.widgetID))
        {
            item.status = AuthoringPreparationItem::Status::Error;
            item.outcome = AuthoringPreparationItem::Outcome::Refused;
            item.code = QStringLiteral("invalidArgument");
            item.error = QStringLiteral("Duplicate widget ID %1").arg(change.widgetID);
            hasErrors = true;
            result.items.append(item);
            continue;
        }
        seen.insert(change.widgetID);

        if (!std::isfinite(change.logicalRect.x()) || !std::isfinite(change.logicalRect.y()) ||
            !std::isfinite(change.logicalRect.width()) || !std::isfinite(change.logicalRect.height()) ||
            change.logicalRect.width() <= 0.0 || change.logicalRect.height() <= 0.0)
        {
            item.status = AuthoringPreparationItem::Status::Error;
            item.outcome = AuthoringPreparationItem::Outcome::Refused;
            item.code = QStringLiteral("invalidArgument");
            item.error = QStringLiteral("Invalid logical geometry for widget %1").arg(change.widgetID);
            hasErrors = true;
            result.items.append(item);
            continue;
        }

        VCWidget *widget = m_vc->widget(change.widgetID);
        if (!widget)
        {
            item.status = AuthoringPreparationItem::Status::Error;
            item.outcome = AuthoringPreparationItem::Outcome::Refused;
            item.code = QStringLiteral("invalidArgument");
            item.error = QStringLiteral("Unknown widget %1").arg(change.widgetID);
            hasErrors = true;
            result.items.append(item);
            continue;
        }

        item.beforeLogicalRect = widget->logicalGeometry();
        item.afterLogicalRect = change.logicalRect;
        populateSelectionDetails(item, widget);

        const QRectF candidate = change.logicalRect;
        item.afterLogicalRect = candidate;
        recomputeGeometryDiff(item, result.diffDetail);
        candidates.insert(change.widgetID, candidate);
        result.items.append(item);
    }

    if (hasErrors)
    {
        if (result.code.isEmpty())
        {
            for (const AuthoringPreparationItem &item : result.items)
                if (!item.code.isEmpty())
                {
                    result.code = item.code;
                    break;
                }
        }
        if (result.error.isEmpty())
            for (const AuthoringPreparationItem &item : result.items)
                if (!item.error.isEmpty())
                {
                    result.error = item.error;
                    break;
                }
        result.preparationToken.clear();
        result.expiresInMs = 0;
        return result;
    }

    const QString geometryPreconditionError = authoringPreconditionError(
        candidates, &preconditionCode, &result.preconditions, predicates);
    if (!geometryPreconditionError.isEmpty())
    {
        result.code = preconditionCode;
        result.error = geometryPreconditionError;
        result.preparationToken.clear();
        result.expiresInMs = 0;
        return result;
    }

    QMap<quint32, QRectF> preflightCandidates = candidates;
    if (layoutRequest->snapToGrid)
    {
        for (int i = 0; i < layoutRequest->geometryChanges.size(); ++i)
        {
            const AuthoringGeometryChange &change = layoutRequest->geometryChanges.at(i);
            const QVariantMap previewSingle = m_vc->previewGeometry(
                change.widgetID, change.logicalRect, true);
            if (!previewSingle.value(QStringLiteral("accepted")).toBool())
            {
                const QString previewError = previewSingle.value(QStringLiteral("error")).toString();
                result.code = QStringLiteral("nativeRefusal");
                result.error = previewError;
                for (AuthoringPreparationItem &item : result.items)
                {
                    if (item.targetWidgetID != change.widgetID)
                        continue;
                    item.status = AuthoringPreparationItem::Status::Error;
                    item.outcome = AuthoringPreparationItem::Outcome::Refused;
                    item.code = result.code;
                    item.error = result.error;
                    break;
                }
                result.preparationToken.clear();
                result.expiresInMs = 0;
                return result;
            }
            const QVariantList geometries = previewSingle.value(QStringLiteral("geometries")).toList();
            if (geometries.isEmpty())
            {
                result.code = QStringLiteral("nativeRefusal");
                result.error = QStringLiteral("Native preview did not return geometry");
                result.preparationToken.clear();
                result.expiresInMs = 0;
                return result;
            }
            const QRectF snapped = geometries.first().toMap().value(QStringLiteral("rect")).toRectF();
            preflightCandidates.insert(change.widgetID, snapped);
            for (AuthoringPreparationItem &item : result.items)
            {
                if (item.targetWidgetID != change.widgetID)
                    continue;
                item.afterLogicalRect = snapped;
                recomputeGeometryDiff(item, result.diffDetail);
                break;
            }
        }
    }

    const QVariantMap previewGroup = m_vc->previewGeometryPlan(preflightCandidates);
    if (!previewGroup.value(QStringLiteral("accepted")).toBool())
    {
        result.code = QStringLiteral("nativeRefusal");
        result.error = previewGroup.value(QStringLiteral("error")).toString();
        for (AuthoringPreparationItem &item : result.items)
        {
            if (item.status == AuthoringPreparationItem::Status::Ok)
            {
                item.status = AuthoringPreparationItem::Status::Error;
                item.outcome = AuthoringPreparationItem::Outcome::Refused;
                item.code = result.code;
                item.error = result.error;
            }
        }
        result.preparationToken.clear();
        result.expiresInMs = 0;
        return result;
    }

    const QMap<quint32, QRectF> nativePlan = planFromNativePreview(previewGroup);
    if (nativePlan.size() != preflightCandidates.size())
    {
        result.code = QStringLiteral("nativeRefusal");
        result.error = QStringLiteral("Native preview plan did not include all requested widgets");
        result.preparationToken.clear();
        result.expiresInMs = 0;
        return result;
    }
    for (AuthoringPreparationItem &item : result.items)
    {
        if (!nativePlan.contains(item.targetWidgetID))
            continue;
        item.afterLogicalRect = nativePlan.value(item.targetWidgetID);
        recomputeGeometryDiff(item, result.diffDetail);
    }

    const int guardCount = estimateGuardCount(nativePlan);
    if (guardCount > kAuthoringMaxGuardsPerRecord)
    {
        result.code = QStringLiteral("preparationLimit");
        result.error = QStringLiteral("Guard closure exceeds limit");
        result.preparationToken.clear();
        result.expiresInMs = 0;
        return result;
    }

    PreparationRecord record;
    record.basis = result.basis;
    record.normalizedRequest = request;
    QMap<quint32, QRectF> baselineRectangles;
    QList<quint32> requestOrder;
    requestOrder.reserve(layoutRequest->geometryChanges.size());
    for (const AuthoringPreparationItem &item : result.items)
    {
        baselineRectangles.insert(item.targetWidgetID, item.beforeLogicalRect);
        requestOrder.append(item.targetWidgetID);
    }
    record.operationPayload = PreparedGeometryPayload{nativePlan, baselineRectangles, requestOrder};
    record.candidateDigest = candidateDigest(nativePlan);
    record.predicates = predicates;

    auto guardKeySignature = [](const GuardKey &key) -> QString {
        QString sig = QString::number(int(key.kind)) + ':';
        sig += key.widgetID.has_value() ? QString::number(*key.widgetID) : QStringLiteral("-");
        sig += ':';
        sig += key.functionID.has_value() ? QString::number(*key.functionID) : QStringLiteral("-");
        sig += ':';
        if (!key.sourceKey.has_value())
            return sig + QStringLiteral("-");
        const AuthoringSourceKey &src = *key.sourceKey;
        sig += src.controlSlotID.has_value() ? QString::number(*src.controlSlotID) : QStringLiteral("-");
        sig += ',';
        sig += src.inputUniverseID.has_value() ? QString::number(*src.inputUniverseID) : QStringLiteral("-");
        sig += ',';
        sig += src.inputChannel.has_value() ? QString::number(*src.inputChannel) : QStringLiteral("-");
        sig += ',';
        sig += src.childSubpage.has_value() ? QString::number(*src.childSubpage) : QStringLiteral("-");
        return sig;
    };

    QSet<QString> guardedKeys;
    bool guardOverflow = false;
    auto addGuard = [&](const GuardKey &key) {
        if (guardOverflow)
            return;
        const QString signature = guardKeySignature(key);
        if (guardedKeys.contains(signature))
            return;
        QObject *resolved = resolveGuardObject(key);
        if (!resolved)
            return;
        if (record.geometryGuards.size() >= kAuthoringMaxGuardsPerRecord)
        {
            guardOverflow = true;
            return;
        }
        GeometryGuard guard;
        guard.key = key;
        guard.identity = resolved;
        guard.scopedConfigurationDigest = scopedConfigurationDigest(key);
        record.geometryGuards.append(guard);
        guardedKeys.insert(signature);
    };

    for (auto it = nativePlan.constBegin(); it != nativePlan.constEnd(); ++it)
    {
        VCWidget *widget = m_vc->widget(it.key());
        if (!widget)
            continue;
        addGuard(GuardKey{GuardKey::Kind::Widget, widget->id(), std::nullopt, std::nullopt});
        if (std::optional<quint32> functionID = selectedFunctionId(widget))
            addGuard(GuardKey{GuardKey::Kind::Function, std::nullopt, functionID, std::nullopt});
        const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
        for (const QSharedPointer<QLCInputSource> &source : sources)
        {
            if (source.isNull())
                continue;
            AuthoringSourceKey sourceKey;
            if (source->id() <= std::numeric_limits<quint8>::max())
                sourceKey.controlSlotID = quint8(source->id());
            sourceKey.inputUniverseID = source->universe();
            sourceKey.inputChannel = source->channel();
            sourceKey.childSubpage = int(source->page());
            addGuard(GuardKey{GuardKey::Kind::InputSource, widget->id(), std::nullopt, sourceKey});
        }
        for (QObject *parent = widget->parent(); parent; parent = parent->parent())
        {
            if (VCWidget *parentWidget = qobject_cast<VCWidget *>(parent))
                addGuard(GuardKey{GuardKey::Kind::Widget, parentWidget->id(), std::nullopt, std::nullopt});
        }
        if (VCFrame *frame = qobject_cast<VCFrame *>(widget))
            for (VCWidget *child : frame->children(true))
                addGuard(GuardKey{GuardKey::Kind::Widget, child->id(), std::nullopt, std::nullopt});
    }

    if (guardOverflow)
    {
        result.code = QStringLiteral("preparationLimit");
        result.error = QStringLiteral("Guard closure exceeds limit");
        result.preparationToken.clear();
        result.expiresInMs = 0;
        return result;
    }

    record.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.expiresAtMs = nowMs + kAuthoringRecordTtlMs;

    result.candidateDigest = record.candidateDigest;
    result.preparationToken = record.token;
    result.expiresInMs = int(kAuthoringRecordTtlMs);
    result.work.payloadBytes = int(VCAuthoring::toJson(result).dump().size());
    if (result.work.payloadBytes > VCAuthoring::kMaxToolPayloadBytes)
    {
        const QString overflowCode = QStringLiteral("itemTooLarge");
        const QString overflowError = QStringLiteral("Serialized preparation payload exceeds 262144 bytes");
        result.code = overflowCode;
        result.error = overflowError;
        result.preparationToken.clear();
        result.expiresInMs = 0;
        result.candidateDigest.clear();
        result.diffComplete = false;
        for (AuthoringPreparationItem &item : result.items)
        {
            item.status = AuthoringPreparationItem::Status::Error;
            item.outcome = AuthoringPreparationItem::Outcome::Refused;
            item.code = overflowCode;
            item.error = overflowError;
            item.changedCount = 0;
            item.beforeLogicalRect = QRectF();
            item.afterLogicalRect = QRectF();
            item.changes.clear();
            item.limitations.clear();
            item.selectedPageWidgetID.reset();
            item.selectedParentWidgetID.reset();
            item.selectedChildSubpage.reset();
            item.selectedControlSlotID.reset();
            item.selectedFunctionID.reset();
            item.selectedFunctionBeforeID.reset();
            item.selectedFunctionAfterID.reset();
            item.selectedSourceKey.reset();
            item.sourceOrdinal.reset();
            item.configurationDigest.reset();
        }
        while (result.preconditions.size() > 16)
            result.preconditions.removeLast();
        while (result.items.size() > 0)
        {
            result.work.payloadBytes = int(VCAuthoring::toJson(result).dump().size());
            if (result.work.payloadBytes <= VCAuthoring::kMaxToolPayloadBytes)
                break;
            result.items.removeLast();
        }
        while (result.preconditions.size() > 0 &&
               int(VCAuthoring::toJson(result).dump().size()) > VCAuthoring::kMaxToolPayloadBytes)
        {
            result.preconditions.removeLast();
        }
        result.work.payloadBytes = int(VCAuthoring::toJson(result).dump().size());
        return result;
    }
    m_authoringRecords.insert(record.token, record);
    return result;
}

VCBridge::AuthoringApplyResult VCBridgeV5::applyAuthoring(const QString &preparationToken)
{
    const qint64 nowMs = authoringNowMs();
    auto recordIt = m_authoringRecords.find(preparationToken);
    if (recordIt == m_authoringRecords.end())
    {
        cleanupExpiredRecords(nowMs);
        AuthoringApplyResult invalid;
        invalid.code = QStringLiteral("invalidPreparation");
        invalid.error = QStringLiteral("Unknown preparation token");
        return invalid;
    }

    const bool expired = recordIt->expiresAtMs <= nowMs;
    if (expired)
    {
        m_authoringRecords.erase(recordIt);
        AuthoringApplyResult expiredResult;
        expiredResult.code = QStringLiteral("preparationExpired");
        expiredResult.error = QStringLiteral("Preparation token expired");
        expiredResult.tokenConsumed = true;
        return expiredResult;
    }

    PreparationRecord record = recordIt.value();
    m_authoringRecords.erase(recordIt);

    AuthoringApplyResult result;
    result.tokenConsumed = true;
    result.atomicScope = AuthoringApplyResult::AtomicScope::NativeGroup;
    if (std::get_if<PreparedBindingPayload>(&record.operationPayload) ||
        std::get_if<PreparedRecordPanelPayload>(&record.operationPayload))
        result.atomicScope = AuthoringApplyResult::AtomicScope::IndependentItems;

    if (!hasMatchingAuthoringContext())
    {
        result.code = QStringLiteral("unsupportedBridge");
        result.error = QStringLiteral("Authoring operations are not supported by this bridge");
        return result;
    }

    if (record.basis != authoringBasis())
    {
        result.code = QStringLiteral("staleBasis");
        result.error = QStringLiteral("Authoring basis changed since preparation");
        return result;
    }

    if (const PreparedGeometryPayload *payload = std::get_if<PreparedGeometryPayload>(&record.operationPayload))
    {
        QString preconditionCode;
        const QString preconditionError = authoringPreconditionError(
            payload->candidateRectangles, &preconditionCode, nullptr, record.predicates);
        if (!preconditionError.isEmpty())
        {
            result.code = preconditionCode;
            result.error = preconditionError;
            return result;
        }

        for (int i = 0; i < record.geometryGuards.size(); ++i)
        {
            const GeometryGuard &guard = record.geometryGuards.at(i);
            QObject *current = resolveGuardObject(guard.key);
            if (!current || guard.identity.isNull() || current != guard.identity.data() ||
                scopedConfigurationDigest(guard.key) != guard.scopedConfigurationDigest)
            {
                result.code = QStringLiteral("staleSelection");
                result.error = QStringLiteral("Guard identity or configuration changed");
                return result;
            }
        }

        const QVariantMap previewGroup = m_vc->previewGeometryPlan(payload->candidateRectangles);
        if (!previewGroup.value(QStringLiteral("accepted")).toBool())
        {
            result.code = QStringLiteral("nativeRefusal");
            result.error = previewGroup.value(QStringLiteral("error")).toString();
            return result;
        }
        const QMap<quint32, QRectF> currentNativePlan = planFromNativePreview(previewGroup);
        if (candidateDigest(currentNativePlan) != record.candidateDigest)
        {
            result.code = QStringLiteral("staleSelection");
            result.error = QStringLiteral("Prepared candidate digest mismatch");
            return result;
        }

        for (auto it = payload->baselineRectangles.constBegin(); it != payload->baselineRectangles.constEnd(); ++it)
        {
            VCWidget *widget = m_vc->widget(it.key());
            if (!widget || widget->logicalGeometry() != it.value())
            {
                result.code = QStringLiteral("staleSelection");
                result.error = QStringLiteral("Target geometry changed since preparation");
                return result;
            }
        }

        MasterTimer *timer = m_doc ? m_doc->masterTimer() : nullptr;
        MasterTimer::EditAdmission admission(timer, timer && timer->beginRegistryEdit());
        if (!admission)
        {
            result.code = QStringLiteral("authoringPrecondition");
            result.error = QStringLiteral("Function playback or queued starts prevent authoring");
            return result;
        }

        QMap<quint32, QRectF> beforeRects;
        for (auto it = payload->candidateRectangles.constBegin(); it != payload->candidateRectangles.constEnd(); ++it)
        {
            if (VCWidget *widget = m_vc->widget(it.key()))
                beforeRects.insert(it.key(), widget->logicalGeometry());
        }

        const QVariantMap nativeResult = m_vc->applyGeometryPlan(payload->candidateRectangles, false);
        if (!nativeResult.value(QStringLiteral("accepted")).toBool())
        {
            result.code = QStringLiteral("nativeRefusal");
            result.error = nativeResult.value(QStringLiteral("error")).toString();
            return result;
        }

        int index = 0;
        const AuthoringLayoutRequest *layoutRequest = std::get_if<AuthoringLayoutRequest>(&record.normalizedRequest);
        const AuthoringPreparationResult::DiffDetail diffDetail = layoutRequest && layoutRequest->diffDetail.trimmed().compare(QStringLiteral("full"), Qt::CaseInsensitive) == 0
            ? AuthoringPreparationResult::DiffDetail::Full
            : AuthoringPreparationResult::DiffDetail::Summary;
        for (quint32 targetId : payload->requestOrder)
        {
            if (!payload->candidateRectangles.contains(targetId))
                continue;
            AuthoringApplyItem item;
            item.index = index++;
            item.targetWidgetID = targetId;
            item.status = AuthoringApplyItem::Status::Ok;
            item.beforeLogicalRect = beforeRects.value(targetId, QRectF());
            item.afterLogicalRect = payload->candidateRectangles.value(targetId);
            VCWidget *widget = m_vc->widget(targetId);
            populateSelectionDetails(item, widget);
            if (item.beforeLogicalRect == item.afterLogicalRect)
            {
                item.outcome = AuthoringApplyItem::Outcome::Unchanged;
            }
            else if (widget && widget->logicalGeometry() == item.afterLogicalRect)
            {
                item.outcome = AuthoringApplyItem::Outcome::Applied;
            }
            else
            {
                item.outcome = AuthoringApplyItem::Outcome::Unchanged;
            }
            recomputeGeometryDiff(item, diffDetail);
            result.items.append(item);
        }

        result.applied = true;
        return result;
    }

    for (int i = 0; i < record.geometryGuards.size(); ++i)
    {
        const GeometryGuard &guard = record.geometryGuards.at(i);
        QObject *current = resolveGuardObject(guard.key);
        if (!current || guard.identity.isNull() || current != guard.identity.data() ||
            scopedConfigurationDigest(guard.key) != guard.scopedConfigurationDigest)
        {
            result.code = QStringLiteral("staleSelection");
            result.error = QStringLiteral("Guard identity or configuration changed");
            return result;
        }
    }

    if (const PreparedLayoutPayload *layoutPayload =
            std::get_if<PreparedLayoutPayload>(&record.operationPayload))
    {
        const AuthoringLayoutRequest &layout = layoutPayload->request;
        QMap<quint32, QRectF> preconditionTargets;
        for (quint32 widgetID : widgetIdsFromRequest(record.normalizedRequest))
            preconditionTargets.insert(widgetID, QRectF());
        QString preconditionCode;
        const QString preconditionError = authoringPreconditionError(
            preconditionTargets, &preconditionCode, nullptr, record.predicates);
        if (!preconditionError.isEmpty())
        {
            result.code = preconditionCode;
            result.error = preconditionError;
            return result;
        }

        MasterTimer *timer = m_doc ? m_doc->masterTimer() : nullptr;
        MasterTimer::EditAdmission admission(timer, timer && timer->beginRegistryEdit());
        if (!admission)
        {
            result.code = QStringLiteral("authoringPrecondition");
            result.error = QStringLiteral("Function playback or queued starts prevent authoring");
            return result;
        }

        auto resolvePageByWidgetId = [&](quint32 pageWidgetID, int *pageIndexOut) -> VCPage * {
            for (int pageIndex = 0; pageIndex < m_vc->pagesCount(); ++pageIndex)
            {
                VCPage *page = m_vc->page(pageIndex);
                if (page && page->id() == pageWidgetID)
                {
                    if (pageIndexOut)
                        *pageIndexOut = pageIndex;
                    return page;
                }
            }
            return nullptr;
        };

        AuthoringApplyItem item;
        item.index = 0;
        item.status = AuthoringApplyItem::Status::Ok;
        item.outcome = AuthoringApplyItem::Outcome::Unchanged;
        item.beforeLogicalRect = QRectF();
        item.afterLogicalRect = QRectF();

        auto appendChange = [&](const QString &field, const QVariant &before, const QVariant &after) {
            if (before == after)
                return;
            AuthoringFieldChange change;
            change.field = field;
            change.before = before;
            change.after = after;
            change.changed = true;
            change.note = QStringLiteral("applied");
            item.changes.append(change);
        };

        switch (layout.operation)
        {
        case AuthoringLayoutRequest::Operation::PageCreate:
        {
            const int beforeCount = m_vc->pagesCount();
            const int pageIndex = addPage(layout.name.value_or(QString()));
            VCPage *created = m_vc->page(pageIndex);
            if (!created || m_vc->pagesCount() != beforeCount + 1)
            {
                result.code = QStringLiteral("nativeRefusal");
                result.error = QStringLiteral("Page creation was refused");
                return result;
            }
            item.targetWidgetID = created->id();
            appendChange(QStringLiteral("pagesCount"), beforeCount, m_vc->pagesCount());
            appendChange(QStringLiteral("name"), QVariant(), created->caption());
            populateSelectionDetails(item, created);
            break;
        }
        case AuthoringLayoutRequest::Operation::PageRename:
        {
            int pageIndex = -1;
            VCPage *page = resolvePageByWidgetId(*layout.pageWidgetID, &pageIndex);
            if (!page || pageIndex < 0)
            {
                result.code = QStringLiteral("staleSelection");
                result.error = QStringLiteral("Target page no longer exists");
                return result;
            }
            item.targetWidgetID = page->id();
            populateSelectionDetails(item, page);
            const QString beforeName = page->caption();
            if (!renamePage(pageIndex, layout.name.value_or(QString())))
            {
                result.code = QStringLiteral("nativeRefusal");
                result.error = QStringLiteral("Page rename was refused");
                return result;
            }
            appendChange(QStringLiteral("name"), beforeName, page->caption());
            break;
        }
        case AuthoringLayoutRequest::Operation::PageDelete:
        {
            int pageIndex = -1;
            VCPage *page = resolvePageByWidgetId(*layout.pageWidgetID, &pageIndex);
            if (!page || pageIndex < 0)
            {
                result.code = QStringLiteral("staleSelection");
                result.error = QStringLiteral("Target page no longer exists");
                return result;
            }
            if (m_vc->pagesCount() <= 1)
            {
                result.code = QStringLiteral("nativeRefusal");
                result.error = QStringLiteral("Cannot delete the last remaining page");
                return result;
            }
            const int beforeCount = m_vc->pagesCount();
            item.targetWidgetID = page->id();
            populateSelectionDetails(item, page);
            if (!deletePage(pageIndex))
            {
                result.code = QStringLiteral("nativeRefusal");
                result.error = QStringLiteral("Page deletion was refused");
                return result;
            }
            appendChange(QStringLiteral("pagesCount"), beforeCount, m_vc->pagesCount());
            break;
        }
        case AuthoringLayoutRequest::Operation::FramePages:
        {
            VCWidget *widget = m_vc->widget(*layout.frameWidgetID);
            VCFrame *frame = qobject_cast<VCFrame *>(widget);
            if (!frame || qobject_cast<VCPage *>(frame))
            {
                result.code = QStringLiteral("staleSelection");
                result.error = QStringLiteral("Target frame no longer exists");
                return result;
            }
            item.targetWidgetID = frame->id();
            item.beforeLogicalRect = frame->logicalGeometry();
            item.afterLogicalRect = item.beforeLogicalRect;
            populateSelectionDetails(item, frame);

            const int beforeTotalPages = frame->totalPagesNumber();
            const QStringList beforeLabels = frame->pageLabels();
            const bool beforePagesLoop = frame->pagesLoop();

            FrameConfig frameConfig;
            frameConfig.totalPages = layout.totalPages;
            frameConfig.pageLabels = layout.pageLabels;
            frameConfig.pagesLoop = layout.pagesLoop;
            if (!configureFrame(frame->id(), frameConfig))
            {
                result.code = QStringLiteral("nativeRefusal");
                result.error = QStringLiteral("Frame page configuration was refused");
                return result;
            }

            appendChange(QStringLiteral("totalPages"), beforeTotalPages, frame->totalPagesNumber());
            appendChange(QStringLiteral("pageLabels"), beforeLabels, frame->pageLabels());
            appendChange(QStringLiteral("pagesLoop"), beforePagesLoop, frame->pagesLoop());
            break;
        }
        default:
            result.code = QStringLiteral("unsupportedPreparation");
            result.error = QStringLiteral("Prepared operation payload is not supported by this bridge");
            return result;
        }

        item.changedCount = item.changes.size();
        if (item.changedCount > 0)
            item.outcome = AuthoringApplyItem::Outcome::Applied;
        result.items.append(item);
        result.applied = (item.changedCount > 0);
        if (result.applied && m_doc)
            m_doc->setModified();
        return result;
    }

    if (const PreparedRecordPanelPayload *recordPanelPayload =
            std::get_if<PreparedRecordPanelPayload>(&record.operationPayload))
    {
        QMap<quint32, QRectF> panelTargets;
        for (quint32 widgetID : recordPanelPayload->requestOrder)
            panelTargets.insert(widgetID, QRectF());

        QString panelPreconditionCode;
        const QString panelPreconditionError = authoringPreconditionError(
            panelTargets, &panelPreconditionCode, nullptr, record.predicates);
        if (!panelPreconditionError.isEmpty())
        {
            result.code = panelPreconditionCode;
            result.error = panelPreconditionError;
            return result;
        }

        MasterTimer *timer = m_doc ? m_doc->masterTimer() : nullptr;
        MasterTimer::EditAdmission admission(timer, timer && timer->beginRegistryEdit());
        if (!admission)
        {
            result.code = QStringLiteral("authoringPrecondition");
            result.error = QStringLiteral("Function playback or queued starts prevent authoring");
            return result;
        }

        int index = 0;
        bool anyApplied = false;
        for (const AuthoringRecordPanelItem &panelItem : std::as_const(recordPanelPayload->items))
        {
            AuthoringApplyItem item;
            item.index = index++;
            item.targetWidgetID = panelItem.widgetID;
            item.status = AuthoringApplyItem::Status::Ok;
            item.outcome = AuthoringApplyItem::Outcome::Unchanged;

            VCWidget *widget = m_vc ? m_vc->widget(panelItem.widgetID) : nullptr;
            VCRecordPanel *recordPanel = qobject_cast<VCRecordPanel *>(widget);
            if (!recordPanel)
            {
                item.status = AuthoringApplyItem::Status::Error;
                item.outcome = AuthoringApplyItem::Outcome::Refused;
                item.code = QStringLiteral("staleSelection");
                item.error = QStringLiteral("Record panel target no longer exists");
                result.items.append(item);
                continue;
            }

            item.beforeLogicalRect = widget->logicalGeometry();
            item.afterLogicalRect = item.beforeLogicalRect;
            populateSelectionDetails(item, widget);

            const QString beforeTargetFolder = recordPanel->targetFolder();
            const QString beforeScenePrefix = recordPanel->scenePrefix();
            const QString beforeChaserPrefix = recordPanel->chaserPrefix();
            const uint beforeDefaultFadeIn = recordPanel->defaultFadeIn();
            const uint beforeDefaultHold = recordPanel->defaultHold();
            const uint beforeDefaultFadeOut = recordPanel->defaultFadeOut();

            VCBridge::RecordPanelConfig config;
            if (panelItem.targetFolder.has_value())
                config.targetFolder = panelItem.targetFolder;
            if (panelItem.scenePrefix.has_value())
                config.scenePrefix = panelItem.scenePrefix;
            if (panelItem.chaserPrefix.has_value())
                config.chaserPrefix = panelItem.chaserPrefix;
            if (panelItem.defaultFadeIn.has_value())
                config.defaultFadeIn = panelItem.defaultFadeIn;
            if (panelItem.defaultHold.has_value())
                config.defaultHold = panelItem.defaultHold;
            if (panelItem.defaultFadeOut.has_value())
                config.defaultFadeOut = panelItem.defaultFadeOut;

            if (!configureRecordPanel(panelItem.widgetID, config))
            {
                item.status = AuthoringApplyItem::Status::Error;
                item.outcome = AuthoringApplyItem::Outcome::Refused;
                item.code = QStringLiteral("nativeRefusal");
                item.error = QStringLiteral("Record panel configuration was refused");
                result.items.append(item);
                continue;
            }

            auto appendChange = [&](const QString &field, const QVariant &before, const QVariant &after) {
                if (before == after)
                    return;
                AuthoringFieldChange change;
                change.field = field;
                change.before = before;
                change.after = after;
                change.changed = true;
                change.note = QStringLiteral("applied");
                item.changes.append(change);
            };

            if (panelItem.targetFolder.has_value())
                appendChange(QStringLiteral("targetFolder"), beforeTargetFolder, recordPanel->targetFolder());
            if (panelItem.scenePrefix.has_value())
                appendChange(QStringLiteral("scenePrefix"), beforeScenePrefix, recordPanel->scenePrefix());
            if (panelItem.chaserPrefix.has_value())
                appendChange(QStringLiteral("chaserPrefix"), beforeChaserPrefix, recordPanel->chaserPrefix());
            if (panelItem.defaultFadeIn.has_value())
                appendChange(QStringLiteral("defaultFadeIn"), beforeDefaultFadeIn, uint(recordPanel->defaultFadeIn()));
            if (panelItem.defaultHold.has_value())
                appendChange(QStringLiteral("defaultHold"), beforeDefaultHold, uint(recordPanel->defaultHold()));
            if (panelItem.defaultFadeOut.has_value())
                appendChange(QStringLiteral("defaultFadeOut"), beforeDefaultFadeOut, uint(recordPanel->defaultFadeOut()));

            item.changedCount = int(item.changes.size());
            if (item.changedCount > 0)
            {
                item.outcome = AuthoringApplyItem::Outcome::Applied;
                anyApplied = true;
            }
            result.items.append(item);
        }

        if (anyApplied && m_doc)
            m_doc->setModified();
        result.applied = anyApplied;
        return result;
    }

    const PreparedBindingPayload *bindingPayload = std::get_if<PreparedBindingPayload>(&record.operationPayload);
    if (!bindingPayload)
    {
        result.code = QStringLiteral("unsupportedPreparation");
        result.error = QStringLiteral("Prepared operation payload is not supported by this bridge");
        return result;
    }

    QMap<quint32, QRectF> bindingTargets;
    for (quint32 widgetID : bindingPayload->requestOrder)
        bindingTargets.insert(widgetID, QRectF());
    QString bindingPreconditionCode;
    PredicateSelection commonBindingPredicates = record.predicates;
    commonBindingPredicates.requireFeedbackPatchIdle = false;
    const QString bindingPreconditionError = authoringPreconditionError(
        bindingTargets, &bindingPreconditionCode, nullptr, commonBindingPredicates);
    if (!bindingPreconditionError.isEmpty())
    {
        result.code = bindingPreconditionCode;
        result.error = bindingPreconditionError;
        return result;
    }
    if (record.predicates.requireFeedbackPatchIdle)
    {
        QMap<quint32, QRectF> functionTargets;
        for (const AuthoringBindingItem &item : std::as_const(bindingPayload->items))
        {
            if (item.kind == AuthoringBindingItem::Kind::Function)
                functionTargets.insert(item.widgetID, QRectF());
        }
        if (!functionTargets.isEmpty())
        {
            PredicateSelection functionPredicates;
            functionPredicates.requireFeedbackPatchIdle = true;
            QString functionPreconditionCode;
            const QString functionPreconditionError = authoringPreconditionError(
                functionTargets, &functionPreconditionCode, nullptr, functionPredicates);
            if (!functionPreconditionError.isEmpty())
            {
                result.code = functionPreconditionCode;
                result.error = functionPreconditionError;
                return result;
            }
        }
    }
    {
        InputOutputMap *ioMap = m_doc ? m_doc->inputOutputMap() : nullptr;
        for (const AuthoringBindingItem &item : std::as_const(bindingPayload->items))
        {
            if (item.kind != AuthoringBindingItem::Kind::Feedback)
                continue;

            VCWidget *widget = m_vc ? m_vc->widget(item.widgetID) : nullptr;
            if (!widget)
            {
                result.code = QStringLiteral("staleSelection");
                result.error = QStringLiteral("Binding target no longer exists");
                return result;
            }
            if (!item.sourceKey.has_value() ||
                !item.sourceKey->controlSlotID.has_value() ||
                !item.sourceKey->inputUniverseID.has_value() ||
                !item.sourceKey->inputChannel.has_value() ||
                !item.sourceKey->childSubpage.has_value())
            {
                result.code = QStringLiteral("staleSelection");
                result.error = QStringLiteral("Source key is ambiguous or unresolved");
                return result;
            }

            QList<QSharedPointer<QLCInputSource>> matches;
            const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
            for (const QSharedPointer<QLCInputSource> &source : sources)
            {
                if (source.isNull())
                    continue;
                if (source->id() != *item.sourceKey->controlSlotID)
                    continue;
                if (source->universe() != *item.sourceKey->inputUniverseID)
                    continue;
                if ((source->channel() & 0x0000FFFF) != *item.sourceKey->inputChannel)
                    continue;
                if (int(source->page()) != *item.sourceKey->childSubpage)
                    continue;
                matches.append(source);
            }
            if (matches.size() != 1)
            {
                result.code = QStringLiteral("staleSelection");
                result.error = QStringLiteral("Source key is ambiguous or unresolved");
                return result;
            }

            QList<QSharedPointer<QLCInputSource>> affectedSources;
            auto appendWidgetSources = [&](VCWidget *targetWidget) {
                if (!targetWidget)
                    return;
                const QList<QSharedPointer<QLCInputSource>> widgetSources = targetWidget->inputSources();
                for (const QSharedPointer<QLCInputSource> &source : widgetSources)
                    if (!source.isNull())
                        affectedSources.append(source);
            };
            std::function<void(VCWidget *)> collectFeedbackClosure = [&](VCWidget *targetWidget) {
                if (!targetWidget)
                    return;
                appendWidgetSources(targetWidget);
                VCFrame *frameWidget = qobject_cast<VCFrame *>(targetWidget);
                if (!frameWidget)
                    return;
                const QList<VCWidget *> descendants = frameWidget->findChildren<VCWidget *>();
                for (VCWidget *child : descendants)
                {
                    if (!child || child->parent() != frameWidget)
                        continue;
                    if (child->page() != frameWidget->currentPage())
                        continue;
                    collectFeedbackClosure(child);
                }
            };
            collectFeedbackClosure(widget);

            if (ioMap)
            {
                for (const QSharedPointer<QLCInputSource> &source : affectedSources)
                {
                    OutputPatch *patch = ioMap->feedbackPatch(source->universe());
                    if (patch && patch->isPatched())
                    {
                        result.code = QStringLiteral("authoringPrecondition");
                        result.error = QStringLiteral("Feedback patch must be disabled for affected input sources");
                        return result;
                    }
                }
            }
            for (const QSharedPointer<QLCInputSource> &source : affectedSources)
            {
                if (source->needsUpdate())
                {
                    result.code = QStringLiteral("unsupportedPreparation");
                    result.error = QStringLiteral("Feedback source update is pending");
                    return result;
                }
            }
        }
    }

    MasterTimer *timer = m_doc ? m_doc->masterTimer() : nullptr;
    MasterTimer::EditAdmission admission(timer, timer && timer->beginRegistryEdit());
    if (!admission)
    {
        result.code = QStringLiteral("authoringPrecondition");
        result.error = QStringLiteral("Function playback or queued starts prevent authoring");
        return result;
    }

    InputOutputMap *ioMap = m_doc ? m_doc->inputOutputMap() : nullptr;
    for (int i = 0; i < bindingPayload->items.size(); ++i)
    {
        const AuthoringBindingItem &binding = bindingPayload->items.at(i);
        if (binding.kind != AuthoringBindingItem::Kind::Controller ||
            (binding.action != AuthoringBindingItem::Action::Append &&
             binding.action != AuthoringBindingItem::Action::Move))
        {
            continue;
        }
        const QString expectedDigest = bindingPayload->destinationDigests.value(i);
        if (expectedDigest.isEmpty() || !binding.destination.has_value())
            continue;
        const QString currentDigest = controllerDestinationDigest(ioMap, *binding.destination);
        if (currentDigest != expectedDigest)
        {
            result.code = QStringLiteral("staleSelection");
            result.error = QStringLiteral("Destination profile routing changed since preparation");
            return result;
        }
        if (!bindingPayload->allowFanout)
        {
            VCWidget *targetWidget = m_vc ? m_vc->widget(binding.widgetID) : nullptr;
            if (!targetWidget)
            {
                result.code = QStringLiteral("staleSelection");
                result.error = QStringLiteral("Binding target no longer exists");
                return result;
            }

            const quint8 slot = binding.controlSlotID.value_or(0);
            const quint32 universe = *binding.destination->inputUniverseID;
            const quint32 channel = *binding.destination->inputChannel;
            bool introducesNewTuple = true;
            if (binding.action == AuthoringBindingItem::Action::Append)
            {
                const int subpage = targetWidget->page();
                const QList<QSharedPointer<QLCInputSource>> currentSources = targetWidget->inputSources();
                for (const QSharedPointer<QLCInputSource> &candidate : currentSources)
                {
                    if (candidate.isNull())
                        continue;
                    if (candidate->id() != slot ||
                        candidate->universe() != universe ||
                        (candidate->channel() & 0x0000FFFF) != channel ||
                        int(candidate->page()) != subpage)
                    {
                        continue;
                    }
                    introducesNewTuple = false;
                    break;
                }
            }
            else
            {
                if (!binding.sourceKey.has_value() ||
                    !binding.sourceKey->controlSlotID.has_value() ||
                    !binding.sourceKey->inputUniverseID.has_value() ||
                    !binding.sourceKey->inputChannel.has_value() ||
                    !binding.sourceKey->childSubpage.has_value())
                {
                    result.code = QStringLiteral("staleSelection");
                    result.error = QStringLiteral("Source key is ambiguous or unresolved");
                    return result;
                }
                const QList<QSharedPointer<QLCInputSource>> currentSources = targetWidget->inputSources();
                for (const QSharedPointer<QLCInputSource> &source : currentSources)
                {
                    if (source.isNull())
                        continue;
                    if (source->id() != *binding.sourceKey->controlSlotID ||
                        source->universe() != *binding.sourceKey->inputUniverseID ||
                        (source->channel() & 0x0000FFFF) != *binding.sourceKey->inputChannel ||
                        int(source->page()) != *binding.sourceKey->childSubpage)
                    {
                        continue;
                    }
                    const bool sameDestination =
                        source->id() == slot &&
                        source->universe() == universe &&
                        (source->channel() & 0x0000FFFF) == channel;
                    introducesNewTuple = !sameDestination;
                    break;
                }
            }

            if (introducesNewTuple &&
                hasCrossWidgetPhysicalConflict(m_vc, targetWidget, universe, channel))
            {
                result.code = QStringLiteral("staleSelection");
                result.error = QStringLiteral("Cross-widget fanout changed since preparation");
                return result;
            }
        }
    }

    auto sourceMatchesFor = [](VCWidget *widget, const AuthoringSourceKey &key) {
        QList<QSharedPointer<QLCInputSource>> matches;
        if (!widget)
            return matches;
        const QList<QSharedPointer<QLCInputSource>> sources = widget->inputSources();
        for (const QSharedPointer<QLCInputSource> &source : sources)
        {
            if (source.isNull())
                continue;
            if (source->id() != *key.controlSlotID)
                continue;
            if (source->universe() != *key.inputUniverseID)
                continue;
            if ((source->channel() & 0x0000FFFF) != *key.inputChannel)
                continue;
            if (int(source->page()) != *key.childSubpage)
                continue;
            matches.append(source);
        }
        return matches;
    };

    int index = 0;
    bool anyApplied = false;
    for (const AuthoringBindingItem &binding : std::as_const(bindingPayload->items))
    {
        AuthoringApplyItem item;
        item.index = index++;
        item.targetWidgetID = binding.widgetID;
        item.status = AuthoringApplyItem::Status::Ok;
        VCWidget *widget = m_vc ? m_vc->widget(binding.widgetID) : nullptr;
        if (!widget)
        {
            item.status = AuthoringApplyItem::Status::Error;
            item.outcome = AuthoringApplyItem::Outcome::Refused;
            item.code = QStringLiteral("staleSelection");
            item.error = QStringLiteral("Binding target no longer exists");
            result.items.append(item);
            continue;
        }
        item.beforeLogicalRect = widget->logicalGeometry();
        item.afterLogicalRect = item.beforeLogicalRect;
        populateSelectionDetails(item, widget);
        item.outcome = AuthoringApplyItem::Outcome::Unchanged;

        if (binding.kind == AuthoringBindingItem::Kind::Function)
        {
            VCButton *button = qobject_cast<VCButton *>(widget);
            if (!button)
            {
                item.status = AuthoringApplyItem::Status::Error;
                item.outcome = AuthoringApplyItem::Outcome::Refused;
                item.code = QStringLiteral("nativeRefusal");
                item.error = QStringLiteral("Function binding target is not a VCButton");
                result.items.append(item);
                continue;
            }
            const quint32 beforeFunction = button->functionID();
            const quint32 afterFunction =
                (binding.action == AuthoringBindingItem::Action::Clear)
                    ? quint32(Function::invalidId())
                    : binding.functionID.value_or(quint32(Function::invalidId()));
            item.selectedFunctionBeforeID = beforeFunction;
            item.selectedFunctionAfterID = afterFunction;
            item.selectedFunctionID = afterFunction;
            if (beforeFunction != afterFunction)
            {
                const QString beforeCaption = button->caption();
                button->setFunctionID(afterFunction);
                const QString afterCaption = button->caption();
                item.outcome = AuthoringApplyItem::Outcome::Applied;
                AuthoringFieldChange change;
                change.field = QStringLiteral("functionID");
                change.before = qulonglong(beforeFunction);
                change.after = qulonglong(afterFunction);
                change.changed = true;
                change.note = QStringLiteral("applied");
                item.changes.append(change);
                if (beforeCaption != afterCaption)
                {
                    AuthoringFieldChange captionChange;
                    captionChange.field = QStringLiteral("caption");
                    captionChange.before = beforeCaption;
                    captionChange.after = afterCaption;
                    captionChange.changed = true;
                    captionChange.note = QStringLiteral("applied");
                    item.changes.append(captionChange);
                }
                item.changedCount = int(item.changes.size());
                anyApplied = true;
            }
            result.items.append(item);
            continue;
        }

        if (binding.kind == AuthoringBindingItem::Kind::Controller)
        {
            if (!binding.controlSlotID.has_value())
            {
                item.status = AuthoringApplyItem::Status::Error;
                item.outcome = AuthoringApplyItem::Outcome::Refused;
                item.code = QStringLiteral("invalidArgument");
                item.error = QStringLiteral("controlSlotID is required");
                result.items.append(item);
                continue;
            }
            const quint8 slot = *binding.controlSlotID;
            item.selectedControlSlotID = slot;
            if (!hasExternalControlSlot(widget, slot))
            {
                item.status = AuthoringApplyItem::Status::Error;
                item.outcome = AuthoringApplyItem::Outcome::Refused;
                item.code = QStringLiteral("invalidArgument");
                item.error = QStringLiteral("controlSlotID %1 is not registered for widget %2")
                                 .arg(slot)
                                 .arg(binding.widgetID);
                result.items.append(item);
                continue;
            }

            auto addControllerChange = [&](const QVariant &before, const QVariant &after) {
                AuthoringFieldChange change;
                change.field = QStringLiteral("sourceKey");
                change.before = before;
                change.after = after;
                change.changed = true;
                change.note = QStringLiteral("applied");
                item.changes.append(change);
                item.changedCount = int(item.changes.size());
            };

            if (binding.action == AuthoringBindingItem::Action::Append)
            {
                if (!binding.destination.has_value() ||
                    !binding.destination->inputUniverseID.has_value() ||
                    !binding.destination->inputChannel.has_value())
                {
                    item.status = AuthoringApplyItem::Status::Error;
                    item.outcome = AuthoringApplyItem::Outcome::Refused;
                    item.code = QStringLiteral("invalidArgument");
                    item.error = QStringLiteral("Controller destination requires inputUniverseID and inputChannel");
                    result.items.append(item);
                    continue;
                }
                const quint32 destUniverse = *binding.destination->inputUniverseID;
                const quint32 destChannel = *binding.destination->inputChannel;
                const int destSubpage = widget->page();
                const QList<QSharedPointer<QLCInputSource>> currentSources = widget->inputSources();
                bool exists = false;
                for (const QSharedPointer<QLCInputSource> &candidate : currentSources)
                {
                    if (candidate.isNull())
                        continue;
                    if (candidate->id() != slot)
                        continue;
                    if (candidate->universe() != destUniverse)
                        continue;
                    if ((candidate->channel() & 0x0000FFFF) != destChannel)
                        continue;
                    if (int(candidate->page()) != destSubpage)
                        continue;
                    exists = true;
                    item.selectedSourceKey = sourceKeyFromSource(candidate.data());
                    break;
                }
                if (!exists)
                {
                    QSharedPointer<QLCInputSource> created(new QLCInputSource(destUniverse, destChannel));
                    created->setID(slot);
                    created->setPage(destSubpage);
                    const quint32 packedChannel = (quint32(destSubpage) << 16) | (destChannel & 0x0000FFFF);
                    created->setChannel(packedChannel);
                    widget->addInputSource(created);
                    for (int p = 0; p < m_vc->pagesCount(); ++p)
                    {
                        if (VCPage *page = m_vc->page(p))
                            page->mapInputSource(created, widget, true);
                    }
                    m_vc->updatePageInputs();
                    if (m_doc)
                        m_doc->setModified();
                    item.outcome = AuthoringApplyItem::Outcome::Applied;
                    item.selectedSourceKey = sourceKeyFromSource(created.data());
                    addControllerChange(QVariant(QString()), sourceKeyVariant(*item.selectedSourceKey));
                    anyApplied = true;
                }
                result.items.append(item);
                continue;
            }

            if (binding.action == AuthoringBindingItem::Action::Move ||
                binding.action == AuthoringBindingItem::Action::Remove)
            {
                if (!binding.sourceKey.has_value() || !binding.sourceKey->controlSlotID.has_value() ||
                    !binding.sourceKey->inputUniverseID.has_value() ||
                    !binding.sourceKey->inputChannel.has_value() ||
                    !binding.sourceKey->childSubpage.has_value())
                {
                    item.status = AuthoringApplyItem::Status::Error;
                    item.outcome = AuthoringApplyItem::Outcome::Refused;
                    item.code = QStringLiteral("invalidArgument");
                    item.error = QStringLiteral("Source key requires controlSlotID, inputUniverseID, inputChannel and childSubpage");
                    result.items.append(item);
                    continue;
                }
                const QList<QSharedPointer<QLCInputSource>> matches = sourceMatchesFor(widget, *binding.sourceKey);
                if (matches.size() != 1)
                {
                    item.status = AuthoringApplyItem::Status::Error;
                    item.outcome = AuthoringApplyItem::Outcome::Refused;
                    item.code = QStringLiteral("staleSelection");
                    item.error = QStringLiteral("Source key is ambiguous or unresolved");
                    result.items.append(item);
                    continue;
                }
                const QSharedPointer<QLCInputSource> source = matches.first();
                const AuthoringSourceKey beforeKey = sourceKeyFromSource(source.data());
                item.selectedSourceKey = beforeKey;

                if (binding.action == AuthoringBindingItem::Action::Move)
                {
                    if (!binding.destination.has_value() ||
                        !binding.destination->inputUniverseID.has_value() ||
                        !binding.destination->inputChannel.has_value())
                    {
                        item.status = AuthoringApplyItem::Status::Error;
                        item.outcome = AuthoringApplyItem::Outcome::Refused;
                        item.code = QStringLiteral("invalidArgument");
                        item.error = QStringLiteral("Controller destination requires inputUniverseID and inputChannel");
                        result.items.append(item);
                        continue;
                    }
                    const quint32 destUniverse = *binding.destination->inputUniverseID;
                    const quint32 destChannel = *binding.destination->inputChannel;
                    const int destSubpage = int(source->page());
                    const bool sameDestination =
                        source->id() == slot &&
                        source->universe() == destUniverse &&
                        (source->channel() & 0x0000FFFF) == destChannel &&
                        int(source->page()) == destSubpage;
                    if (!sameDestination)
                    {
                        const quint32 oldUniverse = source->universe();
                        const quint32 oldPackedChannel = source->channel();
                        for (int p = 0; p < m_vc->pagesCount(); ++p)
                        {
                            if (VCPage *page = m_vc->page(p))
                                page->unMapInputSource(source->id(), oldUniverse, oldPackedChannel, widget, true);
                        }
                        const quint32 newPackedChannel = (quint32(destSubpage) << 16) | (destChannel & 0x0000FFFF);
                        widget->updateInputSource(source, destUniverse, newPackedChannel);
                        for (int p = 0; p < m_vc->pagesCount(); ++p)
                        {
                            if (VCPage *page = m_vc->page(p))
                                page->mapInputSource(source, widget, true);
                        }
                        m_vc->updatePageInputs();
                        item.outcome = AuthoringApplyItem::Outcome::Applied;
                        item.selectedSourceKey = sourceKeyFromSource(source.data());
                        addControllerChange(sourceKeyVariant(beforeKey), sourceKeyVariant(*item.selectedSourceKey));
                        anyApplied = true;
                    }
                    result.items.append(item);
                    continue;
                }

                m_vc->deleteInputSource(widget, source->id(), source->universe(), source->channel());
                m_vc->updatePageInputs();
                item.outcome = AuthoringApplyItem::Outcome::Applied;
                addControllerChange(sourceKeyVariant(beforeKey), QVariant(QString()));
                anyApplied = true;
                result.items.append(item);
                continue;
            }

            item.status = AuthoringApplyItem::Status::Error;
            item.outcome = AuthoringApplyItem::Outcome::Refused;
            item.code = QStringLiteral("invalidArgument");
            item.error = QStringLiteral("Controller action is not supported");
            result.items.append(item);
            continue;
        }

        if (binding.kind == AuthoringBindingItem::Kind::Feedback)
        {
            if (!binding.sourceKey.has_value() ||
                !binding.sourceKey->controlSlotID.has_value() ||
                !binding.sourceKey->inputUniverseID.has_value() ||
                !binding.sourceKey->inputChannel.has_value() ||
                !binding.sourceKey->childSubpage.has_value())
            {
                item.status = AuthoringApplyItem::Status::Error;
                item.outcome = AuthoringApplyItem::Outcome::Refused;
                item.code = QStringLiteral("invalidArgument");
                item.error = QStringLiteral("Source key requires controlSlotID, inputUniverseID, inputChannel and childSubpage");
                result.items.append(item);
                continue;
            }

            const QList<QSharedPointer<QLCInputSource>> matches = sourceMatchesFor(widget, *binding.sourceKey);
            if (matches.size() != 1)
            {
                item.status = AuthoringApplyItem::Status::Error;
                item.outcome = AuthoringApplyItem::Outcome::Refused;
                item.code = QStringLiteral("staleSelection");
                item.error = QStringLiteral("Source key is ambiguous or unresolved");
                result.items.append(item);
                continue;
            }

            const QSharedPointer<QLCInputSource> source = matches.first();
            item.selectedSourceKey = sourceKeyFromSource(source.data());
            auto addFeedbackChange = [&](const QString &field, const QVariant &before, const QVariant &after) {
                if (before == after)
                    return;
                AuthoringFieldChange change;
                change.field = field;
                change.before = before;
                change.after = after;
                change.changed = true;
                change.note = QStringLiteral("applied");
                item.changes.append(change);
            };

            if (binding.idleValue.has_value())
            {
                const int before = int(source->feedbackValue(QLCInputFeedback::LowerValue));
                const int after = *binding.idleValue;
                if (before != after)
                    source->setFeedbackValue(QLCInputFeedback::LowerValue, uchar(after));
                addFeedbackChange(QStringLiteral("idleValue"), before, after);
            }
            if (binding.activeValue.has_value())
            {
                const int before = int(source->feedbackValue(QLCInputFeedback::UpperValue));
                const int after = *binding.activeValue;
                if (before != after)
                    source->setFeedbackValue(QLCInputFeedback::UpperValue, uchar(after));
                addFeedbackChange(QStringLiteral("activeValue"), before, after);
            }
            if (binding.monitorValue.has_value())
            {
                const int before = int(source->feedbackValue(QLCInputFeedback::MonitorValue));
                const int after = *binding.monitorValue;
                if (before != after)
                    source->setFeedbackValue(QLCInputFeedback::MonitorValue, uchar(after));
                addFeedbackChange(QStringLiteral("monitorValue"), before, after);
            }
            if (binding.idleChannel.has_value())
            {
                const int before = source->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
                const int after = *binding.idleChannel;
                if (before != after)
                    source->setFeedbackExtraParams(QLCInputFeedback::LowerValue, after);
                addFeedbackChange(QStringLiteral("idleChannel"), before, after);
            }
            if (binding.activeChannel.has_value())
            {
                const int before = source->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
                const int after = *binding.activeChannel;
                if (before != after)
                    source->setFeedbackExtraParams(QLCInputFeedback::UpperValue, after);
                addFeedbackChange(QStringLiteral("activeChannel"), before, after);
            }
            if (binding.monitorChannel.has_value())
            {
                const int before = source->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
                const int after = *binding.monitorChannel;
                if (before != after)
                    source->setFeedbackExtraParams(QLCInputFeedback::MonitorValue, after);
                addFeedbackChange(QStringLiteral("monitorChannel"), before, after);
            }

            item.changedCount = int(item.changes.size());
            if (item.changedCount > 0)
            {
                item.outcome = AuthoringApplyItem::Outcome::Applied;
                anyApplied = true;
            }
            result.items.append(item);
            continue;
        }

        const quint8 slot = binding.controlSlotID.value_or(0);
        item.selectedControlSlotID = slot;
        const QString keyText = binding.keySequence.value_or(QString()).trimmed();
        const QKeySequence keySequence = QKeySequence::fromString(keyText, QKeySequence::PortableText);
        if (keyText.isEmpty() || keySequence.isEmpty())
        {
            item.status = AuthoringApplyItem::Status::Error;
            item.outcome = AuthoringApplyItem::Outcome::Refused;
            item.code = QStringLiteral("invalidArgument");
            item.error = QStringLiteral("keySequence is invalid");
            result.items.append(item);
            continue;
        }

        const QMap<QKeySequence, quint32> keyMap = widget->keySequenceMap();
        std::optional<QKeySequence> slotKey;
        for (auto it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
        {
            if (it.value() == slot)
            {
                slotKey = it.key();
                break;
            }
        }
        const auto existing = keyMap.constFind(keySequence);
        if (existing != keyMap.constEnd() && existing.value() != slot)
        {
            item.status = AuthoringApplyItem::Status::Error;
            item.outcome = AuthoringApplyItem::Outcome::Refused;
            item.code = QStringLiteral("ambiguousSource");
            item.error = QStringLiteral("keySequence is already assigned to another control slot");
            result.items.append(item);
            continue;
        }

        const bool shouldApply = (binding.action == AuthoringBindingItem::Action::Set)
            ? (!slotKey.has_value() || slotKey.value() != keySequence)
            : (existing != keyMap.constEnd() && existing.value() == slot);
        if (!shouldApply)
        {
            result.items.append(item);
            continue;
        }

        if (slotKey.has_value())
            widget->deleteKeySequence(slotKey.value());
        if (binding.action == AuthoringBindingItem::Action::Set)
            widget->addKeySequence(keySequence, slot);
        item.outcome = AuthoringApplyItem::Outcome::Applied;
        AuthoringFieldChange change;
        change.field = QStringLiteral("keySequence");
        change.before = slotKey.has_value()
            ? slotKey->toString(QKeySequence::PortableText)
            : QVariant(QString());
        change.after = (binding.action == AuthoringBindingItem::Action::Set)
            ? QVariant(keySequence.toString(QKeySequence::PortableText))
            : QVariant(QString());
        change.changed = true;
        change.note = QStringLiteral("applied");
        item.changes.append(change);
        item.changedCount = 1;
        anyApplied = true;
        result.items.append(item);
    }

    result.applied = anyApplied;
    return result;
}

/** MCP action text is the lower-cased canonical action name reported by queries. */
static VCButton::ButtonAction buttonActionFromMcp(const QString &action)
{
    if (action == "flash") return VCButton::Flash;
    if (action == "blackout") return VCButton::Blackout;
    if (action == "stopall") return VCButton::StopAll;
    if (action == "freeze") return VCButton::Freeze;
    if (action == "freezehold") return VCButton::FreezeHold;
    return VCButton::Toggle;
}

QRect VCBridgeV5::snapRect(const QRect &rect) const
{
    if (!m_vc->snapping())
        return rect;
    qreal s = m_vc->snappingSize();
    if (s <= 0)
        return rect;

    int is = qRound(s);
    return QRect(
        qRound((qreal)rect.x() / s) * is,
        qRound((qreal)rect.y() / s) * is,
        qMax(qRound((qreal)rect.width() / s) * is, is),
        qMax(qRound((qreal)rect.height() / s) * is, is)
    );
}

int VCBridgeV5::snappingSize() const
{
    return qRound(m_vc->snappingSize());
}

VCWidget *VCBridgeV5::createGeometryWidget(VCFrame *frame, const QString &type,
                                          const QRect &geometry, int subpage)
{
    const qreal scale = frame->scaleFactor();
    if (!std::isfinite(scale) || scale <= 0)
    {
        qWarning() << "VC creation refused: invalid parent scale" << frame->id();
        return nullptr;
    }
    const QRect display = snapRect(geometry);
    const QVariantMap result = m_vc->createWidgetGeometry(frame->id(), type,
        QRectF(display.x() / scale, display.y() / scale,
               display.width() / scale, display.height() / scale),
        subpage < 0 ? frame->currentPage() : subpage);
    if (!result.value(QStringLiteral("accepted")).toBool())
    {
        qWarning() << "VC creation refused:" << result.value(QStringLiteral("error")).toString();
        return nullptr;
    }
    const QVariantList ids = result.value(QStringLiteral("widgetIDs")).toList();
    if (ids.size() != 1)
    {
        qWarning() << "VC creation returned an invalid widget identity";
        return nullptr;
    }
    return m_vc->widget(ids.first().toUInt());
}

int VCBridgeV5::createWidgetOnPage(int parentID, const QString &type, const QRect &geo,
                                  const QString &caption, int subpage)
{
    VCFrame *frame = qobject_cast<VCFrame *>(m_vc->widget(parentID));
    if (!frame)
        return -1;
    VCWidget *widget = createGeometryWidget(frame, type, geo, subpage);
    if (!widget)
        return -1;
    if (!caption.isNull())
        widget->setCaption(caption);
    return int(widget->id());
}

int VCBridgeV5::addPage(const QString &name)
{
    int idx = m_vc->pagesCount();
    m_vc->addPage(idx);
    VCPage *page = m_vc->page(idx);
    if (page && !name.isEmpty())
        page->setCaption(name);
    return idx;
}

QList<VCBridge::PageInfo> VCBridgeV5::pages() const
{
    QList<PageInfo> result;
    for (int i = 0; i < m_vc->pagesCount(); i++)
    {
        VCPage *page = m_vc->page(i);
        if (!page) continue;
        PageInfo pi;
        pi.index = i;
        pi.name = page->caption().isEmpty()
            ? QString("Page %1").arg(i + 1) : page->caption();
        pi.externalInputMode = VCPage::externalInputModeToString(page->externalInputMode());

        // Populate widgets from the page's children
        for (VCWidget *w : page->children(true))
        {
            WidgetInfo wi;
            wi.id = w->id();
            wi.type = VCWidget::typeToString(w->type());
            wi.machineType = QString::fromStdString(VCType::toString(w->type()));
            wi.caption = w->caption();
            wi.geometry = w->geometry().toRect();
            wi.functionID = Function::invalidId();

            VCButton *btn = qobject_cast<VCButton *>(w);
            if (btn) wi.functionID = btn->functionID();
            VCCueList *cl = qobject_cast<VCCueList *>(w);
            if (cl) wi.functionID = cl->chaserID();

            pi.widgets.append(wi);
        }

        result.append(pi);
    }
    return result;
}

int VCBridgeV5::pagesCount() const
{
    return m_vc->pagesCount();
}

int VCBridgeV5::addFrame(int pageIndex, const QRect &geometry,
                         const QString &caption, bool solo)
{
    VCPage *page = m_vc->page(pageIndex);
    if (!page) return -1;

    VCWidget *widget = createGeometryWidget(page, solo ? "Solo frame" : "Frame", geometry);
    if (!widget) return -1;
    widget->setCaption(caption);
    return widget->id();
}

int VCBridgeV5::addFrameInFrame(int parentID, const QRect &geometry,
                                 const QString &caption, bool solo)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame*>(parent);
    if (!frame) return -1;

    VCWidget *widget = createGeometryWidget(frame, solo ? "Solo frame" : "Frame", geometry);
    if (!widget) return -1;
    widget->setCaption(caption);
    return widget->id();
}

int VCBridgeV5::addButton(int parentID, const QRect &geometry,
                          quint32 functionID, const QString &caption,
                          const QString &action,
                          int stopAllFadeTime)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = createGeometryWidget(frame, "Button", geometry);
    if (!widget) return -1;

    VCButton *button = qobject_cast<VCButton *>(widget);
    if (button)
    {
        button->setCaption(caption);
        if (functionID != Function::invalidId())
            button->setFunctionID(functionID);

        button->setActionType(buttonActionFromMcp(action));
        if (action == "stopall" && stopAllFadeTime > 0)
            button->setStopAllFadeOutTime(stopAllFadeTime);
    }
    return widget->id();
}

int VCBridgeV5::addSlider(int parentID, const QRect &geometry,
                          const QString &mode, const QString &caption,
                          quint32 functionID,
                          const QList<QPair<quint32, quint32>> &channels)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = createGeometryWidget(frame, "Slider", geometry);
    if (!widget) return -1;

    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (slider)
    {
        slider->setCaption(caption);

        if (mode == "submaster")
            slider->setSliderMode(VCSlider::Submaster);
        else if (mode == "playback")
        {
            slider->setSliderMode(VCSlider::Adjust);
            slider->setControlledFunction(functionID);
        }
        else if (mode == "grandmaster")
            slider->setSliderMode(VCSlider::GrandMaster);
        else // "level"
        {
            slider->setSliderMode(VCSlider::Level);
            for (const auto &ch : channels)
                slider->addLevelChannel(ch.first, ch.second);
        }
    }
    return widget->id();
}

int VCBridgeV5::addXYPad(int parentID, const QRect &geometry,
                         const QList<quint32> &fixtureIDs)
{
    // Delegate to addXYPadEx with default config
    QList<XYPadFixtureConfig> configs;
    for (quint32 fxID : fixtureIDs)
    {
        XYPadFixtureConfig cfg;
        cfg.fixtureID = fxID;
        configs.append(cfg);
    }
    return addXYPadEx(parentID, geometry, configs, "degrees", false);
}

int VCBridgeV5::addXYPadEx(int parentID, const QRect &geometry,
                            const QList<XYPadFixtureConfig> &fixtures,
                            const QString &displayMode,
                            bool invertedAppearance)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = createGeometryWidget(frame, "XYPad", geometry);
    if (!widget) return -1;

    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (xyPad)
    {

        for (const XYPadFixtureConfig &cfg : fixtures)
        {
            Fixture *fxi = m_doc->fixture(cfg.fixtureID);
            if (!fxi) continue;

            bool isDefault = (cfg.xMin == 0.0 && cfg.xMax == 1.0
                              && cfg.yMin == 0.0 && cfg.yMax == 1.0
                              && !cfg.xReverse && !cfg.yReverse);

            // Add the fixture/head to the pad
            if (cfg.head == 0 && isDefault)
                xyPad->addFixture(QVariant::fromValue(fxi));
            else
                xyPad->addHead(cfg.fixtureID, cfg.head);

            // Apply custom axis ranges if non-default
            if (!isDefault)
            {
                xyPad->setFixtureRange(cfg.fixtureID, cfg.head,
                                       cfg.xMin, cfg.xMax, cfg.xReverse,
                                       cfg.yMin, cfg.yMax, cfg.yReverse);
            }
        }

        // Set display mode
        if (displayMode == "percentage")
            xyPad->setDisplayMode(VCXYPad::Percentage);
        else if (displayMode == "dmx")
            xyPad->setDisplayMode(VCXYPad::DMX);
        else
            xyPad->setDisplayMode(VCXYPad::Degrees);

        xyPad->setInvertedAppearance(invertedAppearance);
    }
    return widget->id();
}

int VCBridgeV5::addCueList(int parentID, const QRect &geometry,
                           quint32 chaserID, const QString &caption)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = createGeometryWidget(frame, "Cue list", geometry);
    if (!widget) return -1;

    VCCueList *cuelist = qobject_cast<VCCueList *>(widget);
    if (cuelist)
    {
        cuelist->setChaserID(chaserID);
        if (!caption.isEmpty())
            cuelist->setCaption(caption);
    }
    return widget->id();
}

int VCBridgeV5::addLabel(int parentID, const QRect &geometry,
                         const QString &text)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = createGeometryWidget(frame, "Label", geometry);
    if (!widget) return -1;

    widget->setCaption(text);
    return widget->id();
}

bool VCBridgeV5::mapWidgetInput(int widgetID, quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    // Remove all existing input sources to prevent duplicates on re-run
    while (!widget->inputSources().isEmpty())
    {
        auto source = widget->inputSources().first();
        widget->deleteInputSurce(source->id(), source->universe(), source->channel());
    }

    m_vc->createAndAddInputSource(widget, universe, channel);
    return true;
}

bool VCBridgeV5::setWidgetFeedback(int widgetID,
                                    int idleValue, int activeValue, int monitorValue,
                                    int idleMidiCh, int activeMidiCh, int monitorMidiCh)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    for (auto &source : widget->inputSources())
    {
        source->setFeedbackValue(QLCInputFeedback::LowerValue, (uchar)idleValue);
        source->setFeedbackValue(QLCInputFeedback::UpperValue, (uchar)activeValue);
        source->setFeedbackValue(QLCInputFeedback::MonitorValue, (uchar)monitorValue);
        source->setFeedbackExtraParams(QLCInputFeedback::LowerValue, QVariant(idleMidiCh));
        source->setFeedbackExtraParams(QLCInputFeedback::UpperValue, QVariant(activeMidiCh));
        source->setFeedbackExtraParams(QLCInputFeedback::MonitorValue, QVariant(monitorMidiCh));
        return true;
    }
    return false;
}

VCBridge::FeedbackInfo VCBridgeV5::getWidgetFeedback(int widgetID) const
{
    FeedbackInfo fb;
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget || widget->inputSources().isEmpty())
        return fb;

    auto src = widget->inputSources().first();
    fb.idleValue = src->feedbackValue(QLCInputFeedback::LowerValue);
    fb.activeValue = src->feedbackValue(QLCInputFeedback::UpperValue);
    fb.monitorValue = src->feedbackValue(QLCInputFeedback::MonitorValue);
    fb.idleMidiCh = src->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
    fb.activeMidiCh = src->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
    fb.monitorMidiCh = src->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
    return fb;
}

int VCBridgeV5::widgetInputSourceCount(int widgetID) const
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return 0;
    return widget->inputSources().size();
}

QList<VCBridge::SourceDef> VCBridgeV5::getWidgetSourceDefs(int widgetID) const
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return {};

    QString typeName = QString::fromLatin1(widget->metaObject()->className());
    auto result = sourceDefTable.value(typeName);

    // For multipage frames, add dynamic shortcut entries
    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (frame && frame->totalPagesNumber() > 1)
    {
        for (int i = 0; i < frame->totalPagesNumber(); i++)
            result.append({QStringLiteral("shortcut%1").arg(i),
                          quint32(INPUT_SHORTCUT_BASE_ID + i),
                          QStringLiteral("Jump to page %1").arg(i)});
    }
    return result;
}

bool VCBridgeV5::setWidgetFeedbackByName(int widgetID, const QString &sourceName,
                                          int idleVal, int activeVal, int monitorVal,
                                          int idleCh, int activeCh, int monitorCh)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    int controlId = resolveControlId(widget, sourceName);
    if (controlId < 0) return false;

    for (auto &src : widget->inputSources())
    {
        if (src->id() == (quint32)controlId)
        {
            src->setFeedbackValue(QLCInputFeedback::LowerValue, (uchar)idleVal);
            src->setFeedbackValue(QLCInputFeedback::UpperValue, (uchar)activeVal);
            src->setFeedbackValue(QLCInputFeedback::MonitorValue, (uchar)monitorVal);
            src->setFeedbackExtraParams(QLCInputFeedback::LowerValue, QVariant(idleCh));
            src->setFeedbackExtraParams(QLCInputFeedback::UpperValue, QVariant(activeCh));
            src->setFeedbackExtraParams(QLCInputFeedback::MonitorValue, QVariant(monitorCh));
            return true;
        }
    }
    return false;
}

VCBridge::FeedbackInfo VCBridgeV5::getWidgetFeedbackByName(int widgetID, const QString &sourceName) const
{
    FeedbackInfo fb;
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return fb;

    int controlId = resolveControlId(widget, sourceName);
    if (controlId < 0) return fb;

    for (const auto &src : widget->inputSources())
    {
        if (src->id() == (quint32)controlId)
        {
            fb.idleValue = src->feedbackValue(QLCInputFeedback::LowerValue);
            fb.activeValue = src->feedbackValue(QLCInputFeedback::UpperValue);
            fb.monitorValue = src->feedbackValue(QLCInputFeedback::MonitorValue);
            fb.idleMidiCh = src->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
            fb.activeMidiCh = src->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
            fb.monitorMidiCh = src->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
            return fb;
        }
    }
    return fb;
}

bool VCBridgeV5::setWidgetColors(int widgetID, const QColor &bgColor, const QColor &fgColor)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    if (bgColor.isValid())
        widget->setBackgroundColor(bgColor);
    if (fgColor.isValid())
        widget->setForegroundColor(fgColor);
    return true;
}

int VCBridgeV5::addSpeedDial(int parentID, const QRect &geometry,
                              const QList<quint32> &functionIDs)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = createGeometryWidget(frame, "Speed", geometry);
    if (!widget) return -1;

    VCSpeedDial *speedDial = qobject_cast<VCSpeedDial *>(widget);
    if (speedDial)
    {
        for (quint32 fid : functionIDs)
            speedDial->addFunction(fid);
    }
    return widget->id();
}

int VCBridgeV5::addAudioTriggers(int parentID, const QRect &geometry)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = createGeometryWidget(frame, "Audio Triggers", geometry);
    if (!widget) return -1;

    return widget->id();
}

bool VCBridgeV5::configureAudioTriggerBar(int widgetID, const AudioBarConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (!at) return false;

    if (config.barIndex < 0 || config.barIndex >= at->barsNumber())
        return false;

    at->selectBarForEditing(config.barIndex);

    // Set bar type
    VCAudioTriggers::BarType barType = VCAudioTriggers::None;
    if (config.type == "dmx") barType = VCAudioTriggers::DMXBar;
    else if (config.type == "function") barType = VCAudioTriggers::FunctionBar;
    else if (config.type == "widget") barType = VCAudioTriggers::VCWidgetBar;
    at->setBarType(barType);

    // Set thresholds
    at->setBarThresholds(config.minThreshold, config.maxThreshold);

    // Set type-specific assignments
    if (barType == VCAudioTriggers::FunctionBar && config.functionID != (quint32)-1)
        at->setBarFunction(config.functionID);
    else if (barType == VCAudioTriggers::VCWidgetBar && config.widgetID != (quint32)-1)
        at->setBarWidget(config.widgetID);
    else if (barType == VCAudioTriggers::DMXBar && !config.dmxChannels.isEmpty())
    {
        QList<SceneValue> svList;
        for (const auto &ch : config.dmxChannels)
            svList.append(SceneValue(ch.first, ch.second));
        at->setBarDmxChannels(svList);
    }

    return true;
}

bool VCBridgeV5::setAudioTriggerCapture(int widgetID, bool enabled)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (!at) return false;
    at->setCaptureEnabled(enabled);
    return true;
}

bool VCBridgeV5::setAudioTriggerVolume(int widgetID, int volume)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (!at) return false;
    at->setVolumeLevel(qBound(0, volume, 255));
    return true;
}

bool VCBridgeV5::setAudioTriggerBarsNumber(int widgetID, int count)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (!at) return false;
    if (count < 1) count = 1; // minimum: volume bar
    at->setBarsNumber(count);
    return true;
}

int VCBridgeV5::addClock(int parentID, const QRect &geometry,
                          const QString &clockType)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = createGeometryWidget(frame, "Clock", geometry);
    if (!widget) return -1;

    VCClock *clock = qobject_cast<VCClock *>(widget);
    if (clock)
    {
        if (clockType == "stopwatch")
            clock->setClockType(VCClock::Stopwatch);
        else if (clockType == "countdown")
            clock->setClockType(VCClock::Countdown);
        else
            clock->setClockType(VCClock::Clock);
    }
    return widget->id();
}

int VCBridgeV5::addRecordPanel(int parentID, const QRect &geometry)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = createGeometryWidget(frame, "Record Panel", geometry);
    if (!widget) return -1;

    return widget->id();
}

int VCBridgeV5::findPageByName(const QString &name) const
{
    for (int i = 0; i < m_vc->pagesCount(); i++)
    {
        VCPage *page = m_vc->page(i);
        if (page && page->caption() == name)
            return i;
    }
    return -1;
}

int VCBridgeV5::findWidgetByCaption(int parentID, const QString &widgetType,
                                     const QString &caption) const
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    const int targetType = VCType::fromString(widgetType.toStdString());

    for (VCWidget *w : frame->children())
    {
        if (w->type() == targetType && w->caption() == caption)
            return w->id();
    }
    return -1;
}

QRect VCBridgeV5::nextWidgetPosition(int parentID, int width, int height) const
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame)
        return QRect(5, 5, width, height);

    const int pad = 5;
    int maxBottom = 40; // leave space for frame header
    for (VCWidget *w : frame->children())
    {
        int bottom = (int)w->geometry().y() + (int)w->geometry().height() + pad;
        if (bottom > maxBottom)
            maxBottom = bottom;
    }
    return QRect(pad, maxBottom, width, height);
}

QRect VCBridgeV5::nextWidgetPositionFlow(int parentID, int widgetWidth, int widgetHeight,
                                          int columns) const
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame)
        return QRect(5, 5, widgetWidth, widgetHeight);

    int parentWidth = (int)frame->geometry().width();
    int childCount = frame->children().count();

    return computeFlowPosition(parentWidth, 40, childCount, widgetWidth, widgetHeight, columns, 5);
}

void VCBridgeV5::setWidgetGeometry(int widgetID, const QRect &geo)
{
    const QString error = setWidgetGeometryChecked(widgetID, geo);
    if (!error.isEmpty())
        qWarning() << "VC geometry edit refused:" << error;
}

QString VCBridgeV5::setWidgetGeometryChecked(int widgetID, const QRect &geo)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget)
        return QStringLiteral("Widget %1 not found").arg(widgetID);
    const qreal scale = widget->scaleFactor();
    if (!std::isfinite(scale) || scale <= 0)
        return QStringLiteral("Widget %1 has an invalid scale").arg(widgetID);
    const QRect display = snapRect(geo);
    const QVariantMap result = m_vc->applyGeometryPlan({
        {quint32(widgetID), QRectF(display.x() / scale, display.y() / scale,
                                  display.width() / scale, display.height() / scale)}}, false);
    return result.value(QStringLiteral("accepted")).toBool()
        ? QString() : result.value(QStringLiteral("error")).toString();
}

bool VCBridgeV5::removeWidget(int widgetID)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    m_vc->deleteVCWidgets(QVariantList{widgetID});
    return true;
}

// --- Widget details query ---

static QString multiplierToString(VCSpeedDial::SpeedMultiplier m)
{
    switch (m)
    {
        case VCSpeedDial::Zero: return "0";
        case VCSpeedDial::OneSixteenth: return "1/16";
        case VCSpeedDial::OneEighth: return "1/8";
        case VCSpeedDial::OneFourth: return "1/4";
        case VCSpeedDial::Half: return "1/2";
        case VCSpeedDial::One: return "1";
        case VCSpeedDial::Two: return "2";
        case VCSpeedDial::Four: return "4";
        case VCSpeedDial::Eight: return "8";
        case VCSpeedDial::Sixteen: return "16";
        default: return "none";
    }
}

VCBridge::WidgetDetails VCBridgeV5::getWidgetDetails(int widgetID) const
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return WidgetDetails();

    WidgetDetails d;
    d.id = widgetID;
    d.type = VCWidget::typeToString(widget->type());
    d.machineType = QString::fromStdString(VCType::toString(widget->type()));
    d.caption = widget->caption();
    d.geometry = widget->geometry().toRect();
    d.bgColor = widget->backgroundColor();
    d.fgColor = widget->foregroundColor();

    // Parent ID
    VCWidget *parent = qobject_cast<VCWidget*>(widget->parent());
    if (parent)
        d.parentID = (int)parent->id();
    d.childPageIndex = widget->page();

    // Input mappings with per-source feedback
    for (const auto &src : widget->inputSources())
    {
        InputMapping m;
        m.universe = src->universe();
        m.channel = src->channel();
        m.sourceId = src->id();
        m.sourceName = resolveSourceName(widget, src->id());
        m.feedback.idleValue = src->feedbackValue(QLCInputFeedback::LowerValue);
        m.feedback.activeValue = src->feedbackValue(QLCInputFeedback::UpperValue);
        m.feedback.monitorValue = src->feedbackValue(QLCInputFeedback::MonitorValue);
        m.feedback.idleMidiCh = src->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
        m.feedback.activeMidiCh = src->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
        m.feedback.monitorMidiCh = src->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
        d.inputMappings.append(m);
    }

    // Valid source definitions for this widget type
    d.validSources = getWidgetSourceDefs(widgetID);

    // Button-specific
    VCButton *button = qobject_cast<VCButton*>(widget);
    if (button)
    {
        d.functionID = button->functionID();
        d.action = VCButton::actionToString(button->actionType()).toLower();
        d.buttonState = (int)button->state();
        d.startupIntensityEnabled = button->startupIntensityEnabled();
        d.startupIntensity = button->startupIntensity();
        d.flashOverride = button->flashOverrides();
        d.flashForceLTP = button->flashForceLTP();
        d.stopAllFadeTime = button->stopAllFadeOutTime();
    }

    // Slider-specific
    VCSlider *slider = qobject_cast<VCSlider*>(widget);
    if (slider)
    {
        switch (slider->sliderMode())
        {
            case VCSlider::Level: d.sliderMode = "level"; break;
            case VCSlider::Adjust: d.sliderMode = "playback"; break;
            case VCSlider::Submaster: d.sliderMode = "submaster"; break;
            case VCSlider::GrandMaster: d.sliderMode = "grandmaster"; break;
        }
        d.functionID = slider->controlledFunction();
        for (auto &sv : slider->levelChannels())
            d.channels.append(qMakePair(sv.fxi, sv.channel));

        // Extended slider properties
        switch (slider->clickAndGoType())
        {
            case VCSlider::CnGNone: d.clickAndGoType = "none"; break;
            case VCSlider::CnGColors: d.clickAndGoType = "colors"; break;
            case VCSlider::CnGPreset: d.clickAndGoType = "preset"; break;
        }
        switch (slider->valueDisplayStyle())
        {
            case VCSlider::DMXValue: d.valueDisplayStyle = "dmx"; break;
            case VCSlider::PercentageValue: d.valueDisplayStyle = "percentage"; break;
        }
        d.sliderInvertedAppearance = slider->invertedAppearance();
        d.rangeLowLimit = slider->rangeLowLimit();
        d.rangeHighLimit = slider->rangeHighLimit();
        d.monitorEnabled = slider->monitorEnabled();

        if (slider->sliderMode() == VCSlider::GrandMaster)
        {
            auto gmValMode = slider->grandMasterValueMode();
            d.gmValueMode = (gmValMode == GrandMaster::Reduce) ? "reduce" : "limit";
            auto gmChMode = slider->grandMasterChannelMode();
            d.gmChannelMode = (gmChMode == GrandMaster::AllChannels) ? "allchannels" : "intensity";
        }

        // Widget style and catch values
        d.widgetStyle = (slider->widgetStyle() == VCSlider::WKnob) ? "knob" : "slider";
        d.catchValues = slider->catchValues();
    }

    // CueList-specific
    VCCueList *cuelist = qobject_cast<VCCueList*>(widget);
    if (cuelist)
    {
        d.functionID = cuelist->chaserID();
        switch (cuelist->nextPrevBehavior())
        {
            case VCCueList::DefaultRunFirst: d.nextPrevBehavior = "defaultRunFirst"; break;
            case VCCueList::RunNext: d.nextPrevBehavior = "runNext"; break;
            case VCCueList::Select: d.nextPrevBehavior = "select"; break;
            case VCCueList::Nothing: d.nextPrevBehavior = "nothing"; break;
        }
        switch (cuelist->playbackLayout())
        {
            case VCCueList::PlayPauseStop: d.playbackLayout = "playPauseStop"; break;
            case VCCueList::PlayStopPause: d.playbackLayout = "playStopPause"; break;
        }
        switch (cuelist->sideFaderMode())
        {
            case VCCueList::None: d.sideFaderMode = "none"; break;
            case VCCueList::Crossfade: d.sideFaderMode = "crossfade"; break;
            case VCCueList::Steps: d.sideFaderMode = "steps"; break;
        }
    }

    // XYPad-specific
    VCXYPad *xyPad = qobject_cast<VCXYPad*>(widget);
    if (xyPad)
    {
        switch (xyPad->displayMode())
        {
            case VCXYPad::Percentage: d.displayMode = "percentage"; break;
            case VCXYPad::Degrees: d.displayMode = "degrees"; break;
            case VCXYPad::DMX: d.displayMode = "dmx"; break;
        }
        d.invertedAppearance = xyPad->invertedAppearance();
        d.xyPadPosition = xyPad->currentPosition();

        for (const auto &fxItem : xyPad->fixtures())
        {
            XYPadFixtureInfo info;
            info.fixtureID = fxItem.m_head.fxi;
            info.head = fxItem.m_head.head;
            info.xMin = fxItem.m_xMin;
            info.xMax = fxItem.m_xMax;
            info.xReverse = fxItem.m_xReverse;
            info.yMin = fxItem.m_yMin;
            info.yMax = fxItem.m_yMax;
            info.yReverse = fxItem.m_yReverse;

            Fixture *fxi = m_doc->fixture(info.fixtureID);
            if (fxi)
            {
                info.name = fxi->name();
                QRectF degRange = fxi->degreesRange(info.head);
                if (!degRange.isNull())
                {
                    info.panDegreesMax = degRange.width();
                    info.tiltDegreesMax = degRange.height();
                }
            }
            d.xyPadFixtures.append(info);
        }

        // XY Pad presets
        for (const QVariant &v : xyPad->presetsList())
        {
            QVariantMap pm = v.toMap();
            XYPadPresetInfo pi;
            pi.name = pm.value("name").toString();
            pi.functionID = pm.value("functionID").toUInt();
            pi.type = pm.value("typeString").toString().toLower();
            d.xyPadPresets.append(pi);
        }
    }

    // AudioTriggers-specific
    VCAudioTriggers *audioTrig = qobject_cast<VCAudioTriggers*>(widget);
    if (audioTrig)
    {
        d.captureEnabled = audioTrig->captureEnabled();
        d.volumeLevel = audioTrig->volumeLevel();
        d.barsNumber = audioTrig->barsNumber();

        QVariantList bInfo = audioTrig->barsInfo();
        for (const QVariant &v : bInfo)
        {
            QVariantMap bm = v.toMap();
            WidgetDetails::AudioBarInfo bar;
            bar.barIndex = bm.value("index").toInt();
            bar.sourceKey = bm.value("sourceKey").toString();
            bar.label = bm.value("bLabel").toString();
            bar.color = bm.value("color").toString();
            int bType = bm.value("type").toInt();
            switch (bType)
            {
                case VCAudioTriggers::DMXBar: bar.type = "dmx"; break;
                case VCAudioTriggers::FunctionBar: bar.type = "function"; break;
                case VCAudioTriggers::VCWidgetBar: bar.type = "widget"; break;
                default: bar.type = "none"; break;
            }
            bar.minThreshold = bm.value("minThreshold").toInt();
            bar.maxThreshold = bm.value("maxThreshold").toInt();

            if (bType == VCAudioTriggers::FunctionBar)
            {
                int fid = bm.value("intVal").toInt();
                if (fid >= 0)
                {
                    bar.functionID = fid;
                    Function *fn = m_doc->function(fid);
                    if (fn) bar.functionName = fn->name();
                }
            }
            else if (bType == VCAudioTriggers::VCWidgetBar)
            {
                int wid = bm.value("intVal").toInt();
                if (wid >= 0)
                {
                    bar.widgetID = wid;
                    bar.widgetName = bm.value("strVal").toString();
                }
            }

            d.audioBars.append(bar);
        }
    }

    // Frame-specific
    VCFrame *frame = qobject_cast<VCFrame*>(widget);
    if (frame && !qobject_cast<VCPage*>(widget))
    {
        d.multipageMode = frame->multiPageMode();
        d.totalPages = frame->totalPagesNumber();
        d.currentPage = frame->currentPage();
        d.pagesLoop = frame->pagesLoop();
        d.pageLabels = frame->pageLabels();
        d.headerVisible = frame->showHeader();
        d.enableButtonVisible = frame->showEnable();
        d.collapsed = frame->isCollapsed();

        // Grid layout properties
        switch (frame->layoutMode())
        {
            case VCFrame::LayoutGrid: d.gridLayoutMode = "grid"; break;
            default:                  d.gridLayoutMode = "free"; break;
        }
        d.gridColumns = frame->gridColumns();
        d.gridRowHeight = frame->gridRowHeight();
        d.gridCompact = frame->gridCompact();

        VCSoloFrame *soloFrame = qobject_cast<VCSoloFrame*>(widget);
        if (soloFrame)
        {
            d.soloframeMixing = soloFrame->soloframeMixing();
            d.excludeMonitoredFunctions = soloFrame->excludeMonitoredFunctions();
        }
    }

    // Animation/Matrix-specific
    VCAnimation *animation = qobject_cast<VCAnimation*>(widget);
    if (animation)
    {
        d.functionID = animation->functionID();
        d.matrixColor1 = animation->getColor1();
        d.matrixColor2 = animation->getColor2();
        d.matrixColor3 = animation->getColor3();
        d.matrixColor4 = animation->getColor4();
        d.matrixColor5 = animation->getColor5();

        // Build dynamic colors array and count valid colors
        QColor allColors[5] = { d.matrixColor1, d.matrixColor2, d.matrixColor3, d.matrixColor4, d.matrixColor5 };
        for (int i = 0; i < 5; i++)
        {
            if (allColors[i].isValid())
                d.matrixColors.append(allColors[i]);
        }
        d.matrixColorCount = d.matrixColors.size();

        QStringList algos = animation->algorithms();
        int algIdx = animation->algorithmIndex();
        if (algIdx >= 0 && algIdx < algos.size())
            d.matrixAnimation = algos.at(algIdx);
        d.matrixInstantApply = animation->instantChanges();
        d.matrixVisibilityMask = animation->visibilityMask();
    }

    // Clock-specific
    VCClock *clock = qobject_cast<VCClock*>(widget);
    if (clock)
    {
        switch (clock->clockType())
        {
            case VCClock::Clock: d.clockType = "clock"; break;
            case VCClock::Stopwatch: d.clockType = "stopwatch"; break;
            case VCClock::Countdown: d.clockType = "countdown"; break;
        }
        int targetMs = clock->targetTime();
        int totalSecs = targetMs / 1000;
        d.countdownH = totalSecs / 3600;
        d.countdownM = (totalSecs % 3600) / 60;
        d.countdownS = totalSecs % 60;

        for (VCClockSchedule *sched : clock->schedules())
        {
            ClockScheduleInfo csi;
            csi.functionID = sched->functionID();
            int startSecs = sched->startTime();
            csi.hour = startSecs / 3600;
            csi.minute = (startSecs % 3600) / 60;
            csi.second = startSecs % 60;
            d.clockSchedules.append(csi);
        }
    }

    // SpeedDial-specific
    VCSpeedDial *speedDial = qobject_cast<VCSpeedDial*>(widget);
    if (speedDial)
    {
        QMap<quint32, VCSpeedDial::VCSpeedDialFunction> funcs = speedDial->functions();
        for (auto it = funcs.constBegin(); it != funcs.constEnd(); ++it)
        {
            SpeedDialFunctionInfo fi;
            fi.functionID = it.value().m_fId;
            fi.fadeInMultiplier = multiplierToString(it.value().m_fadeInFactor);
            fi.fadeOutMultiplier = multiplierToString(it.value().m_fadeOutFactor);
            fi.durationMultiplier = multiplierToString(it.value().m_durationFactor);
            d.speedDialFunctions.append(fi);
        }
        for (const QVariant &v : speedDial->presetsList())
        {
            QVariantMap pm = v.toMap();
            SpeedDialPresetInfo pi;
            pi.name = pm.value("name").toString();
            pi.value = pm.value("value").toInt();
            d.speedDialPresets.append(pi);
        }
        d.absoluteValueMin = speedDial->timeMinimumValue();
        d.absoluteValueMax = speedDial->timeMaximumValue();
        d.speedDialVisibilityMask = speedDial->visibilityMask();
        d.resetFactorOnDialChange = speedDial->resetOnDialChange();
    }

    // RecordPanel-specific
    VCRecordPanel *recordPanel = qobject_cast<VCRecordPanel*>(widget);
    if (recordPanel)
    {
        d.rpTargetFolder = recordPanel->targetFolder();
        d.rpScenePrefix = recordPanel->scenePrefix();
        d.rpChaserPrefix = recordPanel->chaserPrefix();
        d.rpDefaultFadeIn = recordPanel->defaultFadeIn();
        d.rpDefaultHold = recordPanel->defaultHold();
        d.rpDefaultFadeOut = recordPanel->defaultFadeOut();
        d.rpIsRecordingChaser = recordPanel->isRecordingChaser();
    }

    // Base widget extended properties
    if (widget->hasCustomFont())
    {
        QFont f = widget->font();
        d.fontConfig.family = f.family();
        d.fontConfig.pointSize = f.pointSize();
        d.fontConfig.bold = f.bold();
        d.fontConfig.italic = f.italic();
    }
    d.backgroundImage = widget->backgroundImage();
    d.disabled = widget->isDisabled();

    return d;
}

bool VCBridgeV5::setWidgetPage(int widgetID, int pageIndex)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget)
        return false;
    VCFrame *frame = qobject_cast<VCFrame *>(widget->parent());
    if (!frame || pageIndex < 0 || pageIndex >= frame->totalPagesNumber())
        return false;
    if (widget->page() == pageIndex)
        return true;
    const QVariantMap result = m_vc->editStructure({widgetID}, frame->id(), pageIndex);
    if (!result.value(QStringLiteral("accepted")).toBool())
        qWarning() << "VC subpage edit refused:" << result.value(QStringLiteral("error")).toString();
    return result.value(QStringLiteral("accepted")).toBool();
}

// --- Widget property mutations ---

bool VCBridgeV5::setWidgetCaption(int widgetID, const QString &caption)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;
    widget->setCaption(caption);
    return true;
}

bool VCBridgeV5::setButtonFunction(int widgetID, quint32 functionID)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCButton *button = qobject_cast<VCButton*>(widget);
    if (!button) return false;
    button->setFunctionID(functionID);
    return true;
}

bool VCBridgeV5::setButtonAction(int widgetID, const QString &action)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCButton *button = qobject_cast<VCButton*>(widget);
    if (!button) return false;

    button->setActionType(buttonActionFromMcp(action));
    return true;
}

bool VCBridgeV5::setSliderMode(int widgetID, const QString &mode)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider*>(widget);
    if (!slider) return false;

    if (mode == "level") slider->setSliderMode(VCSlider::Level);
    else if (mode == "playback") slider->setSliderMode(VCSlider::Adjust);
    else if (mode == "submaster") slider->setSliderMode(VCSlider::Submaster);
    else if (mode == "grandmaster") slider->setSliderMode(VCSlider::GrandMaster);
    else return false;
    return true;
}

bool VCBridgeV5::configureSlider(int widgetID, const SliderConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider*>(widget);
    if (!slider) return false;

    if (config.clickAndGoType.has_value())
    {
        const QString &v = config.clickAndGoType.value();
        if (v == "colors" || v == "rgb" || v == "cmy") slider->setClickAndGoType(VCSlider::CnGColors);
        else if (v == "preset") slider->setClickAndGoType(VCSlider::CnGPreset);
        else slider->setClickAndGoType(VCSlider::CnGNone);
    }
    if (config.valueDisplayStyle.has_value())
    {
        const QString &v = config.valueDisplayStyle.value();
        if (v == "percentage") slider->setValueDisplayStyle(VCSlider::PercentageValue);
        else slider->setValueDisplayStyle(VCSlider::DMXValue);
    }
    if (config.invertedAppearance.has_value())
        slider->setInvertedAppearance(config.invertedAppearance.value());
    if (config.rangeLowLimit.has_value())
        slider->setRangeLowLimit(config.rangeLowLimit.value());
    if (config.rangeHighLimit.has_value())
        slider->setRangeHighLimit(config.rangeHighLimit.value());
    if (config.monitorEnabled.has_value())
        slider->setMonitorEnabled(config.monitorEnabled.value());
    if (config.gmValueMode.has_value())
    {
        const QString &v = config.gmValueMode.value();
        slider->setGrandMasterValueMode(
            v == "reduce" ? GrandMaster::Reduce : GrandMaster::Limit);
    }
    if (config.gmChannelMode.has_value())
    {
        const QString &v = config.gmChannelMode.value();
        slider->setGrandMasterChannelMode(
            v == "allchannels" ? GrandMaster::AllChannels : GrandMaster::Intensity);
    }
    return true;
}

bool VCBridgeV5::setSliderFunction(int widgetID, quint32 functionID)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider*>(widget);
    if (!slider) return false;
    slider->setControlledFunction(functionID);
    return true;
}

bool VCBridgeV5::setSliderChannels(int widgetID, const QList<QPair<quint32, quint32>> &channels)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider*>(widget);
    if (!slider) return false;

    slider->clearLevelChannels();
    for (auto &ch : channels)
        slider->addLevelChannel(ch.first, ch.second);
    return true;
}

// --- Input mapping ---

bool VCBridgeV5::addWidgetInput(int widgetID, quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;
    m_vc->createAndAddInputSource(widget, universe, channel);
    return true;
}

bool VCBridgeV5::removeWidgetInput(int widgetID, quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    for (auto &src : widget->inputSources())
    {
        if (src->universe() == universe && src->channel() == channel)
        {
            widget->deleteInputSurce(src->id(), universe, channel);
            return true;
        }
    }
    return false;
}

// --- XY Pad property mutations ---

bool VCBridgeV5::setXYPadPosition(int widgetID, qreal x, qreal y)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;
    xyPad->setCurrentPosition(QPointF(x, y));
    return true;
}

bool VCBridgeV5::setXYPadDisplayMode(int widgetID, const QString &mode)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;

    if (mode == "percentage")
        xyPad->setDisplayMode(VCXYPad::Percentage);
    else if (mode == "dmx")
        xyPad->setDisplayMode(VCXYPad::DMX);
    else if (mode == "degrees")
        xyPad->setDisplayMode(VCXYPad::Degrees);
    else
        return false;
    return true;
}

bool VCBridgeV5::setXYPadInvertedAppearance(int widgetID, bool inverted)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;
    xyPad->setInvertedAppearance(inverted);
    return true;
}

bool VCBridgeV5::addXYPadFixture(int widgetID, const XYPadFixtureConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;

    Fixture *fxi = m_doc->fixture(config.fixtureID);
    if (!fxi) return false;

    xyPad->addHead(config.fixtureID, config.head);
    xyPad->setFixtureRange(config.fixtureID, config.head,
                           config.xMin, config.xMax, config.xReverse,
                           config.yMin, config.yMax, config.yReverse);
    return true;
}

bool VCBridgeV5::removeXYPadFixture(int widgetID, quint32 fixtureID, int head)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;
    return xyPad->removeHead(fixtureID, head);
}

// --- Widget reparenting ---

bool VCBridgeV5::reparentWidget(int widgetID, int newParentID, const QRect &geo)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCWidget *newParent = m_vc->widget(newParentID);
    if (!widget || !newParent) return false;

    VCFrame *targetFrame = qobject_cast<VCFrame*>(newParent);
    if (!targetFrame) return false;

    const qreal scale = targetFrame->scaleFactor();
    if (!std::isfinite(scale) || scale <= 0)
        return false;
    const QRect display = snapRect(geo);
    const QVariantMap result = m_vc->editDrop(widgetID, newParentID, targetFrame->currentPage(),
        QRectF(display.x() / scale, display.y() / scale,
               display.width() / scale, display.height() / scale));
    if (!result.value(QStringLiteral("accepted")).toBool())
        qWarning() << "VC reparent refused:" << result.value(QStringLiteral("error")).toString();
    return result.value(QStringLiteral("accepted")).toBool();
}

// --- Matrix widget ---

int VCBridgeV5::addMatrix(int parentID, const QRect &geometry,
                          quint32 functionID, const QString &caption)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = createGeometryWidget(frame, "Animation", geometry);
    if (!widget) return -1;

    VCAnimation *animation = qobject_cast<VCAnimation *>(widget);
    if (animation)
    {
        if (!caption.isEmpty())
            animation->setCaption(caption);
        if (functionID != Function::invalidId())
            animation->setFunctionID(functionID);
    }
    return widget->id();
}

bool VCBridgeV5::configureMatrix(int widgetID, const MatrixConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAnimation *animation = qobject_cast<VCAnimation *>(widget);
    if (!animation) return false;

    if (config.functionID.has_value())
        animation->setFunctionID(config.functionID.value());
    if (config.colors.has_value())
    {
        const auto &cv = config.colors.value();
        // Map dynamic array to hardcoded setColor1–5
        if (cv.size() > 0) animation->setColor1(cv.at(0));
        if (cv.size() > 1) animation->setColor2(cv.at(1));
        if (cv.size() > 2) animation->setColor3(cv.at(2));
        if (cv.size() > 3) animation->setColor4(cv.at(3));
        if (cv.size() > 4) animation->setColor5(cv.at(4));
    }
    else
    {
        if (config.color1.has_value())
            animation->setColor1(config.color1.value());
        if (config.color2.has_value())
            animation->setColor2(config.color2.value());
        if (config.color3.has_value())
            animation->setColor3(config.color3.value());
        if (config.color4.has_value())
            animation->setColor4(config.color4.value());
        if (config.color5.has_value())
            animation->setColor5(config.color5.value());
    }
    if (config.animation.has_value())
    {
        QStringList algos = animation->algorithms();
        int idx = algos.indexOf(config.animation.value());
        if (idx >= 0)
            animation->setAlgorithmIndex(idx);
    }
    if (config.instantApply.has_value())
        animation->setInstantChanges(config.instantApply.value());
    if (config.visibilityMask.has_value())
        animation->setVisibilityMask(config.visibilityMask.value());
    return true;
}

// --- Button extended config ---

bool VCBridgeV5::configureButton(int widgetID, const ButtonConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCButton *button = qobject_cast<VCButton *>(widget);
    if (!button) return false;

    if (config.functionID.has_value())
        button->setFunctionID(config.functionID.value());
    if (config.action.has_value())
        button->setActionType(buttonActionFromMcp(config.action.value()));
    if (config.startupIntensityEnabled.has_value())
        button->setStartupIntensityEnabled(config.startupIntensityEnabled.value());
    if (config.startupIntensity.has_value())
        button->setStartupIntensity(config.startupIntensity.value());
    if (config.flashOverride.has_value())
        button->setFlashOverride(config.flashOverride.value());
    if (config.flashForceLTP.has_value())
        button->setFlashForceLTP(config.flashForceLTP.value());
    if (config.stopAllFadeTime.has_value())
        button->setStopAllFadeOutTime(config.stopAllFadeTime.value());
    return true;
}

// --- Frame extended config ---

bool VCBridgeV5::configureFrame(int widgetID, const FrameConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (!frame) return false;

    if (config.multipageMode.has_value())
        frame->setMultiPageMode(config.multipageMode.value());
    if (config.totalPages.has_value())
        frame->setTotalPagesNumber(config.totalPages.value());
    if (config.pagesLoop.has_value())
        frame->setPagesLoop(config.pagesLoop.value());
    if (config.pageLabels.has_value())
    {
        const QStringList &labels = config.pageLabels.value();
        for (int i = 0; i < labels.size() && i < frame->totalPagesNumber(); i++)
            frame->setShortcutName(i, labels.at(i));
    }
    if (config.headerVisible.has_value())
        frame->setShowHeader(config.headerVisible.value());
    if (config.enableButtonVisible.has_value())
        frame->setShowEnable(config.enableButtonVisible.value());
    if (config.collapsed.has_value())
        frame->setCollapsed(config.collapsed.value());

    // SoloFrame-specific properties
    VCSoloFrame *soloFrame = qobject_cast<VCSoloFrame *>(widget);
    if (soloFrame)
    {
        if (config.soloframeMixing.has_value())
            soloFrame->setSoloframeMixing(config.soloframeMixing.value());
        if (config.excludeMonitoredFunctions.has_value())
            soloFrame->setExcludeMonitoredFunctions(config.excludeMonitoredFunctions.value());
    }
    return true;
}

// --- CueList extended config ---

bool VCBridgeV5::configureCueList(int widgetID, const CueListConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCCueList *cuelist = qobject_cast<VCCueList *>(widget);
    if (!cuelist) return false;

    if (config.chaserID.has_value())
        cuelist->setChaserID(config.chaserID.value());
    if (config.nextPrevBehavior.has_value())
    {
        const QString &v = config.nextPrevBehavior.value();
        if (v == "runNext") cuelist->setNextPrevBehavior(VCCueList::RunNext);
        else if (v == "select") cuelist->setNextPrevBehavior(VCCueList::Select);
        else if (v == "nothing") cuelist->setNextPrevBehavior(VCCueList::Nothing);
        else cuelist->setNextPrevBehavior(VCCueList::DefaultRunFirst);
    }
    if (config.playbackLayout.has_value())
    {
        const QString &v = config.playbackLayout.value();
        if (v == "playStopPause") cuelist->setPlaybackLayout(VCCueList::PlayStopPause);
        else cuelist->setPlaybackLayout(VCCueList::PlayPauseStop);
    }
    if (config.sideFaderMode.has_value())
    {
        const QString &v = config.sideFaderMode.value();
        if (v == "crossfade") cuelist->setSideFaderMode(VCCueList::Crossfade);
        else if (v == "steps") cuelist->setSideFaderMode(VCCueList::Steps);
        else cuelist->setSideFaderMode(VCCueList::None);
    }
    return true;
}

// --- Clock extended config ---

bool VCBridgeV5::configureClock(int widgetID, const ClockConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCClock *clock = qobject_cast<VCClock *>(widget);
    if (!clock) return false;

    if (config.clockType.has_value())
    {
        const QString &v = config.clockType.value();
        if (v == "stopwatch") clock->setClockType(VCClock::Stopwatch);
        else if (v == "countdown") clock->setClockType(VCClock::Countdown);
        else clock->setClockType(VCClock::Clock);
    }
    if (config.countdownH.has_value() || config.countdownM.has_value() || config.countdownS.has_value())
    {
        int h = config.countdownH.value_or(0);
        int m = config.countdownM.value_or(0);
        int s = config.countdownS.value_or(0);
        clock->setTargetTime((h * 3600 + m * 60 + s) * 1000);
    }
    if (config.schedules.has_value())
    {
        // Remove existing schedules (reverse order)
        while (!clock->schedules().isEmpty())
            clock->removeSchedule(0);

        for (const ClockScheduleInfo &info : config.schedules.value())
        {
            VCClockSchedule *sched = new VCClockSchedule(clock);
            sched->setFunctionID(info.functionID);
            sched->setStartTime(info.hour * 3600 + info.minute * 60 + info.second);
            clock->addSchedule(sched);
        }
    }
    return true;
}

// --- SpeedDial extended config ---

static VCSpeedDial::SpeedMultiplier stringToMultiplier(const QString &str)
{
    if (str == "0") return VCSpeedDial::Zero;
    if (str == "1/16") return VCSpeedDial::OneSixteenth;
    if (str == "1/8") return VCSpeedDial::OneEighth;
    if (str == "1/4") return VCSpeedDial::OneFourth;
    if (str == "1/2") return VCSpeedDial::Half;
    if (str == "1") return VCSpeedDial::One;
    if (str == "2") return VCSpeedDial::Two;
    if (str == "4") return VCSpeedDial::Four;
    if (str == "8") return VCSpeedDial::Eight;
    if (str == "16") return VCSpeedDial::Sixteen;
    return VCSpeedDial::None;
}

bool VCBridgeV5::configureSpeedDial(int widgetID, const SpeedDialConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSpeedDial *speedDial = qobject_cast<VCSpeedDial *>(widget);
    if (!speedDial) return false;

    if (config.functions.has_value())
    {
        QMap<quint32, VCSpeedDial::VCSpeedDialFunction> funcMap;
        for (const SpeedDialFunctionInfo &fi : config.functions.value())
        {
            VCSpeedDial::VCSpeedDialFunction f;
            f.m_fId = fi.functionID;
            f.m_fadeInFactor = stringToMultiplier(fi.fadeInMultiplier);
            f.m_fadeOutFactor = stringToMultiplier(fi.fadeOutMultiplier);
            f.m_durationFactor = stringToMultiplier(fi.durationMultiplier);
            funcMap.insert(fi.functionID, f);
        }
        speedDial->setFunctions(funcMap);
    }
    if (config.presets.has_value())
    {
        // Remove existing presets via public API
        QVariantList existingPresets = speedDial->presetsList();
        for (auto it = existingPresets.rbegin(); it != existingPresets.rend(); ++it)
            speedDial->removePreset(it->toMap().value("id").toUInt());

        for (const SpeedDialPresetInfo &pi : config.presets.value())
            speedDial->addPreset(pi.name, pi.value);
    }
    if (config.absoluteValueMin.has_value())
        speedDial->setTimeMinimumValue(config.absoluteValueMin.value());
    if (config.absoluteValueMax.has_value())
        speedDial->setTimeMaximumValue(config.absoluteValueMax.value());
    if (config.visibilityMask.has_value())
        speedDial->setVisibilityMask(config.visibilityMask.value());
    if (config.resetFactorOnDialChange.has_value())
        speedDial->setResetOnDialChange(config.resetFactorOnDialChange.value());
    return true;
}

// --- XY Pad presets ---

bool VCBridgeV5::setXYPadPresets(int widgetID, const QList<XYPadPresetInfo> &presets)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;

    // Remove existing presets via public API
    QVariantList existingPresets = xyPad->presetsList();
    for (auto it = existingPresets.rbegin(); it != existingPresets.rend(); ++it)
        xyPad->removePreset(it->toMap().value("id").toUInt());

    for (const XYPadPresetInfo &info : presets)
    {
        if (info.type == "position")
        {
            // Set current position before creating preset so it captures it
            xyPad->setCurrentPosition(info.position);
            int presetId = xyPad->addPositionPreset();
            if (presetId >= 0 && !info.name.isEmpty())
                xyPad->setPresetName(static_cast<quint8>(presetId), info.name);
        }
        else if (info.type == "efx" || info.type == "scene")
        {
            int presetId = xyPad->addFunctionPreset(info.functionID);
            if (presetId >= 0 && !info.name.isEmpty())
                xyPad->setPresetName(static_cast<quint8>(presetId), info.name);
        }
    }
    return true;
}

// --- Key sequences ---

bool VCBridgeV5::setWidgetKeySequence(int widgetID, const QString &sourceName,
                                      const QKeySequence &keySequence)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    int controlId = resolveControlId(widget, sourceName);
    if (controlId < 0) return false;

    // Remove existing key sequence for this control ID
    QMap<QKeySequence, quint32> existing = widget->keySequenceMap();
    for (auto it = existing.constBegin(); it != existing.constEnd(); ++it)
    {
        if (it.value() == (quint32)controlId)
            widget->deleteKeySequence(it.key());
    }

    if (!keySequence.isEmpty())
        widget->addKeySequence(keySequence, (quint32)controlId);
    return true;
}

// --- Named input mapping ---

bool VCBridgeV5::mapWidgetInputByName(int widgetID, const QString &sourceName,
                                      quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    int controlId = resolveControlId(widget, sourceName);
    if (controlId < 0) return false;

    // Remove existing input source for this control ID
    for (auto &src : widget->inputSources())
    {
        if (src->id() == (quint32)controlId)
        {
            widget->deleteInputSurce(src->id(), src->universe(), src->channel());
            break;
        }
    }

    QSharedPointer<QLCInputSource> source(new QLCInputSource(universe, channel));
    source->setID((quint32)controlId);
    widget->addInputSource(source);

    // Register with page for event routing
    for (int i = 0; i < m_vc->pagesCount(); i++)
    {
        VCPage *page = m_vc->page(i);
        if (page)
            page->mapInputSource(source, widget, true);
    }
    return true;
}

// --- Base widget properties ---

bool VCBridgeV5::setWidgetFont(int widgetID, const FontConfig &font)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    QFont f = widget->font();
    if (font.family.has_value())
        f.setFamily(font.family.value());
    if (font.pointSize.has_value())
        f.setPointSize(font.pointSize.value());
    if (font.bold.has_value())
        f.setBold(font.bold.value());
    if (font.italic.has_value())
        f.setItalic(font.italic.value());
    widget->setFont(f);
    return true;
}

bool VCBridgeV5::setWidgetBackgroundImage(int widgetID, const QString &path)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;
    widget->setBackgroundImage(path);
    return true;
}

bool VCBridgeV5::setWidgetDisableState(int widgetID, bool disabled)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;
    widget->setDisabled(disabled);
    return true;
}

// --- Page rename ---

bool VCBridgeV5::renamePage(int pageIndex, const QString &name)
{
    VCPage *page = m_vc->page(pageIndex);
    if (!page) return false;
    page->setCaption(name);
    return true;
}

bool VCBridgeV5::deletePage(int pageIndex)
{
    // VirtualConsole::deletePage returns void and silently declines an invalid
    // index or the last remaining page, so the page count is the only signal.
    const int before = m_vc->pagesCount();
    m_vc->deletePage(pageIndex);
    return m_vc->pagesCount() < before;
}

// --- Slider extended ---

bool VCBridgeV5::setSliderWidgetStyle(int widgetID, const QString &style)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (!slider) return false;

    if (style == "knob")
        slider->setWidgetStyle(VCSlider::WKnob);
    else
        slider->setWidgetStyle(VCSlider::WSlider);
    return true;
}

bool VCBridgeV5::setSliderCatchValues(int widgetID, bool enable)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (!slider) return false;
    slider->setCatchValues(enable);
    return true;
}

// --- Layout analysis: snapshot / apply ---

static VCBridge::WidgetSnapshot snapshotWidget(VCWidget *widget)
{
    VCBridge::WidgetSnapshot snap;
    snap.id = widget->id();
    snap.type = widget->type();
    snap.geometry = widget->geometry().toRect();

    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (frame)
    {
        snap.showHeader = frame->showHeader();
        snap.parentID = -1;  // filled by caller
        for (VCWidget *child : frame->children(false))
        {
            VCBridge::WidgetSnapshot childSnap = snapshotWidget(child);
            childSnap.parentID = snap.id;
            snap.children.append(childSnap);
        }
    }
    return snap;
}

VCBridge::WidgetSnapshot VCBridgeV5::snapshotFrame(int frameID) const
{
    VCWidget *widget = m_vc->widget(frameID);
    if (!widget) return WidgetSnapshot();
    return snapshotWidget(widget);
}

VCBridge::WidgetSnapshot VCBridgeV5::snapshotPage(int pageIndex) const
{
    VCPage *page = m_vc->page(pageIndex);
    if (!page) return WidgetSnapshot();
    return snapshotWidget(page);
}

void VCBridgeV5::applyLayoutPlan(const LayoutPlan &plan, bool skipSnap)
{
    const QString error = applyLayoutPlanChecked(plan, skipSnap);
    if (!error.isEmpty())
        qWarning() << "VC layout refused:" << error;
}

QString VCBridgeV5::applyLayoutPlanChecked(const LayoutPlan &plan, bool skipSnap)
{
    QMap<quint32, QRectF> logical;
    for (auto it = plan.geometries.constBegin(); it != plan.geometries.constEnd(); ++it)
    {
        VCWidget *widget = m_vc->widget(it.key());
        if (!widget)
            for (int page = 0; page < m_vc->pagesCount(); ++page)
                if (m_vc->page(page)->id() == quint32(it.key()))
                    widget = m_vc->page(page);
        if (!widget)
            return QStringLiteral("Widget %1 not found").arg(it.key());
        if (it.value() == widget->geometry().toRect())
            continue;
        const qreal scale = widget->scaleFactor();
        if (!std::isfinite(scale) || scale <= 0)
            return QStringLiteral("Widget %1 has an invalid scale").arg(it.key());
        const QRect display = skipSnap ? it.value() : snapRect(it.value());
        logical.insert(quint32(it.key()), QRectF(display.x() / scale, display.y() / scale,
                                                display.width() / scale, display.height() / scale));
    }
    const QVariantMap result = m_vc->applyGeometryPlan(logical, false);
    return result.value(QStringLiteral("accepted")).toBool()
        ? QString() : result.value(QStringLiteral("error")).toString();
}

VCBridge::LayoutEditResult VCBridgeV5::alignLayout(int frameID, int pageIndex,
                                                 int columns, int padding, bool dryRun)
{
    LayoutEditResult result;
    VCWidget *root = frameID >= 0 ? m_vc->widget(frameID) : m_vc->page(pageIndex);
    if (!root)
    {
        result.error = QStringLiteral("Frame/page not found");
        return result;
    }
    const QVariantMap preview = m_vc->arrangeLayout(root->id(),
        QVariantMap{{QStringLiteral("columns"), columns}, {QStringLiteral("pad"), padding},
                    {QStringLiteral("allowHeightGrowth"), true}}, dryRun);
    if (!preview.value(QStringLiteral("accepted")).toBool())
    {
        result.error = preview.value(QStringLiteral("error")).toString();
        return result;
    }
    for (const QVariant &entry : preview.value(QStringLiteral("geometries")).toList())
    {
        const QVariantMap record = entry.toMap();
        result.geometries.insert(record.value(QStringLiteral("id")).toInt(),
                                 record.value(QStringLiteral("rect")).toRectF());
    }
    result.applied = preview.value(QStringLiteral("applied")).toBool();
    return result;
}

bool VCBridgeV5::setFrameGridLayout(int frameID, const QString &mode,
                                     int columns, int rowHeight, bool compact)
{
    VCWidget *widget = m_vc->widget(frameID);
    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (!frame) return false;

    VCFrame::LayoutMode lm = VCFrame::LayoutFree;
    if (mode.compare("grid", Qt::CaseInsensitive) == 0)
        lm = VCFrame::LayoutGrid;

    frame->setLayoutMode(lm);
    if (columns > 0)
        frame->setGridColumns(columns);
    if (rowHeight >= 0)
        frame->setGridRowHeight(rowHeight);
    frame->setGridCompact(compact);
    return true;
}

VCBridge::FrameGridLayout VCBridgeV5::getFrameGridLayout(int frameID) const
{
    FrameGridLayout out;
    VCWidget *widget = m_vc->widget(frameID);
    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (!frame) return out;

    out.found = true;
    out.layoutMode = (frame->layoutMode() == VCFrame::LayoutGrid) ? "grid" : "free";
    out.columns = frame->gridColumns();
    out.rowHeight = frame->gridRowHeight();
    out.compact = frame->gridCompact();
    return out;
}

// --- RecordPanel extended config ---

bool VCBridgeV5::configureRecordPanel(int widgetID, const RecordPanelConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCRecordPanel *rp = qobject_cast<VCRecordPanel *>(widget);
    if (!rp) return false;

    if (config.targetFolder.has_value())
        rp->setTargetFolder(config.targetFolder.value());
    if (config.scenePrefix.has_value())
        rp->setScenePrefix(config.scenePrefix.value());
    if (config.chaserPrefix.has_value())
        rp->setChaserPrefix(config.chaserPrefix.value());
    if (config.defaultFadeIn.has_value())
        rp->setDefaultFadeIn(config.defaultFadeIn.value());
    if (config.defaultHold.has_value())
        rp->setDefaultHold(config.defaultHold.value());
    if (config.defaultFadeOut.has_value())
        rp->setDefaultFadeOut(config.defaultFadeOut.value());
    return true;
}
