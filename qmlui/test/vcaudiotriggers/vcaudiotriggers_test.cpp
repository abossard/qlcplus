#include "vcaudiotriggers_test.h"

#include <QtTest>
#include <QBuffer>
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QJSValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickItem>
#include <QQuickView>
#include <QSignalSpy>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <limits>
#include <memory>
#include <array>
#include <cmath>
#define private public
#include "doc.h"
#include "huematrixeditor.h"
#include "scriptrunner.h"
#include "tardis.h"
#undef private
#include "audioanalyzer.h"
#include "audioview.h"
#include "huecolor.h"
#include "huescript.h"
#include "huescriptscache.h"
#include "rgbaudio.h"

#include "audiochannel.h"
#include "audiocapture_qt6.h"
#include "audioprofile.h"
#include "audiosparklineitem.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "fixturemanager.h"
#include "function.h"
#include "functionmanager.h"
#include "contextmanager.h"
#include "grouphead.h"
#include "huematrix.h"
#include "inputoutputmanager.h"
#include "mastertimer.h"
#include "scene.h"
#include "vcbutton.h"
#include "vcaudiotriggers.h"
#include "virtualconsole.h"

namespace
{
class MissingInputBackend : public AudioCaptureQt6::InputBackend
{
public:
    QList<AudioCaptureQt6::Device> devices() const override { return {}; }
    QIODevice *open(const QByteArray &, int, int, QAudioFormat &) override { return nullptr; }
    bool failed() const override { return false; }
    void close() override {}
    void setVolume(qreal) override {}
};

class ConsumerCapture final : public AudioCapture
{
public:
    ConsumerCapture()
    {
        m_sampleRate = 30000;
        m_channels = 1;
        m_readFrames = 500;
        m_floatInput = true;
    }
    ~ConsumerCapture() override { stop(); }
    void run() override {}
    void setVolume(qreal) override {}
    bool feed(const std::array<float, 500> &samples)
    {
        m_floatBuffer.assign(samples.begin(), samples.end());
        return processData();
    }
    int subscribers() const { return m_registerCount; }
protected:
    bool initialize() override { return true; }
    void uninitialize() override {}
    void suspend() override {}
    void resume() override {}
    qint64 latency() const override { return 0; }
    bool readAudio(int) override { return false; }
};

class MappedButton final : public VCButton
{
public:
    MappedButton(Doc *doc, quint32 functionId) : VCButton(doc)
    {
        m_functionID = functionId;
    }
};

QVariantMap qmlMap(const QVariant &value)
{
    return value.metaType().id() == qMetaTypeId<QJSValue>()
        ? value.value<QJSValue>().toVariant().toMap() : value.toMap();
}

AudioProfile *addProfile(Doc &doc, quint32 id)
{
    auto *profile = new AudioProfile(id, &doc);
    profile->setName(QStringLiteral("Profile %1").arg(id));
    auto config = profile->channelConfig();
    config.noiseGate.thresholdDb = id == 7 ? -65 : -43;
    config.aubio.melBanks.low.post.agcDecay = id == 7 ? 0.17 : 0.29;
    config.aubio.melBanks.mid.post.powerFactor = id == 7 ? 1.43 : 2.71;
    config.aubio.melBanks.high.bands = id == 7 ? 17 : 29;
    config.aubio.onsetOverrides[2].threshold = 0.37;
    config.aubio.melBanks.preset = QStringLiteral("Custom");
    profile->setChannelConfig(config);
    if (!doc.addAudioProfile(profile))
    {
        delete profile;
        return nullptr;
    }
    return profile;
}

QString profileXml(AudioProfile *profile)
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    profile->saveXML(&writer);
    return xml;
}
}

void VCAudioTriggers_Test::initTestCase()
{
    for (const QString &name : {QStringLiteral("VCAudioTriggersProperties.qml"),
                                QStringLiteral("VCAudioTriggersItem.qml")})
    {
        QFile source(QFINDTESTDATA(QStringLiteral("../../qml/virtualconsole/") + name));
        QFile resource(QStringLiteral(":/") + name);
        QVERIFY(source.open(QIODevice::ReadOnly));
        QVERIFY(resource.open(QIODevice::ReadOnly));
        QCOMPARE(resource.readAll(), source.readAll());
    }
}

void VCAudioTriggers_Test::realPanel_data()
{
    QTest::addColumn<int>("profileId");
    QTest::newRow("null-widget") << -1;
    QTest::newRow("noncontiguous-7") << 7;
    QTest::newRow("noncontiguous-29") << 29;
    QTest::newRow("missing-device") << -2;
}

void VCAudioTriggers_Test::realPanel()
{
    QFETCH(int, profileId);
    Doc doc(nullptr, 1);
    auto *seven = addProfile(doc, 7);
    auto *twentyNine = addProfile(doc, 29);
    QVERIFY(seven && twentyNine);
    const QString sevenBefore = profileXml(seven);
    const QString twentyNineBefore = profileXml(twentyNine);
    VCAudioTriggers widget(&doc);
    widget.setAudioProfileId(profileId < 0 ? 7 : quint32(profileId));
    AudioCaptureQt6 missingCapture(std::make_unique<MissingInputBackend>());
    if (profileId == -2)
    {
        missingCapture.setInputDevice("id:bWlzc2luZw==");
        missingCapture.registerBandsNumber(3);
        QTRY_COMPARE(missingCapture.status(), AudioCapture::Unavailable);
        widget.m_inputCapture = &missingCapture;
    }

    qmlRegisterType<VCAudioTriggers>("org.qlcplus.classes", 1, 0, "VCAudioTriggers");
    qmlRegisterType<AudioSparklineItem>("org.qlcplus.classes", 1, 0, "AudioSparkline");
    qmlRegisterUncreatableType<Function>("org.qlcplus.classes", 1, 0, "QLCFunction", "Engine-owned");
    QQuickView view;
    view.resize(900, 700);
    view.rootContext()->setContextProperty("screenPixelDensity", 4.0);
    view.rootContext()->setContextProperty("mainView", view.contentItem());
    InputOutputManager ioManager(&view, &doc);
    FunctionManager functionManager(&view, &doc);
    QStringList warnings;
    connect(view.engine(), &QQmlEngine::warnings, this, [&warnings](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
            warnings.append(error.toString());
    });
    QQmlComponent component(view.engine(), QUrl(QStringLiteral("qrc:/VCAudioTriggersProperties.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {"width", 780},
        {"widgetRef", QVariant::fromValue(profileId == -1 ? nullptr : &widget)}
    }));
    QVERIFY2(panel, qPrintable(component.errorString()));
    qobject_cast<QQuickItem *>(panel.get())->setParentItem(view.contentItem());
    QCoreApplication::processEvents();

    auto *selector = panel->findChild<QObject *>("audioProfileSelector");
    QVERIFY(selector);
    if (profileId >= 0)
        QTRY_COMPARE(selector->property("currentValue").toUInt(), quint32(profileId));
    if (profileId == -2)
    {
        QVERIFY(!widget.analysisAvailable());
        QVERIFY(!widget.analysisStatus().isEmpty());
        auto *status = panel->findChild<QObject *>("audioStatus");
        QVERIFY(status);
        QVERIFY(status->property("label").toString().contains(missingCapture.statusMessage()));
    }
    auto *advanced = panel->findChild<QObject *>("audioAdvanced");
    QVERIFY(advanced);
    QVERIFY(!advanced->property("isExpanded").toBool());
    int editable = 0;
    for (QObject *control : panel->findChildren<QObject *>())
        if (control->property("setupControl").toBool())
            ++editable;
    QCOMPARE(editable, 6);
    advanced->setProperty("isExpanded", true);
    QCoreApplication::processEvents();
    if (profileId >= 0)
    {
        auto *lowEditor = panel->findChild<QObject *>("audioBankEditor0");
        QVERIFY(lowEditor);
        QCOMPARE(qmlMap(lowEditor->property("bank")).value("agcDecay").toDouble(),
                 profileId == 7 ? 0.17 : 0.29);
        widget.setAudioProfileId(profileId == 7 ? 29 : 7);
        QTRY_COMPARE(selector->property("currentValue").toUInt(), widget.audioProfileId());
        QCOMPARE(qmlMap(lowEditor->property("bank")).value("agcDecay").toDouble(),
                 profileId == 7 ? 0.29 : 0.17);
    }
    QCOMPARE(profileXml(seven), sevenBefore);
    QCOMPARE(profileXml(twentyNine), twentyNineBefore);
    if (profileId >= 0)
    {
        const int count = widget.profileListModel()->rowCount();
        widget.duplicateCurrentProfile("Duplicate with overrides");
        QTRY_COMPARE(widget.profileListModel()->rowCount(), count + 1);
        QTRY_COMPARE(selector->property("currentValue").toUInt(), widget.audioProfileId());
        widget.renameCurrentProfile("Renamed profile");
        QTRY_COMPARE(selector->property("currentText").toString(), QString("Renamed profile"));
        widget.deleteCurrentProfile();
        QTRY_COMPARE(widget.profileListModel()->rowCount(), count);
        QVERIFY(selector->property("currentIndex").toInt() >= 0);
        widget.setAudioProfileId(9999);
        QVERIFY(!widget.analysisAvailable());
        QCOMPARE(selector->property("currentIndex").toInt(), -1);
        QVERIFY(!widget.analysisStatus().isEmpty());
        widget.duplicateCurrentProfile("New profile");
        QTRY_COMPARE(widget.profileListModel()->rowCount(), count + 1);
        QTRY_COMPARE(selector->property("currentValue").toUInt(), widget.audioProfileId());
        QTRY_COMPARE(selector->property("currentText").toString(), QString("New profile"));
        QCOMPARE(widget.bankConfiguration()[0].toMap().value("agcDecay").toDouble(),
                 AudioChannelConfig::defaults().aubio.melBanks.low.post.agcDecay);
        widget.deleteCurrentProfile();
        QTRY_COMPARE(widget.profileListModel()->rowCount(), count);
        QTRY_COMPARE(selector->property("currentValue").toUInt(), widget.resolvedProfileId());
    }
    panel.reset();
    QCoreApplication::processEvents();
    widget.m_inputCapture = doc.audioInputCapture().data();
    if (profileId == -2)
        missingCapture.unregisterBandsNumber(3);
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
}

