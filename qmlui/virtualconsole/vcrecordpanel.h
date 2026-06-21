/*
  Q Light Controller Plus
  vcrecordpanel.h

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

#ifndef VCRECORDPANEL_H
#define VCRECORDPANEL_H

#include "vcwidget.h"

#define KXMLQLCVCRecordPanel QStringLiteral("RecordPanel")

class Doc;
class Scene;
class Chaser;

class VCRecordPanel : public VCWidget
{
    Q_OBJECT

    Q_PROPERTY(QString targetFolder READ targetFolder WRITE setTargetFolder NOTIFY targetFolderChanged)
    Q_PROPERTY(QString scenePrefix READ scenePrefix WRITE setScenePrefix NOTIFY scenePrefixChanged)
    Q_PROPERTY(QString chaserPrefix READ chaserPrefix WRITE setChaserPrefix NOTIFY chaserPrefixChanged)
    Q_PROPERTY(uint defaultFadeIn READ defaultFadeIn WRITE setDefaultFadeIn NOTIFY defaultFadeInChanged)
    Q_PROPERTY(uint defaultHold READ defaultHold WRITE setDefaultHold NOTIFY defaultHoldChanged)
    Q_PROPERTY(uint defaultFadeOut READ defaultFadeOut WRITE setDefaultFadeOut NOTIFY defaultFadeOutChanged)
    Q_PROPERTY(bool isRecordingChaser READ isRecordingChaser NOTIFY isRecordingChaserChanged)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    VCRecordPanel(Doc *doc = nullptr, QObject *parent = nullptr);
    virtual ~VCRecordPanel();

    /** @reimp */
    QString defaultCaption() const override;

    /** @reimp */
    void setupLookAndFeel(qreal pixelDensity, int page) override;

    /** @reimp */
    void render(QQuickView *view, QQuickItem *parent) override;

    /** @reimp */
    QString propertiesResource() const override;

    /** @reimp */
    VCWidget *createCopy(VCWidget *parent) const override;

    /*********************************************************************
     * Properties
     *********************************************************************/
public:
    QString targetFolder() const;
    void setTargetFolder(const QString &folder);

    QString scenePrefix() const;
    void setScenePrefix(const QString &prefix);

    QString chaserPrefix() const;
    void setChaserPrefix(const QString &prefix);

    uint defaultFadeIn() const;
    void setDefaultFadeIn(uint ms);

    uint defaultHold() const;
    void setDefaultHold(uint ms);

    uint defaultFadeOut() const;
    void setDefaultFadeOut(uint ms);

signals:
    void targetFolderChanged();
    void scenePrefixChanged();
    void chaserPrefixChanged();
    void defaultFadeInChanged();
    void defaultHoldChanged();
    void defaultFadeOutChanged();

private:
    QString m_targetFolder;
    QString m_scenePrefix;
    QString m_chaserPrefix;
    uint m_defaultFadeIn;
    uint m_defaultHold;
    uint m_defaultFadeOut;

    /*********************************************************************
     * Recording
     *********************************************************************/
public:
    /** Capture current DMX state and create a new Scene in targetFolder.
     *  If chaser recording is active, also appends the scene as a step. */
    Q_INVOKABLE void createScene();

    /** Create a new Chaser in targetFolder and enter recording mode. */
    Q_INVOKABLE void startChaser();

    /** Stop chaser recording mode. No-op if not recording. */
    Q_INVOKABLE void stopChaser();

    bool isRecordingChaser() const;

signals:
    void isRecordingChaserChanged();
    void sceneCreated(quint32 sceneId);
    void chaserStarted(quint32 chaserId);
    void chaserStopped(quint32 chaserId);

private slots:
    void slotFunctionRemoved(quint32 fid);

private:
    /** Derive the next auto-increment number for a given prefix in targetFolder. */
    int nextNameNumber(const QString &prefix) const;

    bool m_isRecordingChaser;
    quint32 m_activeChaserId;

    /*********************************************************************
     * External input
     *********************************************************************/
public:
    /** @reimp */
    void slotInputValueChanged(quint8 id, uchar value) override;

private:
    uchar m_lastCreateSceneValue;
    uchar m_lastStartChaserValue;
    uchar m_lastStopChaserValue;

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    /** @reimp */
    bool loadXML(QXmlStreamReader &root) override;

    /** @reimp */
    bool saveXML(QXmlStreamWriter *doc) const override;
};

#endif // VCRECORDPANEL_H
