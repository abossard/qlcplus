/*
  Q Light Controller Plus
  vcrecordpanel.cpp

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

#include "vcrecordpanel.h"
#include "doc.h"
#include "scene.h"
#include "chaser.h"
#include "chaserstep.h"
#include "dmxcapture.h"

#define INPUT_CREATE_SCENE_ID   0
#define INPUT_START_CHASER_ID   1
#define INPUT_STOP_CHASER_ID    2

#define KXMLQLCVCRecordPanelTargetFolder    QStringLiteral("TargetFolder")
#define KXMLQLCVCRecordPanelScenePrefix     QStringLiteral("ScenePrefix")
#define KXMLQLCVCRecordPanelChaserPrefix    QStringLiteral("ChaserPrefix")
#define KXMLQLCVCRecordPanelDefaultFadeIn   QStringLiteral("DefaultFadeIn")
#define KXMLQLCVCRecordPanelDefaultHold     QStringLiteral("DefaultHold")
#define KXMLQLCVCRecordPanelDefaultFadeOut  QStringLiteral("DefaultFadeOut")
#define KXMLQLCVCRecordPanelCreateScene     QStringLiteral("CreateScene")
#define KXMLQLCVCRecordPanelStartChaser     QStringLiteral("StartChaser")
#define KXMLQLCVCRecordPanelStopChaser      QStringLiteral("StopChaser")

VCRecordPanel::VCRecordPanel(Doc *doc, QObject *parent)
    : VCWidget(doc, parent)
    , m_targetFolder(QStringLiteral("Recordings"))
    , m_scenePrefix(QStringLiteral("Scene"))
    , m_chaserPrefix(QStringLiteral("Chaser"))
    , m_defaultFadeIn(0)
    , m_defaultHold(1000)
    , m_defaultFadeOut(0)
    , m_isRecordingChaser(false)
    , m_activeChaserId(Function::invalidId())
    , m_lastCreateSceneValue(0)
    , m_lastStartChaserValue(0)
    , m_lastStopChaserValue(0)
{
    setType(VCWidget::RecordPanelWidget);

    registerExternalControl(INPUT_CREATE_SCENE_ID, tr("Create Scene"), true);
    registerExternalControl(INPUT_START_CHASER_ID, tr("Start Chaser"), true);
    registerExternalControl(INPUT_STOP_CHASER_ID, tr("Stop Chaser"), true);

    if (m_doc != nullptr)
    {
        connect(m_doc, &Doc::functionRemoved,
                this, &VCRecordPanel::slotFunctionRemoved);
    }
}

VCRecordPanel::~VCRecordPanel()
{
    if (m_item)
        delete m_item;
}

QString VCRecordPanel::defaultCaption() const
{
    return tr("Record Panel %1").arg(id() + 1);
}

void VCRecordPanel::setupLookAndFeel(qreal pixelDensity, int page)
{
    setPage(page);
    setDefaultFontSize(pixelDensity * 3.5);
}

void VCRecordPanel::render(QQuickView *view, QQuickItem *parent)
{
    initRenderItem(view, parent, "qrc:/VCRecordPanelItem.qml", "recordPanelObj");
}

QString VCRecordPanel::propertiesResource() const
{
    return QString("qrc:/VCRecordPanelProperties.qml");
}

VCWidget *VCRecordPanel::createCopy(VCWidget *parent) const
{
    Q_ASSERT(parent != nullptr);

    VCRecordPanel *copy = new VCRecordPanel(m_doc, parent);
    if (copy->copyFrom(this) == false)
    {
        delete copy;
        copy = nullptr;
    }
    else
    {
        copy->m_targetFolder = m_targetFolder;
        copy->m_scenePrefix = m_scenePrefix;
        copy->m_chaserPrefix = m_chaserPrefix;
        copy->m_defaultFadeIn = m_defaultFadeIn;
        copy->m_defaultHold = m_defaultHold;
        copy->m_defaultFadeOut = m_defaultFadeOut;
    }

    return copy;
}

/*********************************************************************
 * Properties
 *********************************************************************/

QString VCRecordPanel::targetFolder() const
{
    return m_targetFolder;
}

void VCRecordPanel::setTargetFolder(const QString &folder)
{
    if (m_targetFolder == folder)
        return;
    m_targetFolder = folder;
    emit targetFolderChanged();
}

QString VCRecordPanel::scenePrefix() const
{
    return m_scenePrefix;
}

void VCRecordPanel::setScenePrefix(const QString &prefix)
{
    if (m_scenePrefix == prefix)
        return;
    m_scenePrefix = prefix;
    emit scenePrefixChanged();
}