void VCAudioTriggers_Test::referenceDefaults_data()
{
    QTest::addColumn<bool>("reset");
    QTest::addColumn<double>("frequency");
    QTest::newRow("new bass") << false << 70.0;
    QTest::newRow("new mids") << false << 800.0;
    QTest::newRow("new highs") << false << 5000.0;
    QTest::newRow("reset bass") << true << 70.0;
    QTest::newRow("reset mids") << true << 800.0;
    QTest::newRow("reset highs") << true << 5000.0;
}

void VCAudioTriggers_Test::referenceDefaults()
{
    QFETCH(bool, reset);
    QFETCH(double, frequency);

    Doc doc(nullptr, 0);
    auto capture = QSharedPointer<ConsumerCapture>::create();
    doc.m_inputCapture = capture;
    capture->setAnalyzer(doc.audioAnalyzer());
    auto *profile = new AudioProfile(7, &doc);
    profile->setName("Selected audio");
    QVERIFY(doc.addAudioProfile(profile));
    auto *other = addProfile(doc, 29);
    QVERIFY(other);
    const QString otherBefore = profileXml(other);
    doc.setActiveAudioProfileId(7);
    VCAudioTriggers widget(&doc);
    widget.setAudioProfileId(7);
    if (reset)
    {
        auto config = profile->channelConfig();
        config.noiseGate.thresholdDb = 0;
        config.noiseGate.holdMs = 2000;
        config.aubio.filterbankPower = 0.6;
        config.aubio.melBanks.low.post.agcDecay = 0.3;
        config.aubio.melBanks.mid.post.enabled = false;
        config.aubio.melBanks.high.post.powerFactor = 3;
        config.aubio.onsetMethodIndex = 8;
        profile->setChannelConfig(config);
    }

    qmlRegisterType<VCAudioTriggers>("org.qlcplus.classes", 1, 0, "VCAudioTriggers");
    qmlRegisterUncreatableType<Function>("org.qlcplus.classes", 1, 0, "QLCFunction", "Engine-owned");
    QQuickView view;
    view.rootContext()->setContextProperty("screenPixelDensity", 4.0);
    view.rootContext()->setContextProperty("mainView", view.contentItem());
    InputOutputManager ioManager(&view, &doc);
    FunctionManager functionManager(&view, &doc);
    QStringList warnings;
    connect(view.engine(), &QQmlEngine::warnings, this, [&warnings](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
            warnings.append(error.toString());
    });
    QQmlComponent component(view.engine(), QUrl("qrc:/VCAudioTriggersProperties.qml"));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {"width", 780}, {"widgetRef", QVariant::fromValue(&widget)}
    }));
    QVERIFY2(panel, qPrintable(component.errorString()));
    if (reset)
    {
        auto *button = panel->findChild<QObject *>("audioResetDefaults");
        QVERIFY(button);
        QCOMPARE(button->property("label").toString(), QString("LedFx defaults"));
        QVERIFY(QMetaObject::invokeMethod(button, "clicked", Q_ARG(int, int(Qt::LeftButton))));
    }

    const auto config = profile->channelConfig();
    static const MelPostConfig referencePost{
        1.9626105055051506, 1.0, 0.7, 0.99, 0.99, 0.01, 0.15, 0.99, 0.01, 0.99, true};
    QCOMPARE(config.noiseGate.thresholdDb, -80.0);
    QCOMPARE(config.noiseGate.holdMs, 0.0);
    QCOMPARE(config.aubio.filterbankPower, 1.0);
    QCOMPARE(config.aubio.filterbankNorm, 1.0);
    QCOMPARE(config.aubio.melScale, QString("matt_mel"));
    QVERIFY(config.aubio.preEmphasisEnabled);
    QVERIFY(config.melPost == referencePost);
    const MelBankConfig::Bank banks[] = {
        config.aubio.melBanks.low, config.aubio.melBanks.mid, config.aubio.melBanks.high};
    static constexpr double maxima[] = {350.0, 2000.0, 15000.0};
    for (int i = 0; i < 3; ++i)
    {
        QCOMPARE(banks[i].minHz, 20.0);
        QCOMPARE(banks[i].maxHz, maxima[i]);
        QCOMPARE(banks[i].bands, 24);
        QVERIFY(banks[i].post == referencePost);
    }
    QCOMPARE(config.aubio.onsetMethodIndex, 1);
    for (int i = 0; i < 9; ++i)
        QCOMPARE(config.aubio.onsetMethodEnabled[i], i == 1);
    QCOMPARE(config.aubio.pitchMethod, QString("yinfft"));
    QCOMPARE(config.aubio.pitchUnit, QString("midi"));
    QCOMPARE(config.aubio.pitchTolerance, 0.8);
    QCOMPARE(config.kick.beatHistoryLen, 12);

    QVERIFY(doc.hueScriptsCache()->load(QDir(QFINDTESTDATA("../../../resources/huescripts")), true));
    HUEScript spectrum(&doc);
    QVERIFY(spectrum.load(QFINDTESTDATA("../../../resources/huescripts/audiospectrum.js")));
    RGBMap previous;
    for (int phase = 0; phase < 2; ++phase)
    {
        for (int hop = 0; hop < 120; ++hop)
        {
            std::array<float, 500> samples{};
            for (int i = 0; i < 500; ++i)
            {
                const double time = double((phase * 120 + hop) * 500 + i) / 30000;
                const double tone = frequency * (phase == 0 ? 1.0 : 1.7);
                samples[i] = float(0.003 * (std::sin(2 * M_PI * tone * time)
                    + 0.3 * std::sin(2 * M_PI * tone * 1.3 * time)));
            }
            QVERIFY(capture->feed(samples));
        }
        widget.processAudioSnapshot();
        const auto snapshot = doc.audioSnapshot();
        QVERIFY(snapshot.available);
        QVERIFY(!snapshot.noiseGateClosed);
        QVERIFY(snapshot.config.aubio.melBanks == config.aubio.melBanks);
        const double level = frequency < 250 ? snapshot.lows :
            frequency < 3000 ? snapshot.mids : snapshot.highs;
        QVERIFY(level > 0.0);
        RGBMap pixels;
        spectrum.rgbMap({37, 1}, 0xffffff, 0, pixels);
        QCOMPARE(pixels.size(), 1);
        QCOMPARE(pixels[0].size(), 37);
        QVERIFY(std::any_of(pixels[0].cbegin(), pixels[0].cend(),
                           [](uint rgb) { return (rgb & 0xffffff) != 0; }));
        if (phase == 1)
            QVERIFY(pixels != previous);
        previous = pixels;
    }
    QCOMPARE(profile->id(), quint32(7));
    QCOMPARE(profile->name(), QString("Selected audio"));
    QCOMPARE(profileXml(other), otherBefore);
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
}

