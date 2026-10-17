/*
  Q Light Controller Plus
  vcbridgev5.h

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

#ifndef VCBRIDGEV5_H
#define VCBRIDGEV5_H

#include "vcbridge.h"
#include <QElapsedTimer>
#include <QHash>
#include <QMetaObject>
#include <QPointer>
#include <QVector>

class App;
class Doc;
class VirtualConsole;
class VCFrame;
class VCPage;
class VCWidget;
class Function;
class QLCInputSource;
class VCAuthoringOwner_Test;

class VCBridgeV5 : public VCBridge
{
public:
    VCBridgeV5(Doc *doc, VirtualConsole *vc, App *authoringApp = nullptr);
    ~VCBridgeV5() override;

    int addPage(const QString &name) override;
    QList<PageInfo> pages() const override;
    int pagesCount() const override;

    int addFrame(int pageIndex, const QRect &geometry,
                 const QString &caption, bool solo) override;
    int addFrameInFrame(int parentID, const QRect &geometry,
                        const QString &caption, bool solo) override;

    int addButton(int parentID, const QRect &geometry,
                  quint32 functionID, const QString &caption,
                  const QString &action,
                  int stopAllFadeTime = 0) override;

    int addSlider(int parentID, const QRect &geometry,
                  const QString &mode, const QString &caption,
                  quint32 functionID,
                  const QList<QPair<quint32, quint32>> &channels) override;

    int addXYPad(int parentID, const QRect &geometry,
                 const QList<quint32> &fixtureIDs) override;

    int addXYPadEx(int parentID, const QRect &geometry,
                   const QList<XYPadFixtureConfig> &fixtures,
                   const QString &displayMode = "degrees",
                   bool invertedAppearance = false) override;

    bool setXYPadPosition(int widgetID, qreal x, qreal y) override;
    bool setXYPadDisplayMode(int widgetID, const QString &mode) override;
    bool setXYPadInvertedAppearance(int widgetID, bool inverted) override;
    bool addXYPadFixture(int widgetID, const XYPadFixtureConfig &config) override;
    bool removeXYPadFixture(int widgetID, quint32 fixtureID, int head = 0) override;

    int addCueList(int parentID, const QRect &geometry,
                   quint32 chaserID, const QString &caption) override;

    int addLabel(int parentID, const QRect &geometry,
                 const QString &text) override;

    bool mapWidgetInput(int widgetID, quint32 universe,
                        quint32 channel) override;

    bool setWidgetFeedback(int widgetID,
                           int idleValue, int activeValue, int monitorValue,
                           int idleMidiCh, int activeMidiCh, int monitorMidiCh) override;

    FeedbackInfo getWidgetFeedback(int widgetID) const override;
    int widgetInputSourceCount(int widgetID) const override;

    QList<SourceDef> getWidgetSourceDefs(int widgetID) const override;
    bool setWidgetFeedbackByName(int widgetID, const QString &sourceName,
                                  int idleVal, int activeVal, int monitorVal,
                                  int idleCh, int activeCh, int monitorCh) override;
    FeedbackInfo getWidgetFeedbackByName(int widgetID, const QString &sourceName) const override;

    bool setWidgetColors(int widgetID,
                         const QColor &bgColor = QColor(),
                         const QColor &fgColor = QColor()) override;

    int addSpeedDial(int parentID, const QRect &geometry,
                     const QList<quint32> &functionIDs) override;

    int addAudioTriggers(int parentID, const QRect &geometry) override;

    bool configureAudioTriggerBar(int widgetID, const AudioBarConfig &config) override;
    bool setAudioTriggerCapture(int widgetID, bool enabled) override;
    bool setAudioTriggerVolume(int widgetID, int volume) override;
    bool setAudioTriggerBarsNumber(int widgetID, int count) override;

    int addClock(int parentID, const QRect &geometry,
                 const QString &clockType) override;

    int addRecordPanel(int parentID, const QRect &geometry) override;

    int findPageByName(const QString &name) const override;
    int findWidgetByCaption(int parentID, const QString &widgetType,
                            const QString &caption) const override;
    QRect nextWidgetPosition(int parentID, int width, int height) const override;
    QRect nextWidgetPositionFlow(int parentID, int widgetWidth, int widgetHeight,
                                  int columns = 0) const override;
    void setWidgetGeometry(int widgetID, const QRect &geo) override;
    bool removeWidget(int widgetID) override;

    // Widget details query
    WidgetDetails getWidgetDetails(int widgetID) const override;
    bool setWidgetPage(int widgetID, int pageIndex) override;

    // Widget property mutations
    bool setWidgetCaption(int widgetID, const QString &caption) override;
    bool setButtonFunction(int widgetID, quint32 functionID) override;
    bool setButtonAction(int widgetID, const QString &action) override;
    bool setSliderMode(int widgetID, const QString &mode) override;
    bool setSliderFunction(int widgetID, quint32 functionID) override;
    bool setSliderChannels(int widgetID, const QList<QPair<quint32, quint32>> &channels) override;
    bool configureSlider(int widgetID, const SliderConfig &config) override;

    // Input mapping
    bool addWidgetInput(int widgetID, quint32 universe, quint32 channel) override;
    bool removeWidgetInput(int widgetID, quint32 universe, quint32 channel) override;

    // Widget reparenting
    bool reparentWidget(int widgetID, int newParentID, const QRect &geo) override;

    // Matrix widget
    int addMatrix(int parentID, const QRect &geometry,
                  quint32 functionID, const QString &caption) override;
    bool configureMatrix(int widgetID, const MatrixConfig &config) override;

    // Extended widget config
    bool configureButton(int widgetID, const ButtonConfig &config) override;
    bool configureFrame(int widgetID, const FrameConfig &config) override;
    bool configureCueList(int widgetID, const CueListConfig &config) override;
    bool configureClock(int widgetID, const ClockConfig &config) override;
    bool configureSpeedDial(int widgetID, const SpeedDialConfig &config) override;
    bool configureRecordPanel(int widgetID, const RecordPanelConfig &config) override;

    // XY Pad presets
    bool setXYPadPresets(int widgetID, const QList<XYPadPresetInfo> &presets) override;

    // Key sequences and named input mapping
    bool setWidgetKeySequence(int widgetID, const QString &sourceName,
                              const QKeySequence &keySequence) override;
    bool mapWidgetInputByName(int widgetID, const QString &sourceName,
                              quint32 universe, quint32 channel) override;

    // Base widget properties
    bool setWidgetFont(int widgetID, const FontConfig &font) override;
    bool setWidgetBackgroundImage(int widgetID, const QString &path) override;
    bool setWidgetDisableState(int widgetID, bool disabled) override;

    // Page rename
    bool renamePage(int pageIndex, const QString &name) override;
    bool deletePage(int pageIndex) override;

    // Slider extended
    bool setSliderWidgetStyle(int widgetID, const QString &style) override;
    bool setSliderCatchValues(int widgetID, bool enable) override;

    // Layout analysis
    WidgetSnapshot snapshotFrame(int frameID) const override;
    WidgetSnapshot snapshotPage(int pageIndex) const override;
    void applyLayoutPlan(const LayoutPlan &plan, bool skipSnap = false) override;
    QString applyLayoutPlanChecked(const LayoutPlan &plan, bool skipSnap = false) override;
    LayoutEditResult alignLayout(int frameID, int pageIndex, int columns,
                                 int padding, bool dryRun) override;
    QString setWidgetGeometryChecked(int widgetID, const QRect &geo) override;
    int createWidgetOnPage(int parentID, const QString &type, const QRect &geo,
                           const QString &caption, int subpage) override;

    // Frame grid layout
    bool setFrameGridLayout(int frameID, const QString &mode,
                            int columns, int rowHeight, bool compact) override;
    FrameGridLayout getFrameGridLayout(int frameID) const override;

    int snappingSize() const override;

    AuthoringReadResult queryAuthoring(const AuthoringQuery &query) const override;
    AuthoringPreparationResult prepareAuthoring(const AuthoringRequest &request) override;
    AuthoringApplyResult applyAuthoring(const QString &preparationToken) override;

private:
    friend class VCAuthoringOwner_Test;
    struct PreparedGeometryPayload;
    struct GuardKey;

    struct PredicateSelection
    {
        bool requireFeedbackPatchIdle = false;  // F
        bool requireNoRelativeSources = false;  // R
        bool requireFeedbackNeedsUpdateIdle = false; // U
        bool requireWriterSafe = false;         // W
    };

    /** Round a rectangle's position and size to the nearest grid multiple */
    QRect snapRect(const QRect &rect) const;
    VCWidget *createGeometryWidget(VCFrame *frame, const QString &type, const QRect &geometry,
                                    int subpage = -1);
    bool hasMatchingAuthoringContext() const;
    QString authoringBasis() const;
    QByteArray canonicalizeLayoutRequest(const AuthoringLayoutRequest &request) const;
    QByteArray canonicalizeRequest(const AuthoringRequest &request) const;
    QString candidateDigest(const QMap<quint32, QRectF> &rectangles) const;
    QMap<quint32, QRectF> planFromNativePreview(const QVariantMap &preview) const;
    QVector<VCWidget *> affectedClosure(const QMap<quint32, QRectF> &rectangles) const;
    QVector<VCWidget *> affectedClosureForWidgetIds(const QList<quint32> &widgetIds) const;
    PredicateSelection predicateSelectionForRequest(const AuthoringRequest &request) const;
    QString scopedConfigurationDigest(VCWidget *widget) const;
    QString scopedConfigurationDigest(const AuthoringSourceKey &key, const QLCInputSource *source) const;
    QString scopedConfigurationDigest(const Function *function) const;
    QString scopedConfigurationDigest(const GuardKey &key) const;
    QString preconditionErrorForClosure(const QVector<VCWidget *> &closure, QString *code,
                                        QList<AuthoringPreconditionObservation> *observations,
                                        const PredicateSelection &selection) const;
    int estimateGuardCount(const QMap<quint32, QRectF> &rectangles) const;
    QList<quint32> widgetIdsFromRequest(const AuthoringRequest &request) const;
    int requestItemCount(const AuthoringRequest &request) const;
    int rootIdentityCount(const AuthoringRequest &request) const;
    void clearAuthoringRecords();
    void cleanupExpiredRecords(qint64 nowMs);
    QString authoringPreconditionError(const QMap<quint32, QRectF> &rectangles, QString *code,
                                       QList<AuthoringPreconditionObservation> *observations,
                                       const PredicateSelection &selection) const;
    std::optional<quint32> owningPageWidgetId(VCWidget *widget) const;
    std::optional<quint32> selectedFunctionId(VCWidget *widget) const;
    std::optional<AuthoringSourceKey> selectedSourceKey(VCWidget *widget, std::optional<int> *ordinal) const;
    void populateSelectionDetails(AuthoringPreparationItem &item, VCWidget *widget) const;
    void populateSelectionDetails(AuthoringApplyItem &item, VCWidget *widget) const;
    void recomputeGeometryDiff(AuthoringPreparationItem &item,
                               AuthoringPreparationResult::DiffDetail diffDetail) const;
    void recomputeGeometryDiff(AuthoringApplyItem &item,
                               AuthoringPreparationResult::DiffDetail diffDetail) const;
    QObject *resolveGuardObject(const GuardKey &key) const;
    qint64 authoringNowMs() const;
    void setAuthoringNowForTest(qint64 nowMs);
    void clearAuthoringNowForTest();

    QPointer<Doc> m_doc;
    QPointer<VirtualConsole> m_vc;
    QPointer<App> m_authoringApp;

    struct GuardKey
    {
        enum class Kind
        {
            Widget,
            Function,
            InputSource
        };

        Kind kind = Kind::Widget;
        std::optional<quint32> widgetID;
        std::optional<quint32> functionID;
        std::optional<AuthoringSourceKey> sourceKey;
    };

    struct GeometryGuard
    {
        GuardKey key;
        QPointer<QObject> identity;
        QString scopedConfigurationDigest;
    };

    struct PreparedGeometryPayload
    {
        QMap<quint32, QRectF> candidateRectangles;
        QMap<quint32, QRectF> baselineRectangles;
        QList<quint32> requestOrder;
    };

    struct PreparedLayoutPayload
    {
        AuthoringLayoutRequest request;
    };

    struct PreparedBindingPayload
    {
        QList<AuthoringBindingItem> items;
        QList<quint32> requestOrder;
        QMap<int, QString> destinationDigests;
        bool allowFanout = false;
    };

    struct PreparedRecordPanelPayload
    {
        QList<AuthoringRecordPanelItem> items;
        QList<quint32> requestOrder;
    };

    struct PreparationRecord
    {
        QString token;
        qint64 expiresAtMs = 0;
        QString basis;
        AuthoringRequest normalizedRequest;
        QString candidateDigest;
        std::variant<PreparedGeometryPayload, PreparedLayoutPayload, PreparedBindingPayload, PreparedRecordPanelPayload> operationPayload;
        QVector<GeometryGuard> geometryGuards;
        PredicateSelection predicates;
    };

    QHash<QString, PreparationRecord> m_authoringRecords;
    QMetaObject::Connection m_docDestroyedConn;
    QMetaObject::Connection m_vcDestroyedConn;
    QMetaObject::Connection m_appDestroyedConn;
    QElapsedTimer m_authoringMonotonicClock;
    std::optional<qint64> m_authoringNowOverrideMs;
};

#endif // VCBRIDGEV5_H