QString VCRecordPanel::chaserPrefix() const
{
    return m_chaserPrefix;
}

void VCRecordPanel::setChaserPrefix(const QString &prefix)
{
    if (m_chaserPrefix == prefix)
        return;
    m_chaserPrefix = prefix;
    emit chaserPrefixChanged();
}

uint VCRecordPanel::defaultFadeIn() const
{
    return m_defaultFadeIn;
}

void VCRecordPanel::setDefaultFadeIn(uint ms)
{
    if (m_defaultFadeIn == ms)
        return;
    m_defaultFadeIn = ms;
    emit defaultFadeInChanged();
}

uint VCRecordPanel::defaultHold() const
{
    return m_defaultHold;
}

void VCRecordPanel::setDefaultHold(uint ms)
{
    if (m_defaultHold == ms)
        return;
    m_defaultHold = ms;
    emit defaultHoldChanged();
}

uint VCRecordPanel::defaultFadeOut() const
{
    return m_defaultFadeOut;
}

void VCRecordPanel::setDefaultFadeOut(uint ms)
{
    if (m_defaultFadeOut == ms)
        return;
    m_defaultFadeOut = ms;
    emit defaultFadeOutChanged();
}

/*********************************************************************
 * Recording
 *********************************************************************/

int VCRecordPanel::nextNameNumber(const QString &prefix) const
{
    int maxNum = 0;

    if (m_doc == nullptr)
        return 1;

    for (Function *f : m_doc->functions())
    {
        if (f->path(true) != m_targetFolder)
            continue;

        const QString &name = f->name();
        if (!name.startsWith(prefix))
            continue;

        // Extract trailing number: "Scene 42" → 42
        QString numStr = name.mid(prefix.length()).trimmed();
        bool ok = false;
        int num = numStr.toInt(&ok);
        if (ok && num > maxNum)
            maxNum = num;
    }

    return maxNum + 1;
}

void VCRecordPanel::createScene()
{
    if (m_doc == nullptr)
        return;

    QList<SceneValue> values = DmxCapture::captureAllFixtures(m_doc, false);

    Scene *scene = new Scene(m_doc);
    int num = nextNameNumber(m_scenePrefix + " ");
    scene->setName(QStringLiteral("%1 %2").arg(m_scenePrefix).arg(num));
    scene->setPath(m_targetFolder);

    for (const SceneValue &sv : values)
        scene->setValue(sv);

    if (!m_doc->addFunction(scene))
    {
        delete scene;
        qWarning() << Q_FUNC_INFO << "Failed to add scene to Doc";
        return;
    }

    // If recording a chaser, add as a step
    if (m_isRecordingChaser && m_activeChaserId != Function::invalidId())
    {
        Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(m_activeChaserId));
        if (chaser != nullptr)
        {
            ChaserStep step(scene->id(), m_defaultFadeIn, m_defaultHold, m_defaultFadeOut);
            chaser->addStep(step);
        }
        else
        {
            // Chaser was deleted — stop recording
            stopChaser();
        }
    }

    emit sceneCreated(scene->id());
}

void VCRecordPanel::startChaser()
{
    if (m_doc == nullptr)
        return;

    // Stop any previous chaser recording
    if (m_isRecordingChaser)
        stopChaser();

    Chaser *chaser = new Chaser(m_doc);
    int num = nextNameNumber(m_chaserPrefix + " ");
    chaser->setName(QStringLiteral("%1 %2").arg(m_chaserPrefix).arg(num));
    chaser->setPath(m_targetFolder);

    // Use per-step timing
    chaser->setFadeInMode(Chaser::PerStep);
    chaser->setFadeOutMode(Chaser::PerStep);
    chaser->setDurationMode(Chaser::PerStep);

    if (!m_doc->addFunction(chaser))
    {
        delete chaser;
        qWarning() << Q_FUNC_INFO << "Failed to add chaser to Doc";
        return;
    }

    m_activeChaserId = chaser->id();
    m_isRecordingChaser = true;
    emit isRecordingChaserChanged();
    emit chaserStarted(chaser->id());
}

void VCRecordPanel::stopChaser()
{
    if (!m_isRecordingChaser)
        return;

    quint32 stoppedId = m_activeChaserId;
    m_activeChaserId = Function::invalidId();
    m_isRecordingChaser = false;
    emit isRecordingChaserChanged();
    emit chaserStopped(stoppedId);
}

bool VCRecordPanel::isRecordingChaser() const
{
    return m_isRecordingChaser;
}