void VCAudioTriggers_Test::bankUpdate_data()
{
    QTest::addColumn<int>("bank");
    QTest::addColumn<QVariantMap>("changes");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("low") << 0 << QVariantMap{{"agcDecay", 0.31}, {"trigHigh", 0.73}, {"trigLow", 0.22}} << true;
    QTest::newRow("mid") << 1 << QVariantMap{{"powerFactor", 1.83}, {"gaussianSigma", 2.1}} << true;
    QTest::newRow("high") << 2 << QVariantMap{{"bands", 19}, {"minHz", 160.0}, {"maxHz", 8700.0}} << true;
    QTest::newRow("lower-bounds") << 0 << QVariantMap{{"bands", 4}, {"minHz", 0.0}, {"maxHz", 1.0},
        {"agcDecay", 0.0}, {"powerFactor", 0.1}, {"trigLow", 0.0}, {"trigHigh", 1.0}} << true;
    QTest::newRow("upper-bounds") << 2 << QVariantMap{{"bands", AudioSnapshot::kMelBankBandsMax},
        {"maxHz", 15000.0}, {"agcDecay", 1.0}, {"powerFactor", 10.0}, {"trigHold", 10000.0}} << true;
    QTest::newRow("nan") << 0 << QVariantMap{{"agcDecay", std::numeric_limits<double>::quiet_NaN()}} << false;
    QTest::newRow("unknown") << 1 << QVariantMap{{"powerFactor", 1.5}, {"typo", 0.5}} << false;
    QTest::newRow("reversed") << 2 << QVariantMap{{"minHz", 9000.0}, {"maxHz", 8000.0}} << false;
    QTest::newRow("fractional-bands") << 0 << QVariantMap{{"bands", 17.5}} << false;
    QTest::newRow("bad-hysteresis") << 1 << QVariantMap{{"trigLow", 0.95}, {"trigHigh", 0.25}} << false;
    QTest::newRow("invalid-bank") << 3 << QVariantMap{{"agcDecay", 0.5}} << false;
}

void VCAudioTriggers_Test::bankUpdate()
{
    QFETCH(int, bank);
    QFETCH(QVariantMap, changes);
    QFETCH(bool, accepted);
    Doc doc(nullptr, 1);
    auto *seven = addProfile(doc, 7);
    auto *twentyNine = addProfile(doc, 29);
    QVERIFY(seven && twentyNine);
    VCAudioTriggers first(&doc), second(&doc);
    first.setAudioProfileId(7);
    second.setAudioProfileId(29);
    const QString before = profileXml(seven);
    const QString otherBefore = profileXml(twentyNine);
    const QVariantList banksBefore = first.bankConfiguration();
    QSignalSpy edits(seven, &AudioProfile::configChanged);
    QCOMPARE(first.updateBank(bank, changes), accepted);
    QCOMPARE(profileXml(twentyNine), otherBefore);
    if (!accepted)
    {
        QCOMPARE(profileXml(seven), before);
        QCOMPARE(edits.count(), 0);
        return;
    }
    QCOMPARE(edits.count(), 1);
    const auto banksAfter = first.bankConfiguration();
    for (int index = 0; index < 3; ++index)
        if (index != bank)
            QCOMPARE(banksAfter[index], banksBefore[index]);
    const QVariantMap values = banksAfter[bank].toMap();
    for (auto it = changes.cbegin(); it != changes.cend(); ++it)
        QCOMPARE(values.value(it.key()).toDouble(), it.value().toDouble());
    QCOMPARE(seven->channelConfig().aubio.onsetOverrides[2].threshold, 0.37);
}

void VCAudioTriggers_Test::bankControlBounds_data()
{
    QTest::addColumn<int>("bankIndex");
    QTest::addColumn<QString>("field");
    QTest::addColumn<double>("initial");
    QTest::addColumn<double>("scale");
    QTest::addColumn<int>("key");
    for (int bank = 0; bank < 3; ++bank)
    {
        const QByteArray prefix = QByteArray::number(bank) + '-';
        QTest::newRow((prefix + "max-frequency").constData())
            << bank << QString("maxHz") << 15000.0 << 1.0 << int(Qt::Key_Up);
        QTest::newRow((prefix + "max-bands").constData())
            << bank << QString("bands") << 32.0 << 1.0 << int(Qt::Key_Up);
        QTest::newRow((prefix + "crossed-range").constData())
            << bank << QString("minHz") << 199.0 << 1.0 << int(Qt::Key_Up);
        QTest::newRow((prefix + "crossed-low-trigger").constData())
            << bank << QString("trigLow") << 0.49 << 100.0 << int(Qt::Key_Up);
        QTest::newRow((prefix + "crossed-high-trigger").constData())
            << bank << QString("trigHigh") << 0.50 << 100.0 << int(Qt::Key_Down);
    }
}

void VCAudioTriggers_Test::bankControlBounds()
{
    QFETCH(int, bankIndex);
    QFETCH(QString, field);
    QFETCH(double, initial);
    QFETCH(double, scale);
    QFETCH(int, key);
    Doc doc(nullptr, 0);
    QVERIFY(addProfile(doc, 7));
    VCAudioTriggers widget(&doc);
    widget.setAudioProfileId(7);
    QVariantMap setup{{field, initial}};
    if (field == "minHz")
        setup["maxHz"] = 200.0;
    if (field.startsWith("trig"))
    {
        setup["trigLow"] = 0.49;
        setup["trigHigh"] = 0.50;
    }
    QVERIFY(widget.updateBank(bankIndex, setup));
    const QString before = profileXml(doc.audioProfile(7));
    qmlRegisterType<VCAudioTriggers>("org.qlcplus.classes", 1, 0, "VCAudioTriggers");
    qmlRegisterUncreatableType<Function>("org.qlcplus.classes", 1, 0, "QLCFunction", "Engine-owned");
    QQuickView view;
    view.resize(900, 900);
    view.rootContext()->setContextProperty("screenPixelDensity", 4.0);
    view.rootContext()->setContextProperty("mainView", view.contentItem());
    InputOutputManager ioManager(&view, &doc);
    FunctionManager functionManager(&view, &doc);
    QQmlComponent component(view.engine(), QUrl("qrc:/VCAudioTriggersProperties.qml"));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {"width", 780}, {"widgetRef", QVariant::fromValue(&widget)}
    }));
    QVERIFY2(panel, qPrintable(component.errorString()));
    qobject_cast<QQuickItem *>(panel.get())->setParentItem(view.contentItem());
    auto *advanced = panel->findChild<QObject *>("audioAdvanced");
    QVERIFY(advanced);
    advanced->setProperty("isExpanded", true);
    auto *control = panel->findChild<QQuickItem *>(
        QString("audioBank%1_%2").arg(bankIndex).arg(field));
    QVERIFY(control);
    QCOMPARE(control->property("value").toInt(), qRound(initial * scale));
    view.show();
    control->forceActiveFocus();
    QTest::keyClick(&view, Qt::Key(key));
    QCOMPARE(control->property("value").toInt(), qRound(initial * scale));
    QCOMPARE(profileXml(doc.audioProfile(7)), before);
}

void VCAudioTriggers_Test::coherentPublication()
{
    Doc doc(nullptr, 1);
    auto *seven = addProfile(doc, 7);
    auto *twentyNine = addProfile(doc, 29);
    QVERIFY(seven && twentyNine && seven->channel() && twentyNine->channel());
    VCAudioTriggers first(&doc), second(&doc);
    first.setAudioProfileId(7);
    second.setAudioProfileId(29);
    AudioSnapshot frame;
    frame.sourceId = QStringLiteral("bank-fixture");
    frame.available = true;
    frame.sourceEpoch = 17;
    frame.frameSequence = 3;
    frame.lows = 0.18;
    frame.mids = 0.43;
    frame.highs = 0.87;
    frame.melLow.count = 2;
    frame.melLow.minHz = 30;
    frame.melLow.maxHz = 300;
    frame.melLow.centersHz[0] = 47;
    frame.melLow.centersHz[1] = 193;
    frame.melLow.processed[0] = 0.31;
    frame.melLow.processed[1] = 0.79;
    seven->channel()->injectSnapshot(frame);
    first.processAudioSnapshot();
    const auto applied = first.appliedAudio();
    const quint64 revision = applied.value("revision").toULongLong();
    QCOMPARE(applied.value("profileId").toUInt(), quint32(7));
    QCOMPARE(applied.value("banks").toList()[0].toMap().value("centersHz").toList(),
             QVariantList({47.0, 193.0}));
    QCOMPARE(first.melLowValues(), QVariantList({0.31, 0.79}));
    QVERIFY(first.updateBank(0, {{"agcDecay", 0.41}, {"maxHz", 470.0}}));
    QCOMPARE(first.appliedAudio().value("revision").toULongLong(), revision);
    QCOMPARE(first.appliedAudio().value("configuration").toList()[0].toMap().value("agcDecay").toDouble(), 0.17);
    QCOMPARE(first.bankConfiguration()[0].toMap().value("agcDecay").toDouble(), 0.41);
    frame.frameSequence++;
    frame.melLow.maxHz = 470;
    frame.melLow.centersHz[1] = 331;
    frame.melLow.processed[1] = 1.23;
    seven->channel()->injectSnapshot(frame);
    first.m_uiThrottleTimer.invalidate();
    first.updateAudioProfileSnapshotPowers();
    QVERIFY(first.appliedAudio().value("revision").toULongLong() > revision);
    QCOMPARE(first.appliedAudio().value("configuration").toList()[0].toMap().value("agcDecay").toDouble(), 0.41);
    QCOMPARE(first.melLowValues(), QVariantList({0.31, 1.23}));
    QCOMPARE(twentyNine->channelConfig().aubio.melBanks.low.post.agcDecay, 0.29);
    QCOMPARE(second.lowsPower(), 0.0);
    QVERIFY(doc.removeAudioProfile(7));
    QVERIFY(!first.analysisAvailable());
    QCOMPARE(first.lowsPower(), 0.0);
    QCOMPARE(first.resolvedProfileId(), AudioProfile::invalidId());
    QCOMPARE(second.resolvedProfileId(), quint32(29));
}

void VCAudioTriggers_Test::sparklineRecovery_data()
{
    QTest::addColumn<QString>("recovery");
    for (const auto &recovery : {"same-epoch", "new-epoch", "new-profile"})
        QTest::newRow(recovery) << QString(recovery);
}

void VCAudioTriggers_Test::sparklineRecovery()
{
    QFETCH(QString, recovery);
    Doc doc(nullptr, 0);
    auto *profile = addProfile(doc, 7);
    auto *other = addProfile(doc, 29);
    QVERIFY(profile && other);
    VCAudioTriggers widget(&doc);
    widget.setAudioProfileId(7);
    AudioSparklineItem sparkline;
    sparkline.setSize(QSizeF(400, 200));
    sparkline.setSource(&widget);
    QSignalSpy events(&widget, &VCAudioTriggers::audioEvents);
    AudioSnapshot frame;
    frame.sourceId = "sparkline-fixture";
    frame.sourceEpoch = 5;
    frame.frameSequence = 10;
    frame.available = true;
    frame.events.beat = 5;
    frame.events.kick = 2;
    frame.kickTrigger.value = 0.8;
    auto publish = [&] {
        profile->channel()->injectSnapshot(frame);
        widget.m_uiThrottleTimer.invalidate();
        widget.processAudioSnapshot();
        // Exercise the actual widget signal even when its visual throttle skips a frame.
        widget.updateAudioProfileSnapshotPowers();
    };
    auto lastBeat = [&] {
        return sparkline.m_beatHistory[(sparkline.m_nextWrite + sparkline.m_capacity - 1) % sparkline.m_capacity];
    };
    auto lastKick = [&] {
        return sparkline.m_kickHistory[(sparkline.m_nextWrite + sparkline.m_capacity - 1) % sparkline.m_capacity];
    };
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        frame.publishTimeNs = AudioRenderView::nowNs();
        publish();
        frame.frameSequence++;
        frame.events.beat++;
        frame.events.kick++;
        publish();
        QCOMPARE(lastBeat(), uint8_t(1));
        QVERIFY(lastKick() > 0);
        const int samples = sparkline.m_sampleCount;
        publish();
        QCOMPARE(sparkline.m_sampleCount, samples);

        frame.publishTimeNs = AudioRenderView::nowNs() - 300000000;
        publish();
        QVERIFY(!widget.analysisAvailable());
        QCOMPARE(sparkline.m_sampleCount, 0);
        QVERIFY(!sparkline.m_cursorReady);
        publish();
        QCOMPARE(sparkline.m_sampleCount, 0);
        events.clear();
        frame.frameSequence++;
        frame.events.beat += 3;
        frame.events.kick += 2;
        frame.publishTimeNs = AudioRenderView::nowNs();
        if (recovery == "new-epoch")
            frame.sourceEpoch++;
        else if (recovery == "new-profile")
        {
            std::swap(profile, other);
            profile->channel()->injectSnapshot(frame);
            widget.setAudioProfileId(profile->id());
        }
        publish();
        QVERIFY(widget.analysisAvailable());
        QCOMPARE(events.count(), 0);
        QCOMPARE(sparkline.m_sampleCount, 1);
        QCOMPARE(lastBeat(), uint8_t(0));
        QCOMPARE(lastKick(), uint8_t(0));
        publish();
        QCOMPARE(sparkline.m_sampleCount, 1);
        frame.frameSequence++;
        publish();
        QCOMPARE(sparkline.m_sampleCount, 2);
        QCOMPARE(lastBeat(), uint8_t(0));
        QCOMPARE(lastKick(), uint8_t(0));
        frame.frameSequence++;
        frame.events.beat++;
        frame.events.kick++;
        publish();
        QCOMPARE(events.count(), 1);
        QCOMPARE(events[0][1].toULongLong(), quint64(1));
        QCOMPARE(events[0][2].toULongLong(), quint64(1));
        QCOMPARE(lastBeat(), uint8_t(1));
        QVERIFY(lastKick() > 0);
    }
}

void VCAudioTriggers_Test::diagnosticsControl_data()
{
    QTest::addColumn<int>("selected");
    QTest::newRow("profile-7") << 7;
    QTest::newRow("profile-29") << 29;
}