void VCRecordPanel::slotFunctionRemoved(quint32 fid)
{
    if (m_isRecordingChaser && fid == m_activeChaserId)
        stopChaser();
}

/*********************************************************************
 * External input
 *********************************************************************/

void VCRecordPanel::slotInputValueChanged(quint8 id, uchar value)
{
    switch (id)
    {
        case INPUT_CREATE_SCENE_ID:
            if (value > 0 && m_lastCreateSceneValue == 0)
                createScene();
            m_lastCreateSceneValue = value;
            break;

        case INPUT_START_CHASER_ID:
            if (value > 0 && m_lastStartChaserValue == 0)
                startChaser();
            m_lastStartChaserValue = value;
            break;

        case INPUT_STOP_CHASER_ID:
            if (value > 0 && m_lastStopChaserValue == 0)
                stopChaser();
            m_lastStopChaserValue = value;
            break;

        default:
            break;
    }
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool VCRecordPanel::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCVCRecordPanel)
    {
        qWarning() << Q_FUNC_INFO << "RecordPanel node not found";
        return false;
    }

    loadXMLCommon(root);

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCWindowState)
        {
            bool visible = false;
            int x = 0, y = 0, w = 0, h = 0;
            loadXMLWindowState(root, &x, &y, &w, &h, &visible);
            setGeometry(QRect(x, y, w, h));
        }
        else if (root.name() == KXMLQLCVCWidgetAppearance)
        {
            loadXMLAppearance(root);
        }
        else if (root.name() == KXMLQLCVCRecordPanelTargetFolder)
        {
            setTargetFolder(root.readElementText());
        }
        else if (root.name() == KXMLQLCVCRecordPanelScenePrefix)
        {
            setScenePrefix(root.readElementText());
        }
        else if (root.name() == KXMLQLCVCRecordPanelChaserPrefix)
        {
            setChaserPrefix(root.readElementText());
        }
        else if (root.name() == KXMLQLCVCRecordPanelDefaultFadeIn)
        {
            setDefaultFadeIn(root.readElementText().toUInt());
        }
        else if (root.name() == KXMLQLCVCRecordPanelDefaultHold)
        {
            setDefaultHold(root.readElementText().toUInt());
        }
        else if (root.name() == KXMLQLCVCRecordPanelDefaultFadeOut)
        {
            setDefaultFadeOut(root.readElementText().toUInt());
        }
        else if (root.name() == KXMLQLCVCRecordPanelCreateScene)
        {
            loadXMLSources(root, INPUT_CREATE_SCENE_ID);
        }
        else if (root.name() == KXMLQLCVCRecordPanelStartChaser)
        {
            loadXMLSources(root, INPUT_START_CHASER_ID);
        }
        else if (root.name() == KXMLQLCVCRecordPanelStopChaser)
        {
            loadXMLSources(root, INPUT_STOP_CHASER_ID);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown RecordPanel tag:" << root.name().toString();
            root.skipCurrentElement();
        }
    }

    return true;
}

bool VCRecordPanel::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);

    doc->writeStartElement(KXMLQLCVCRecordPanel);

    saveXMLCommon(doc);

    /* Window state */
    saveXMLWindowState(doc);

    /* Appearance */
    saveXMLAppearance(doc);

    /* Properties */
    doc->writeTextElement(KXMLQLCVCRecordPanelTargetFolder, m_targetFolder);
    doc->writeTextElement(KXMLQLCVCRecordPanelScenePrefix, m_scenePrefix);
    doc->writeTextElement(KXMLQLCVCRecordPanelChaserPrefix, m_chaserPrefix);
    doc->writeTextElement(KXMLQLCVCRecordPanelDefaultFadeIn, QString::number(m_defaultFadeIn));
    doc->writeTextElement(KXMLQLCVCRecordPanelDefaultHold, QString::number(m_defaultHold));
    doc->writeTextElement(KXMLQLCVCRecordPanelDefaultFadeOut, QString::number(m_defaultFadeOut));

    /* Input controls */
    saveXMLInputControl(doc, INPUT_CREATE_SCENE_ID, false, KXMLQLCVCRecordPanelCreateScene);
    saveXMLInputControl(doc, INPUT_START_CHASER_ID, false, KXMLQLCVCRecordPanelStartChaser);
    saveXMLInputControl(doc, INPUT_STOP_CHASER_ID, false, KXMLQLCVCRecordPanelStopChaser);

    /* End the <RecordPanel> tag */
    doc->writeEndElement();

    return true;
}