void VCAudioTriggers_Test::diagnosticsControl()
{
    QFETCH(int, selected);
    Doc doc(nullptr, 0);
    auto capture = QSharedPointer<ConsumerCapture>::create();
    doc.m_inputCapture = capture;
    capture->setAnalyzer(doc.audioAnalyzer());
    auto *seven = addProfile(doc, 7);
    auto *twentyNine = addProfile(doc, 29);
    QVERIFY(seven && twentyNine);
    auto *profile = selected == 7 ? seven : twentyNine;
    auto *other = selected == 7 ? twentyNine : seven;
    VCAudioTriggers widget(&doc), sibling(&doc);
    widget.setAudioProfileId(selected);
    sibling.setAudioProfileId(other->id());
    widget.setMfccPower(selected == 7 ? 1.7 : 2.1);
    widget.setMfccScale(selected == 7 ? 0.7 : 1.3);
    widget.setTssAlpha(selected == 7 ? 2.3 : 1.9);
    widget.setTssBeta(selected == 7 ? 3.1 : 2.7);
    widget.setTssThreshold(selected == 7 ? 0.29 : 0.43);
    const QString before = profileXml(profile);
    const QString otherBefore = profileXml(other);
    QVERIFY(!profile->channelConfig().aubio.diagnosticsEnabled);
    const auto tuning = profile->channelConfig();

    qmlRegisterType<VCAudioTriggers>("org.qlcplus.classes", 1, 0, "VCAudioTriggers");
    qmlRegisterUncreatableType<Function>("org.qlcplus.classes", 1, 0, "QLCFunction", "Engine-owned");
    QQuickView view;
    view.resize(900, 900);
    view.rootContext()->setContextProperty("screenPixelDensity", 4.0);
    view.rootContext()->setContextProperty("mainView", view.contentItem());
    InputOutputManager ioManager(&view, &doc);
    FunctionManager functionManager(&view, &doc);
    QStringList warnings;
    connect(view.engine(), &QQmlEngine::warnings, this, [&warnings](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
            warnings.append(error.toString());
    });
    QQmlComponent component(view.engine(), QUrl("qrc:/VCAudioTriggersProperties.qml"));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {"width", 780}, {"widgetRef", QVariant::fromValue(&widget)}
    }));
    QVERIFY2(panel, qPrintable(component.errorString()));
    qobject_cast<QQuickItem *>(panel.get())->setParentItem(view.contentItem());
    auto *advanced = panel->findChild<QObject *>("audioAdvanced");
    QVERIFY(advanced);
    advanced->setProperty("isExpanded", true);
    auto *diagnostics = panel->findChild<QObject *>("audioDiagnostics");
    QVERIFY(diagnostics);
    diagnostics->setProperty("isExpanded", true);
    auto *control = panel->findChild<QQuickItem *>("audioDiagnosticsEnabled");
    auto *status = panel->findChild<QObject *>("audioDiagnosticsStatus");
    QVERIFY(control && status);
    view.show();
    int hop = 0;
    auto feed = [&] {
        for (int block = 0; block < 16; ++block, ++hop)
        {
            std::array<float, 500> samples{};
            for (int i = 0; i < 500; ++i)
            {
                const double time = double(hop * 500 + i) / 30000;
                samples[i] = float(0.08 * (std::sin(2 * M_PI * 437 * time)
                    + 0.3 * std::sin(2 * M_PI * 3101 * time)));
            }
            QVERIFY(capture->feed(samples));
        }
        widget.processAudioSnapshot();
        widget.updateAudioProfileSnapshotPowers();
        sibling.processAudioSnapshot();
    };
    feed();
    QVERIFY(widget.analysisAvailable());
    QCOMPARE(control->property("checked").toBool(), false);
    QVERIFY(status->property("label").toString().contains("disabled", Qt::CaseInsensitive));
    const quint64 otherRevision = other->channel()->snapshot().configRevision;
    for (bool enabled : {true, false, true})
    {
        const auto previous = profile->channel()->snapshot();
        control->forceActiveFocus();
        QTest::keyClick(&view, Qt::Key_Space);
        QCOMPARE(control->property("checked").toBool(), enabled);
        QCOMPARE(profile->channelConfig().aubio.diagnosticsEnabled, enabled);
        QCOMPARE(widget.diagnosticsEnabled(), enabled);
        QCOMPARE(profile->channel()->snapshot().configRevision, previous.configRevision);
        QCOMPARE(profile->channel()->snapshot().config.aubio.diagnosticsEnabled,
                 previous.config.aubio.diagnosticsEnabled);
        feed();
        const auto actual = profile->channel()->snapshot();
        QVERIFY(actual.configRevision > previous.configRevision);
        QCOMPARE(actual.config.aubio.diagnosticsEnabled, enabled);
        QCOMPARE(widget.appliedAudio()["diagnosticsEnabled"].toBool(), enabled);
        double mfccMax = 0;
        for (double value : actual.mfcc)
            mfccMax = std::max(mfccMax, std::abs(value));
        QCOMPARE(mfccMax > 0, enabled);
        QCOMPARE(actual.tss.binCount > 0, enabled);
        QCOMPARE(widget.tssBinCount(), actual.tss.binCount);
        QCOMPARE(profileXml(other), otherBefore);
        QCOMPARE(other->channel()->snapshot().configRevision, otherRevision);
        QCOMPARE(other->channel()->snapshot().tss.binCount, 0);
        for (double value : other->channel()->snapshot().mfcc)
            QCOMPARE(value, 0.0);
        QCOMPARE(actual.config.aubio.mfccPower, tuning.aubio.mfccPower);
        QCOMPARE(actual.config.aubio.mfccScale, tuning.aubio.mfccScale);
        QCOMPARE(actual.config.aubio.tssAlpha, tuning.aubio.tssAlpha);
        QCOMPARE(actual.config.aubio.tssBeta, tuning.aubio.tssBeta);
        QCOMPARE(actual.config.aubio.tssThreshold, tuning.aubio.tssThreshold);
        QVERIFY(actual.config.aubio.melBanks == tuning.aubio.melBanks);
        QCOMPARE(actual.config.noiseGate.thresholdDb, tuning.noiseGate.thresholdDb);
        AudioProfile restored(0);
        const QString saved = profileXml(profile);
        QXmlStreamReader reader(saved);
        QVERIFY(reader.readNextStartElement());
        QVERIFY(restored.loadXML(reader));
        QCOMPARE(profileXml(&restored), saved);
        QCOMPARE(restored.channelConfig().aubio.diagnosticsEnabled, enabled);
        if (!enabled)
        {
            QCOMPARE(saved, before);
            QVERIFY(status->property("label").toString().contains("disabled", Qt::CaseInsensitive));
        }
    }
    widget.setAudioProfileId(other->id());
    QCOMPARE(control->property("checked").toBool(), false);
    widget.setAudioProfileId(profile->id());
    QCOMPARE(control->property("checked").toBool(), true);
    profile->setAudioSource(AudioProfile::OscSynesthesia);
    widget.audioSourceChanged();
    QVERIFY(!control->isEnabled());
    QVERIFY(status->property("label").toString().contains("disabled", Qt::CaseInsensitive));
    QVERIFY(status->property("label").toString().contains("OSC"));
    QVERIFY(profile->channelConfig().aubio.diagnosticsEnabled);
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
}

void VCAudioTriggers_Test::editorLongStepPreview()
{
    Doc doc(nullptr, 1);
    auto capture = QSharedPointer<ConsumerCapture>::create();
    doc.m_inputCapture = capture;
    capture->setAnalyzer(doc.audioAnalyzer());
    auto *profile = addProfile(doc, 7);
    QVERIFY(profile);
    doc.setActiveAudioProfileId(7);
    auto *group = new FixtureGroup(&doc);
    group->setSize(QSize(3, 1));
    QVERIFY(doc.addFixtureGroup(group));
    for (int x = 0; x < 3; ++x)
    {
        auto *fixture = new Fixture(&doc);
        fixture->setChannels(1);
        fixture->setAddress(x);
        QVERIFY(doc.addFixture(fixture));
        QVERIFY(group->assignHead(QLCPoint(x, 0), GroupHead(fixture->id(), 0)));
    }
    auto *matrix = new HUEMatrix(&doc);
    QVERIFY(doc.addFunction(matrix));
    auto *script = new HUEScript(&doc);
    QVERIFY(script->load(QFINDTESTDATA("preview-cadence.js")));
    matrix->setAlgorithm(script);
    matrix->setFixtureGroup(group->id());
    matrix->setDuration(10000);
    matrix->setBeatEffect(HUEMatrix::BeatEffectOff);
    QQuickView view;
    HUEMatrixEditor editor(&view, &doc);
    QSignalSpy previews(&editor, &HUEMatrixEditor::previewDataChanged);
    AudioSnapshot frame;
    frame.sourceId = "editor-timer-fixture";
    frame.sourceEpoch = 7;
    frame.available = true;
    auto publish = [&](double low, double mid, double high) {
        frame.frameSequence++;
        frame.publishTimeNs = AudioRenderView::nowNs();
        frame.lows = low;
        frame.mids = mid;
        frame.highs = high;
        profile->channel()->injectSnapshot(frame);
    };
    auto colors = [&](double low, double mid, double high) {
        return QVariantList{
            QColor(HUEColor::hsvToRgb(0, 1, float(low))),
            QColor(HUEColor::hsvToRgb(1.0f / 3, 1, float(mid))),
            QColor(HUEColor::hsvToRgb(2.0f / 3, 1, float(high)))};
    };
    publish(0.19, 0.47, 0.83);
    editor.setFunctionID(matrix->id());
    QVERIFY(editor.m_previewTimer->isActive());
    QCOMPARE(editor.m_previewTimer->interval(), int(MasterTimer::tick()));
    QTRY_COMPARE_WITH_TIMEOUT(editor.previewData(), colors(0.19, 0.47, 0.83), 1000);
    const int initialPreviews = previews.count();
    publish(0.73, 0.31, 0.59);
    QTRY_COMPARE_WITH_TIMEOUT(editor.previewData(), colors(0.73, 0.31, 0.59), 1000);
    QVERIFY(previews.count() > initialPreviews);
    QVERIFY(script->property("calls").toInt() >= 2);
    QCOMPARE(editor.m_previewStepHandler->currentStepIndex(), 0);
    QVERIFY(editor.m_previewElapsed < matrix->duration());

    matrix->preRun(doc.masterTimer());
    matrix->write(doc.masterTimer(), {});
    const int committedCalls = script->property("calls").toInt();
    const int livePreviews = previews.count();
    publish(0.11, 0.89, 0.37);
    QTRY_VERIFY_WITH_TIMEOUT(previews.count() >= livePreviews + 3, 1000);
    QCOMPARE(script->property("calls").toInt(), committedCalls);
    QCOMPARE(editor.previewData(), colors(0.73, 0.31, 0.59));
    QCOMPARE(editor.m_previewStepHandler->currentStepIndex(), 0);
    QVERIFY(editor.m_previewElapsed < matrix->duration());
    matrix->write(doc.masterTimer(), {});
    const int nextCalls = script->property("calls").toInt();
    QVERIFY(nextCalls > committedCalls);
    QTRY_COMPARE_WITH_TIMEOUT(editor.previewData(), colors(0.11, 0.89, 0.37), 1000);
    QCOMPARE(script->property("calls").toInt(), nextCalls);
    editor.setFunctionID(Function::invalidId());
    matrix->postRun(doc.masterTimer(), {});
}

void VCAudioTriggers_Test::nativeConsumers_data()
{
    QTest::addColumn<int>("selected");
    QTest::newRow("active-7") << 7;
    QTest::newRow("active-29") << 29;
}

void VCAudioTriggers_Test::nativeConsumers()
{
    QFETCH(int, selected);
    Doc doc(nullptr, 1);
    auto capture = QSharedPointer<ConsumerCapture>::create();
    doc.m_inputCapture = capture;
    capture->setAnalyzer(doc.audioAnalyzer());
    auto *seven = addProfile(doc, 7);
    auto *twentyNine = addProfile(doc, 29);
    QVERIFY(seven && twentyNine);
    VCAudioTriggers first(&doc), second(&doc);
    first.setAudioProfileId(7);
    second.setAudioProfileId(29);
    doc.setActiveAudioProfileId(quint32(selected));
    QVERIFY(doc.hueScriptsCache()->load(QDir(QFINDTESTDATA("../../../resources/huescripts")), true));
    HUEScript hue(&doc);
    QVERIFY(hue.load(QFINDTESTDATA("consumer-view.js")));
    HUEScript spectrum(&doc);
    QVERIFY(spectrum.load(QFINDTESTDATA("../../../resources/huescripts/audiospectrum.js")));
    RGBAudio builtin(&doc);
    auto *fixture = new Fixture(&doc);
    fixture->setChannels(1);
    fixture->setAddress(0);
    fixture->setUniverse(0);
    QVERIFY(doc.addFixture(fixture, 19));
    RGBMap previousPixels;
    quint64 previousRevision = 0;
    for (int phase = 0; phase < 3; ++phase)
    {
        if (phase == 2)
        {
            QVERIFY(second.updateBank(2, {{"bands", 21}, {"maxHz", 12000.0}}));
            QVERIFY(first.updateBank(2, {{"bands", 13}, {"maxHz", 9000.0}}));
        }
        for (int hop = 0; hop < 40; ++hop)
        {
            std::array<float, 500> samples{};
            for (int i = 0; i < 500; ++i)
            {
                const double time = double((phase * 40 + hop) * 500 + i) / 30000;
                const double frequency = phase == 1 ? 6000 + hop * 25 : 400 + hop * 12;
                const double amplitude = phase == 0 ? 0.001 : 0.06;
                samples[i] = float(amplitude * (std::sin(2 * M_PI * frequency * time)
                    + 0.3 * std::sin(2 * M_PI * 1.31 * frequency * time)));
            }
            QVERIFY(capture->feed(samples));
        }
        QTest::qWait(40);
        first.processAudioSnapshot();
        second.processAudioSnapshot();
        for (auto *widget : {&first, &second})
        {
            const auto snapshot = doc.audioSnapshot(widget->audioProfileId());
            QCOMPARE(widget->lowsPower(), snapshot.lows);
            QCOMPARE(widget->midsPower(), snapshot.mids);
            QCOMPARE(widget->highsPower(), snapshot.highs);
            QCOMPARE(widget->melHighValues().size(), snapshot.melHigh.count);
            for (int bin = 0; bin < snapshot.melHigh.count; ++bin)
                QCOMPARE(widget->melHighValues()[bin].toDouble(), snapshot.melHigh.processed[bin]);
            QCOMPARE(widget->appliedAudio()["profileId"].toUInt(), snapshot.profileId);
        }
        const auto snapshot = doc.audioSnapshot();
        const auto expected = audioViewToVariant(AudioRenderView::fromSnapshot(snapshot, AudioRenderView::nowNs()));
        RGBMap map;
        hue.rgbMap({3, 1}, 0xffffff, 0, map);
        QVERIFY(!map.isEmpty());
        const auto observed = QJsonDocument::fromJson(hue.property("snapshot").toUtf8()).object().toVariantMap();
        for (const QString &key : {"version", "profileId", "sourceId", "sourceEpoch", "frameSequence",
                                   "configRevision", "available", "low", "mid", "high", "banks"})
            QCOMPARE(QJsonValue::fromVariant(observed[key]), QJsonValue::fromVariant(expected[key]));
        QCOMPARE(map[0][0], HUEColor::hsvToRgb(0, 1, float(snapshot.lows)));
        spectrum.rgbMap({37, 1}, 0xffffff, 0, map);
        if (phase > 0 || selected == 7)
            QVERIFY(std::any_of(map[0].cbegin(), map[0].cend(), [](uint rgb) { return (rgb & 0xffffff) != 0; }));
        if (phase == 1)
            QVERIFY(map != previousPixels);
        previousPixels = map;
        builtin.rgbMap({37, 8}, 0xff0000, 0, map);
        for (int x = 0; x < 37; ++x)
        {
            int lit = 0;
            for (const auto &row : map)
                lit += (row[x] & 0xffffff) != 0;
            const auto view = AudioRenderView::fromSnapshot(snapshot, AudioRenderView::nowNs());
            QCOMPARE(lit, int(std::ceil(audioFrequency(view, x, 37) * 8)));
        }
        const QByteArray expectedJson = QJsonDocument::fromVariant(expected).toJson(QJsonDocument::Compact);
        const QString code = QString::fromUtf8(R"JS(
var expected = %1, actual = Engine.getAudioSnapshot();
["version","profileId","sourceId","sourceEpoch","frameSequence","configRevision",
 "available","low","mid","high","banks"].forEach(function(key) {
    if (JSON.stringify(actual[key]) !== JSON.stringify(expected[key]))
        throw new Error("snapshot mismatch: " + key);
});
if (Engine.getAudioLevel() !== Math.floor(actual.volume.rawRms * 255))
    throw new Error("raw level mismatch");
for (var i = 0; i < 3; ++i)
    if (Engine.getAudioFrequency(i, 3) !== Math.floor([actual.low, actual.mid, actual.high][i] * 255))
        throw new Error("legacy band mismatch");
for (var i = 0; i < actual.banks.full.count; ++i)
    if (Engine.getAudioFrequency(i, actual.banks.full.count) !==
        Math.floor(Math.max(0, Math.min(1, actual.banks.full.processed[i])) * 255))
        throw new Error("legacy full spectrum mismatch");
var again = Engine.getAudioSnapshot();
["onset","beat","kick","bar"].forEach(function(key) {
    if (again.events.delta[key] !== 0) throw new Error("repeated event: " + key);
});
Engine.setFixture(19, 0, 173);
)JS").arg(QString::fromUtf8(expectedJson));
        const int subscribers = capture->subscribers();
        {
            ScriptRunner runner(&doc, nullptr, code);
            runner.execute();
            QVERIFY(runner.wait(3000));
            QCOMPARE(runner.m_fixtureValueQueue.size(), 1);
            QCOMPARE(runner.m_fixtureValueQueue.head().m_value, uchar(173));
        }
        QCOMPARE(capture->subscribers(), subscribers);
        if (phase == 0)
        {
            QVERIFY(!doc.audioSnapshot(7).noiseGateClosed);
            QVERIFY(doc.audioSnapshot(29).noiseGateClosed);
            QCOMPARE(snapshot.events.beat, uint64_t(0));
        }
        if (phase == 2)
        {
            QVERIFY(snapshot.configRevision > previousRevision);
            QCOMPARE(snapshot.melHigh.count, selected == 7 ? 13 : 21);
        }
        previousRevision = snapshot.configRevision;
    }
    QTest::qWait(260);
    first.processAudioSnapshot();
    second.processAudioSnapshot();
    QVERIFY(!first.analysisAvailable());
    QVERIFY(!second.analysisAvailable());
    QCOMPARE(first.lowsPower(), 0.0);
    QCOMPARE(second.highsPower(), 0.0);
    QCOMPARE(first.melHighValues().size(), 13);
    QCOMPARE(second.melHighValues().size(), 21);
    for (const auto &value : first.melHighValues())
        QCOMPARE(value.toDouble(), 0.0);
    for (const auto &value : second.melHighValues())
        QCOMPARE(value.toDouble(), 0.0);
    RGBMap staleMap;
    hue.rgbMap({3, 1}, 0xffffff, 0, staleMap);
    for (uint rgb : staleMap[0])
        QCOMPARE(rgb & 0xffffff, 0u);
    QVERIFY(!QJsonDocument::fromJson(hue.property("snapshot").toUtf8())
                 .object()["available"].toBool());
    QVERIFY(doc.removeAudioProfile(quint32(selected)));
    auto &removed = selected == 7 ? first : second;
    QVERIFY(!removed.analysisAvailable());
    QCOMPARE(removed.lowsPower(), 0.0);
}

void VCAudioTriggers_Test::eventDelivery_data()
{
    QTest::addColumn<int>("rate");
    QTest::addColumn<bool>("osc");
    for (int rate : {25, 30, 50, 60})
        for (bool osc : {false, true})
            QTest::newRow(qPrintable(QString("%1Hz-%2").arg(rate).arg(osc ? "OSC" : "microphone"))) << rate << osc;
}

void VCAudioTriggers_Test::eventDelivery()
{
    QFETCH(int, rate);
    QFETCH(bool, osc);
    Doc doc(nullptr, 1);
    auto *profile = addProfile(doc, 7);
    QVERIFY(profile && profile->channel());
    VCAudioTriggers first(&doc), second(&doc);
    first.setAudioProfileId(7);
    second.setAudioProfileId(7);
    first.m_captureEnabled = second.m_captureEnabled = true;
    QSignalSpy firstEvents(&first, &VCAudioTriggers::audioEvents);
    QSignalSpy secondEvents(&second, &VCAudioTriggers::audioEvents);
    AudioSnapshot frame;
    frame.sourceId = osc ? QStringLiteral("osc-test") : QStringLiteral("microphone-test");
    frame.available = true;
    frame.profileId = 7;
    frame.sourceEpoch = 1;
    frame.frameSequence = 1;
    auto read = [osc](VCAudioTriggers &widget) {
        if (osc)
            widget.slotOscSnapshotInjected();
        else
            widget.slotAubioDataReady(AubioResults{}, 0);
    };
    profile->channel()->injectSnapshot(frame);
    read(first);
    read(second);
    for (int tick = 1; tick <= 120; ++tick)
    {
        frame.frameSequence++;
        frame.events.onset += (tick % 3 == 0);
        frame.events.beat += (tick % 10 == 0);
        frame.events.kick += (tick % 7 == 0);
        frame.events.bar += (tick % 40 == 0);
        profile->channel()->injectSnapshot(frame);
        // 120 ms stall and a second subscriber with a distinct, jittered cadence.
        if (!(tick >= 43 && tick <= 50) && (tick * rate / 60 != (tick - 1) * rate / 60))
        {
            read(first);
            read(first);
        }
        if (tick % 4 == 0 || tick % 7 == 0)
            read(second);
    }
    read(first);
    read(second);
    for (const QSignalSpy *spy : {&firstEvents, &secondEvents})
    {
        quint64 sums[4] = {};
        for (const auto &args : *spy)
            for (int event = 0; event < 4; ++event)
                sums[event] += args[event].toULongLong();
        QCOMPARE(sums[0], quint64(40));
        QCOMPARE(sums[1], quint64(12));
        QCOMPARE(sums[2], quint64(17));
        QCOMPARE(sums[3], quint64(3));
    }
    firstEvents.clear();
    secondEvents.clear();
    frame.sourceEpoch++;
    frame.frameSequence = 1;
    frame.events.beat = 600;
    frame.events.onset = 800;
    profile->channel()->injectSnapshot(frame);
    read(first);
    read(second);
    QCOMPARE(firstEvents.count(), 0);
    QCOMPARE(secondEvents.count(), 0);
    frame.available = false;
    frame.frameSequence++;
    profile->channel()->injectSnapshot(frame);
    read(first);
    QVERIFY(!first.analysisAvailable());
    QCOMPARE(first.lowsPower(), 0.0);
    QVERIFY(!first.beatActive());
    frame.available = true;
    frame.frameSequence++;
    frame.events.onset += 3;
    frame.events.beat += 2;
    profile->channel()->injectSnapshot(frame);
    read(first);
    QCOMPARE(firstEvents.count(), 0);
    frame.frameSequence++;
    frame.events.onset++;
    profile->channel()->injectSnapshot(frame);
    read(first);
    QCOMPARE(firstEvents.count(), 1);
    QCOMPARE(firstEvents[0][0].toULongLong(), quint64(1));
    QCOMPARE(firstEvents[0][1].toULongLong(), quint64(0));
    first.m_captureEnabled = second.m_captureEnabled = false;
}

void VCAudioTriggers_Test::mappedActions_data()
{
    eventDelivery_data();
}

void VCAudioTriggers_Test::mappedActions()
{
    QFETCH(int, rate);
    QFETCH(bool, osc);
    Doc doc(nullptr, 0);
    auto *profile = addProfile(doc, 7);
    QVERIFY(profile);
    QQuickView view;
    FixtureManager fixtureManager(&view, &doc);
    FunctionManager functionManager(&view, &doc);
    ContextManager contextManager(&view, &doc, &fixtureManager, &functionManager);
    VirtualConsole vc(&view, &doc, &contextManager);
    auto cleanupTardis = [](Tardis *tardis) {
        delete tardis;
        Tardis::s_instance = nullptr;
    };
    std::unique_ptr<Tardis, decltype(cleanupTardis)> tardis(
        new Tardis(&view, &doc, nullptr, &fixtureManager, &functionManager,
                   &contextManager, nullptr, nullptr, &vc), cleanupTardis);
    tardis->m_busy = true;
    auto *fixture = new Fixture(&doc);
    fixture->setChannels(1);
    QVERIFY(doc.addFixture(fixture));
    Scene *functions[2], *buttonFunctions[2];
    for (int i = 0; i < 2; ++i)
    {
        functions[i] = new Scene(&doc);
        buttonFunctions[i] = new Scene(&doc);
        QVERIFY(doc.addFunction(functions[i]));
        QVERIFY(doc.addFunction(buttonFunctions[i]));
        functions[i]->setValue(fixture->id(), 0, uchar(i == 0 ? 73 : 191));
    }
    MappedButton firstButton(&doc, buttonFunctions[0]->id());
    MappedButton secondButton(&doc, buttonFunctions[1]->id());
    vc.addWidgetToMap(&firstButton);
    vc.addWidgetToMap(&secondButton);
    VCAudioTriggers first(&doc, &vc), second(&doc, &vc);
    first.setID(107);
    second.setID(129);
    VCAudioTriggers *widgets[] = {&first, &second};
    MappedButton *buttons[] = {&firstButton, &secondButton};
    for (int i = 0; i < 2; ++i)
    {
        widgets[i]->setAudioProfileId(7);
        widgets[i]->m_snapshotTimer->stop();
        widgets[i]->selectBarForEditing(i == 0 ? VCAudioTriggers::BandBeat : VCAudioTriggers::BandKick);
        widgets[i]->setBarType(VCAudioTriggers::FunctionBar);
        widgets[i]->setBarFunction(functions[i]->id());
        widgets[i]->selectBarForEditing(i == 0 ? VCAudioTriggers::BandKick : VCAudioTriggers::BandBeat);
        widgets[i]->setBarType(VCAudioTriggers::VCWidgetBar);
        widgets[i]->setBarWidget(buttons[i]->id());
        widgets[i]->m_captureEnabled = true;
    }
    QSignalSpy firstStates(&firstButton, &VCButton::stateChanged);
    QSignalSpy secondStates(&secondButton, &VCButton::stateChanged);
    QSignalSpy *states[] = {&firstStates, &secondStates};
    AudioSnapshot frame;
    frame.sourceId = osc ? "osc-mapped" : "microphone-mapped";
    frame.sourceEpoch = 1;
    frame.frameSequence = 1;
    frame.available = true;
    frame.kickTrigger.value = 0.8;
    quint64 previousFunctionEvents[2] = {}, previousButtonEvents[2] = {};
    auto read = [&](int i) {
        const quint64 functionEvents = i == 0 ? frame.events.beat : frame.events.kick;
        const quint64 buttonEvents = i == 0 ? frame.events.kick : frame.events.beat;
        const quint64 newButtonEvents = buttonEvents - previousButtonEvents[i];
        const int before = states[i]->count();
        // Advance real scene time so a new start must reset it, even after a held pulse.
        functions[i]->write(doc.masterTimer(), {});
        const quint32 elapsed = functions[i]->elapsed();
        QVERIFY(elapsed > 0);
        if (osc)
            widgets[i]->slotOscSnapshotInjected();
        else
            widgets[i]->slotAubioDataReady(AubioResults{}, 0);
        QCOMPARE(functions[i]->stopped(), functionEvents == previousFunctionEvents[i]);
        QCOMPARE(functions[i]->elapsed(), functionEvents > previousFunctionEvents[i] ? quint32(0) : elapsed);
        QCOMPARE(buttons[i]->state(), newButtonEvents ? VCButton::Active : VCButton::Inactive);
        QCOMPARE(buttonFunctions[i]->stopped(), newButtonEvents == 0);
        int activations = 0;
        for (int event = before; event < states[i]->count(); ++event)
            activations += states[i]->at(event)[0].toInt() == VCButton::Active;
        QCOMPARE(quint64(activations), newButtonEvents);
        const int after = states[i]->count();
        const bool stopped = functions[i]->stopped();
        const quint32 afterElapsed = functions[i]->elapsed();
        widgets[i]->processAudioSnapshot();
        QCOMPARE(states[i]->count(), after);
        QCOMPARE(functions[i]->stopped(), stopped);
        QCOMPARE(functions[i]->elapsed(), afterElapsed);
        previousFunctionEvents[i] = functionEvents;
        previousButtonEvents[i] = buttonEvents;
    };
    profile->channel()->injectSnapshot(frame);
    read(0);
    read(1);
    for (int tick = 1; tick <= 120; ++tick)
    {
        frame.frameSequence++;
        frame.events.beat += tick % 10 == 0;
        frame.events.kick += tick % 7 == 0;
        profile->channel()->injectSnapshot(frame);
        if (!(tick >= 43 && tick <= 50) && tick * rate / 60 != (tick - 1) * rate / 60)
            read(0);
        if (tick % 4 == 0 || tick % 7 == 0)
            read(1);
    }
    // A fresh no-event frame releases targets, rather than re-reading the last pulse.
    frame.frameSequence++;
    profile->channel()->injectSnapshot(frame);
    read(0);
    read(1);
    for (int i = 0; i < 2; ++i)
    {
        int activations = 0;
        for (const auto &event : *states[i])
            activations += event[0].toInt() == VCButton::Active;
        QCOMPARE(activations, i == 0 ? 17 : 12);
        states[i]->clear();
    }
    frame.available = false;
    profile->channel()->injectSnapshot(frame);
    first.processAudioSnapshot();
    second.processAudioSnapshot();
    frame.available = true;
    frame.frameSequence++;
    frame.events.beat += 5;
    frame.events.kick += 3;
    profile->channel()->injectSnapshot(frame);
    for (int i = 0; i < 2; ++i)
    {
        widgets[i]->processAudioSnapshot();
        QVERIFY(functions[i]->stopped());
        QVERIFY(buttonFunctions[i]->stopped());
        QCOMPARE(buttons[i]->state(), VCButton::Inactive);
        QCOMPARE(states[i]->count(), 0);
        previousFunctionEvents[i] = i == 0 ? frame.events.beat : frame.events.kick;
        previousButtonEvents[i] = i == 0 ? frame.events.kick : frame.events.beat;
    }
    frame.frameSequence++;
    frame.events.beat++;
    frame.events.kick++;
    profile->channel()->injectSnapshot(frame);
    read(0);
    read(1);
    frame.available = false;
    profile->channel()->injectSnapshot(frame);
    first.processAudioSnapshot();
    second.processAudioSnapshot();
    QVERIFY(functions[0]->stopped() && functions[1]->stopped());
    QVERIFY(buttonFunctions[0]->stopped() && buttonFunctions[1]->stopped());
    first.m_captureEnabled = second.m_captureEnabled = false;
    vc.removeWidgetFromMap(&firstButton);
    vc.removeWidgetFromMap(&secondButton);
}

void VCAudioTriggers_Test::mappingRoundTrip_data()
{
    QTest::addColumn<int>("source");
    QTest::addColumn<bool>("withChannels");
    QTest::addColumn<bool>("formatted");
    for (int source : {int(VCAudioTriggers::BandBeat), int(VCAudioTriggers::BandKick)})
        for (bool withChannels : {false, true})
            for (bool formatted : {false, true})
                QTest::newRow(qPrintable(QString("%1-%2-%3")
                    .arg(source == VCAudioTriggers::BandBeat ? "beat" : "kick")
                    .arg(withChannels ? "channels" : "empty")
                    .arg(formatted ? "formatted" : "compact")))
                    << source << withChannels << formatted;
}

void VCAudioTriggers_Test::mappingRoundTrip()
{
    QFETCH(int, source);
    QFETCH(bool, withChannels);
    QFETCH(bool, formatted);
    Doc doc(nullptr, 1);
    QVERIFY(addProfile(doc, 29));
    auto *fixture = new Fixture(&doc);
    fixture->setChannels(512);
    fixture->setAddress(0);
    fixture->setUniverse(0);
    QVERIFY(doc.addFixture(fixture, 19));
    const QList<SceneValue> channels = withChannels
        ? QList<SceneValue>{SceneValue(19, 0), SceneValue(19, 117), SceneValue(19, 511)}
        : QList<SceneValue>{};
    VCAudioTriggers widget(&doc);
    widget.setAudioProfileId(29);
    widget.setCaption("Audio mapping round trip");
    widget.setGeometry(QRect(17, 29, 310, 190));
    widget.setBackgroundColor(QColor("#17395b"));
    widget.selectBarForEditing(source);
    widget.setBarType(VCAudioTriggers::DMXBar);
    widget.setBarDmxChannels(channels);
    widget.setBarDmxScale(1.73);
    widget.setBarDmxFloor(37);
    widget.setBarBeatHoldMs(143);
    const int sibling = source == VCAudioTriggers::BandBeat
        ? VCAudioTriggers::BandKick : VCAudioTriggers::BandBeat;
    widget.selectBarForEditing(sibling);
    widget.setBarType(VCAudioTriggers::DMXBar);
    widget.setBarDmxScale(0.62);
    widget.setBarDmxFloor(19);
    widget.setBarBeatHoldMs(287);
    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.setAutoFormatting(formatted);
    writer.writeStartElement("VirtualConsole");
    QVERIFY(widget.saveXML(&writer));
    writer.writeEmptyElement("FollowingWidget");
    writer.writeEndElement();
    VCAudioTriggers restored(&doc);
    QBuffer buffer;
    buffer.setData(xml.toUtf8());
    QVERIFY(buffer.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&buffer);
    QVERIFY(reader.readNextStartElement());
    QVERIFY(reader.readNextStartElement());
    QVERIFY(restored.loadXML(reader));
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    QCOMPARE(reader.name(), QString("AudioTriggers"));
    QVERIFY(reader.isEndElement());
    QVERIFY(reader.readNextStartElement());
    QCOMPARE(reader.name(), QString("FollowingWidget"));
    QCOMPARE(restored.audioProfileId(), quint32(29));
    QCOMPARE(restored.caption(), widget.caption());
    QCOMPARE(restored.geometry(), widget.geometry());
    QCOMPARE(restored.backgroundColor(), widget.backgroundColor());
    const QVariantMap restoredBar = restored.barsInfo()[source].toMap();
    QCOMPARE(restoredBar.value("type").toInt(), int(VCAudioTriggers::DMXBar));
    QCOMPARE(restoredBar.value("dmxScale").toDouble(), 1.73);
    QCOMPARE(restoredBar.value("dmxFloor").toInt(), 37);
    QCOMPARE(restoredBar.value("beatHoldMs").toInt(), 143);
    QCOMPARE(restored.m_bandMappings[source].dmxChannels, channels);
    QCOMPARE(restored.m_bandMappings[source].absDmxChannels,
             withChannels ? QList<int>({0, 117, 511}) : QList<int>());
    const QVariantMap restoredSibling = restored.barsInfo()[sibling].toMap();
    QCOMPARE(restoredSibling.value("type").toInt(), int(VCAudioTriggers::DMXBar));
    QCOMPARE(restoredSibling.value("dmxScale").toDouble(), 0.62);
    QCOMPARE(restoredSibling.value("dmxFloor").toInt(), 19);
    QCOMPARE(restoredSibling.value("beatHoldMs").toInt(), 287);
}

QTEST_MAIN(VCAudioTriggers_Test)
