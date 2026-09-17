/*
  Q Light Controller Plus - Unit test
*/

#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickView>
#include <QDir>
#include <QValidator>
#include <QFile>
#include <QTemporaryDir>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QLocale>
#include <memory>
#include <cmath>

#include "matrixscriptfloatcontrols_test.h"

#define private public
#ifdef QT_QML_LIB
  #include "rgbscriptv4.h"
#else
  #include "rgbscript.h"
#endif
#undef private

#include "doc.h"
#include "rgbmatrix.h"
#include "huematrix.h"
#include "rgbscriptscache.h"
#include "rgbmatrixeditor.h"
#include "huematrixeditor.h"

namespace
{
class FloatControlCollector final : public QQuickItem
{
    Q_OBJECT

public:
    struct FloatCall
    {
        QString propName;
        double currentValue;
        QVariant minValue;
        QVariant maxValue;
        QVariant stepValue;
        QVariant decimalsValue;
        QVariant boundedControlValue;
    };

    QList<FloatCall> floatCalls;

    Q_INVOKABLE void addLabel(const QVariant &) {}
    Q_INVOKABLE void addComboBox(const QVariant &, const QVariant &, const QVariant &) {}
    Q_INVOKABLE void addSpinBox(const QVariant &, const QVariant &, const QVariant &, const QVariant &) {}
    Q_INVOKABLE void addTextEdit(const QVariant &, const QVariant &) {}

    Q_INVOKABLE void addDoubleSpinBox(const QVariant &propName, const QVariant &currentValue,
                                      const QVariant &minValue, const QVariant &maxValue,
                                      const QVariant &stepValue, const QVariant &decimalsValue,
                                      const QVariant &boundedControlValue)
    {
        FloatCall call;
        call.propName = propName.toString();
        call.currentValue = currentValue.toDouble();
        call.minValue = minValue;
        call.maxValue = maxValue;
        call.stepValue = stepValue;
        call.decimalsValue = decimalsValue;
        call.boundedControlValue = boundedControlValue;
        floatCalls.append(call);
    }
};

QString f(double value)
{
    return QString::number(value, 'g', 15);
}

QString buildFloatScript(const QString &name, const QString &propertyDescriptor, double initialValue)
{
    return QStringLiteral(
               "(function() {\n"
               " var algo = new Object;\n"
               " var gain = %1;\n"
               " algo.apiVersion = 2;\n"
               " algo.name = '%2';\n"
               " algo.author = 'QLC+ Unit Test';\n"
               " algo.acceptColors = 0;\n"
               " algo.properties = ['%3'];\n"
               " algo.rgbMap = function(width, height, rgb, step) {\n"
               "   var out = [];\n"
               "   for (var y = 0; y < height; y++) {\n"
               "     var row = [];\n"
               "     for (var x = 0; x < width; x++) row.push(0);\n"
               "     out.push(row);\n"
               "   }\n"
               "   return out;\n"
               " };\n"
               " algo.rgbMapStepCount = function(width, height) { return 1; };\n"
               " algo.setGain = function(value) { gain = parseFloat(value); };\n"
               " algo.getGain = function() { return gain; };\n"
               " return algo;\n"
               "})();")
        .arg(f(initialValue), name, propertyDescriptor);
}

QString boundedFloatDescriptor(double minBound, double maxBound)
{
    return QStringLiteral("name:gain|display:Gain|type:float|values:%1,%2|write:setGain|read:getGain")
        .arg(f(minBound), f(maxBound));
}

RGBScript *makeScript(Doc *doc, const QString &name, const QString &propertyDescriptor, double initialValue)
{
    auto *script = new RGBScript(doc);
    script->m_fileName = QStringLiteral("inline_bounded_float_test.js");
    script->m_contents = buildFloatScript(name, propertyDescriptor, initialValue);
    if (!script->evaluate())
    {
        delete script;
        return nullptr;
    }
    return script;
}

template <typename EditorT, typename MatrixT>
FloatControlCollector::FloatCall collectFloatCall(Doc *doc, QQuickView *view,
                                                  double minBound, double maxBound, double initialValue)
{
    std::unique_ptr<MatrixT> matrix(new MatrixT(doc));
    matrix->setName(QStringLiteral("Float controls matrix"));

    std::unique_ptr<RGBScript> script(makeScript(doc, QStringLiteral("Bounded Float Test"),
                                                 boundedFloatDescriptor(minBound, maxBound),
                                                 initialValue));
    if (!script)
        return {};

    matrix->setAlgorithm(script.release());
    if (!doc->addFunction(matrix.get()))
        return {};
    const quint32 functionId = matrix->id();
    matrix.release();

    EditorT editor(view, doc);
    editor.setFunctionID(functionId);

    FloatControlCollector collector;
    editor.createScriptObjects(&collector);

    doc->deleteFunction(functionId);
    if (collector.floatCalls.isEmpty())
        return {};
    return collector.floatCalls.first();
}

template <typename EditorT, typename MatrixT>
QPair<QString, QString> writeAuthoredFloat(Doc *doc, QQuickView *view,
                                           double minBound, double maxBound,
                                           double initialValue, double authoredValue)
{
    MatrixT *matrix = new MatrixT(doc);
    matrix->setName(QStringLiteral("Authored float matrix"));

    std::unique_ptr<RGBScript> script(makeScript(doc, QStringLiteral("Authored Float Test"),
                                                 boundedFloatDescriptor(minBound, maxBound),
                                                 initialValue));
    if (!script)
        return {};

    matrix->setAlgorithm(script.release());
    if (!doc->addFunction(matrix))
        return {};
    const quint32 functionId = matrix->id();

    EditorT editor(view, doc);
    editor.setFunctionID(functionId);
    editor.setScriptFloatProperty(QStringLiteral("gain"), authoredValue);

    const QString matrixValue = matrix->property(QStringLiteral("gain"));
    const QString scriptValue = static_cast<RGBScript *>(matrix->algorithm())
                                    ->property(QStringLiteral("gain"));

    doc->deleteFunction(functionId);
    return qMakePair(matrixValue, scriptValue);
}

template <typename EditorT, typename MatrixT>
QVariantMap collectFloatCallFromQmlReceiver(Doc *doc, QQuickView *view, const QString &descriptor, double initialValue)
{
    MatrixT *matrix = new MatrixT(doc);
    matrix->setName(QStringLiteral("QML receiver matrix"));
    std::unique_ptr<RGBScript> script(makeScript(doc, QStringLiteral("QML Receiver Script"), descriptor, initialValue));
    if (!script)
        return {};
    matrix->setAlgorithm(script.release());
    if (!doc->addFunction(matrix))
        return {};
    const quint32 functionId = matrix->id();

    EditorT editor(view, doc);
    editor.setFunctionID(functionId);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("screenPixelDensity", 4.0);
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        Item {
            property int calls: 0
            property var lastArgs: ({})
            function addLabel(v) {}
            function addComboBox(a, b, c) {}
            function addSpinBox(a, b, c, d) {}
            function addTextEdit(a, b) {}
            function addDoubleSpinBox(propName, currentValue, minValue, maxValue, stepValue, decimalPlaces, boundedControl) {
                calls += 1
                lastArgs = {
                    propName: propName,
                    currentValue: currentValue,
                    minDefined: (minValue !== undefined),
                    maxDefined: (maxValue !== undefined),
                    stepDefined: (stepValue !== undefined),
                    decimalsDefined: (decimalPlaces !== undefined),
                    boundedControl: boundedControl
                }
            }
        })", QUrl("qrc:/FloatQmlReceiver.qml"));
    std::unique_ptr<QObject> receiver(component.create());
    if (!receiver)
    {
        doc->deleteFunction(functionId);
        return {};
    }

    auto *receiverItem = qobject_cast<QQuickItem *>(receiver.get());
    if (!receiverItem)
    {
        doc->deleteFunction(functionId);
        return {};
    }

    editor.createScriptObjects(receiverItem);
    QVariantMap result = receiver->property("lastArgs").toMap();
    result.insert(QStringLiteral("calls"), receiver->property("calls"));
    doc->deleteFunction(functionId);
    return result;
}

void configureAndCheckComponent(const FloatControlCollector::FloatCall &call,
                                double minBound, double maxBound, int expectedDecimals)
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("screenPixelDensity", 4.0);
    const QString file = QFINDTESTDATA("../../qml/CustomDoubleSpinBox.qml");
    QVERIFY2(!file.isEmpty(), "CustomDoubleSpinBox.qml not found");
    QQmlComponent component(&engine, QUrl::fromLocalFile(file));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));

    object->setProperty("suffix", "");
    object->setProperty("decimals", call.decimalsValue.toInt());
    object->setProperty("realFrom", call.minValue.toDouble());
    object->setProperty("realTo", call.maxValue.toDouble());
    object->setProperty("realStep", call.stepValue.toDouble());
    object->setProperty("realValue", call.currentValue);
    object->setProperty("boundedControl", call.boundedControlValue.toBool());
    QCoreApplication::processEvents();

    const int scale = qRound(std::pow(10.0, expectedDecimals));
    const int expectedFrom = qRound(minBound * scale);
    const int expectedTo = qRound(maxBound * scale);

    QCOMPARE(object->property("from").toInt(), expectedFrom);
    QCOMPARE(object->property("to").toInt(), expectedTo);
    QCOMPARE(object->property("stepSize").toInt(), 1);

    QVERIFY(QMetaObject::invokeMethod(object.get(), "setValue", Q_ARG(QVariant, expectedFrom)));
    const double tolerance = std::pow(10.0, -expectedDecimals);
    QVERIFY(std::fabs(object->property("realValue").toDouble() - minBound) <= tolerance);

    QVERIFY(QMetaObject::invokeMethod(object.get(), "setValue", Q_ARG(QVariant, expectedTo)));
    QVERIFY(std::fabs(object->property("realValue").toDouble() - maxBound) <= tolerance);
}

QValidator::State validateText(QObject *object, QString text)
{
    QObject *contentItem = object->property("contentItem").value<QObject *>();
    if (contentItem == nullptr)
        return QValidator::Invalid;
    QValidator *validator = qobject_cast<QValidator *>(contentItem->property("validator").value<QObject *>());
    if (validator == nullptr)
        return QValidator::Invalid;
    int pos = 0;
    return validator->validate(text, pos);
}

QObject *lineEditObject(QObject *object)
{
    if (object == nullptr)
        return nullptr;
    return object->property("contentItem").value<QObject *>();
}

bool commitTypedValue(QObject *object, const QString &typedValue)
{
    QObject *contentItem = lineEditObject(object);
    if (contentItem == nullptr)
        return false;

    contentItem->setProperty("text", typedValue);
    QCoreApplication::processEvents();
    if (!contentItem->property("acceptableInput").toBool())
        return false;

    QQmlContext *context = qmlContext(object);
    if (context == nullptr)
        return false;
    QString escaped = typedValue;
    escaped.replace("\\", "\\\\");
    escaped.replace("'", "\\'");
    QQmlExpression parseExpr(context, object,
                             QStringLiteral("valueFromText('%1', locale)").arg(escaped));
    const QVariant parsedVariant = parseExpr.evaluate();
    if (parseExpr.hasError())
        return false;
    const int parsedValue = parsedVariant.toInt();

    const bool invoked = QMetaObject::invokeMethod(object, "setValue",
                                                   Q_ARG(QVariant, parsedValue));
    QCoreApplication::processEvents();
    return invoked;
}

std::unique_ptr<QObject> createDoubleSpinObject()
{
    auto *engine = new QQmlEngine();
    engine->rootContext()->setContextProperty("screenPixelDensity", 4.0);
    const QString file = QFINDTESTDATA("../../qml/CustomDoubleSpinBox.qml");
    if (file.isEmpty())
        return {};
    QQmlComponent component(engine, QUrl::fromLocalFile(file));
    if (!component.isReady())
        return {};
    QObject *object = component.create();
    if (object == nullptr)
        return {};
    object->setParent(engine);
    return std::unique_ptr<QObject>(object);
}

std::unique_ptr<QObject> createBoundedControlFromCall(const FloatControlCollector::FloatCall &call)
{
    std::unique_ptr<QObject> object = createDoubleSpinObject();
    if (!object)
        return {};
    object->setProperty("suffix", "");
    object->setProperty("decimals", call.decimalsValue.toInt());
    object->setProperty("realFrom", call.minValue.toDouble());
    object->setProperty("realTo", call.maxValue.toDouble());
    object->setProperty("realStep", call.stepValue.toDouble());
    object->setProperty("realValue", call.currentValue);
    object->setProperty("boundedControl", call.boundedControlValue.toBool());
    QCoreApplication::processEvents();
    return object;
}

bool writeTempScript(const QString &path, const QString &name,
                     const QString &descriptor, double initialValue)
{
    QFile scriptFile(path);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    scriptFile.write(buildFloatScript(name, descriptor, initialValue).toUtf8());
    scriptFile.close();
    return true;
}
}

void MatrixScriptFloatControls_Test::editorFloatBounds_data()
{
    QTest::addColumn<QString>("editor");
    QTest::addColumn<double>("minBound");
    QTest::addColumn<double>("maxBound");
    QTest::addColumn<double>("initialValue");
    QTest::addColumn<int>("expectedDecimals");

    auto addRows = [](const QString &editor) {
        QTest::newRow(QString("%1-negative--1..1").arg(editor).toUtf8().constData())
            << editor << -1.0 << 1.0 << -0.25 << 3;
        QTest::newRow(QString("%1-narrow-0..1").arg(editor).toUtf8().constData())
            << editor << 0.0 << 1.0 << 0.375 << 3;
        QTest::newRow(QString("%1-minimum-1e-5").arg(editor).toUtf8().constData())
            << editor << 0.00001 << 1.0 << 0.00001 << 5;
        QTest::newRow(QString("%1-above-100-0..250").arg(editor).toUtf8().constData())
            << editor << 0.0 << 250.0 << 123.456 << 3;
    };

    addRows(QStringLiteral("rgb"));
    addRows(QStringLiteral("hue"));
}

void MatrixScriptFloatControls_Test::editorFloatBounds()
{
    QFETCH(QString, editor);
    QFETCH(double, minBound);
    QFETCH(double, maxBound);
    QFETCH(double, initialValue);
    QFETCH(int, expectedDecimals);

    Doc doc(nullptr, 1);
    QQuickView view;
    const QDir rgbDir(QFINDTESTDATA("../../../resources/rgbscripts"));
    QVERIFY(doc.rgbScriptsCache()->load(rgbDir));

    FloatControlCollector::FloatCall call;
    if (editor == QStringLiteral("rgb"))
        call = collectFloatCall<RGBMatrixEditor, RGBMatrix>(&doc, &view, minBound, maxBound, initialValue);
    else
        call = collectFloatCall<HUEMatrixEditor, HUEMatrix>(&doc, &view, minBound, maxBound, initialValue);

    QVERIFY2(!call.propName.isEmpty(), "No float editor call captured");
    QCOMPARE(call.propName, QStringLiteral("gain"));
    QVERIFY2(call.minValue.isValid(), "Float bounds min not forwarded by editor");
    QVERIFY2(call.maxValue.isValid(), "Float bounds max not forwarded by editor");
    QCOMPARE(call.minValue.toDouble(), minBound);
    QCOMPARE(call.maxValue.toDouble(), maxBound);
    QVERIFY2(call.decimalsValue.isValid(), "Float precision not forwarded by editor");
    QCOMPARE(call.decimalsValue.toInt(), expectedDecimals);
    QVERIFY2(call.stepValue.isValid(), "Float step not forwarded by editor");
    QCOMPARE(call.stepValue.toDouble(), std::pow(10.0, -expectedDecimals));
    QCOMPARE(call.boundedControlValue.toBool(), true);

    configureAndCheckComponent(call, minBound, maxBound, expectedDecimals);
}

void MatrixScriptFloatControls_Test::validatorTextPath_data()
{
    QTest::addColumn<QString>("editor");
    QTest::addColumn<double>("minBound");
    QTest::addColumn<double>("maxBound");
    QTest::addColumn<double>("initialValue");
    QTest::addColumn<int>("expectedDecimals");
    QTest::addColumn<QString>("typedValue");

    auto addRows = [](const QString &editor) {
        QTest::newRow(QString("%1-typed-min-1e-5").arg(editor).toUtf8().constData())
            << editor << 0.00001 << 1.0 << 0.2 << 5 << QStringLiteral("0.00001");
        QTest::newRow(QString("%1-typed-min-0.1").arg(editor).toUtf8().constData())
            << editor << 0.1 << 10.0 << 1.3 << 3 << QStringLiteral("0.1");
    };
    addRows(QStringLiteral("rgb"));
    addRows(QStringLiteral("hue"));
}

void MatrixScriptFloatControls_Test::validatorTextPath()
{
    QFETCH(QString, editor);
    QFETCH(double, minBound);
    QFETCH(double, maxBound);
    QFETCH(double, initialValue);
    QFETCH(int, expectedDecimals);
    QFETCH(QString, typedValue);

    Doc doc(nullptr, 1);
    QQuickView view;
    const QDir rgbDir(QFINDTESTDATA("../../../resources/rgbscripts"));
    QVERIFY(doc.rgbScriptsCache()->load(rgbDir));

    FloatControlCollector::FloatCall call;
    if (editor == QStringLiteral("rgb"))
        call = collectFloatCall<RGBMatrixEditor, RGBMatrix>(&doc, &view, minBound, maxBound, initialValue);
    else
        call = collectFloatCall<HUEMatrixEditor, HUEMatrix>(&doc, &view, minBound, maxBound, initialValue);

    QCOMPARE(call.decimalsValue.toInt(), expectedDecimals);
    std::unique_ptr<QObject> object = createBoundedControlFromCall(call);
    QVERIFY2(object != nullptr, "Failed to create CustomDoubleSpinBox");
    QVERIFY(call.boundedControlValue.toBool());

    QCOMPARE(validateText(object.get(), typedValue), QValidator::Acceptable);
    QObject *contentItem = lineEditObject(object.get());
    QVERIFY(contentItem != nullptr);
    contentItem->setProperty("text", typedValue);
    QCoreApplication::processEvents();
    QVERIFY(contentItem->property("acceptableInput").toBool());
    QVERIFY(commitTypedValue(object.get(), typedValue));

    const double expected = typedValue.toDouble();
    const double tolerance = std::pow(10.0, -call.decimalsValue.toInt());
    QVERIFY(std::fabs(object->property("realValue").toDouble() - expected) <= tolerance);
}

void MatrixScriptFloatControls_Test::unboundedValidatorTextPath_data()
{
    QTest::addColumn<QString>("typedValue");
    QTest::addColumn<double>("expectedRealValue");

    QTest::newRow("extra-fractional-digits")
        << QStringLiteral("50.123") << 50.12;
    QTest::newRow("scientific-notation")
        << QStringLiteral("5e1") << 50.0;
}

void MatrixScriptFloatControls_Test::unboundedValidatorTextPath()
{
    QFETCH(QString, typedValue);
    QFETCH(double, expectedRealValue);

    std::unique_ptr<QObject> object = createDoubleSpinObject();
    QVERIFY2(object != nullptr, "Failed to create CustomDoubleSpinBox");

    object->setProperty("suffix", "");
    object->setProperty("decimals", 2);
    object->setProperty("realFrom", 0.0);
    object->setProperty("realTo", 100.0);
    object->setProperty("realStep", 0.5);
    object->setProperty("realValue", 0.0);
    object->setProperty("boundedControl", false);
    QCoreApplication::processEvents();

    QCOMPARE(validateText(object.get(), typedValue), QValidator::Acceptable);
    QObject *contentItem = lineEditObject(object.get());
    QVERIFY(contentItem != nullptr);
    contentItem->setProperty("text", typedValue);
    QCoreApplication::processEvents();
    QVERIFY(contentItem->property("acceptableInput").toBool());
    QVERIFY(commitTypedValue(object.get(), typedValue));
    QVERIFY(std::fabs(object->property("realValue").toDouble() - expectedRealValue) <= 0.01);
}

void MatrixScriptFloatControls_Test::unboundedQmlReceiver_data()
{
    QTest::addColumn<QString>("editor");
    QTest::newRow("rgb") << QStringLiteral("rgb");
    QTest::newRow("hue") << QStringLiteral("hue");
}

void MatrixScriptFloatControls_Test::unboundedQmlReceiver()
{
    QFETCH(QString, editor);

    Doc doc(nullptr, 1);
    QQuickView view;
    const QDir rgbDir(QFINDTESTDATA("../../../resources/rgbscripts"));
    QVERIFY(doc.rgbScriptsCache()->load(rgbDir));

    const QString descriptor = QStringLiteral("name:gain|display:Gain|type:float|write:setGain|read:getGain");
    QVariantMap call;
    if (editor == QStringLiteral("rgb"))
    {
        call = collectFloatCallFromQmlReceiver<RGBMatrixEditor, RGBMatrix>(&doc, &view, descriptor, 0.42);
    }
    else
    {
        call = collectFloatCallFromQmlReceiver<HUEMatrixEditor, HUEMatrix>(&doc, &view, descriptor, 0.42);
    }

    QCOMPARE(call.value(QStringLiteral("calls")).toInt(), 1);
    QCOMPARE(call.value(QStringLiteral("propName")).toString(), QStringLiteral("gain"));
    QCOMPARE(call.value(QStringLiteral("boundedControl")).toBool(), false);
    QCOMPARE(call.value(QStringLiteral("minDefined")).toBool(), false);
    QCOMPARE(call.value(QStringLiteral("maxDefined")).toBool(), false);
    QCOMPARE(call.value(QStringLiteral("stepDefined")).toBool(), false);
    QCOMPARE(call.value(QStringLiteral("decimalsDefined")).toBool(), false);
}

void MatrixScriptFloatControls_Test::constructionBelowMinimumKeepsSync()
{
    FloatControlCollector::FloatCall call;
    call.propName = QStringLiteral("gain");
    call.currentValue = 0.0;
    call.minValue = 0.00001;
    call.maxValue = 1.0;
    call.stepValue = 0.00001;
    call.decimalsValue = 5;
    call.boundedControlValue = true;

    std::unique_ptr<QObject> object = createBoundedControlFromCall(call);
    QVERIFY2(object != nullptr, "Failed to create bounded control");

    QCOMPARE(object->property("realValue").toDouble(), 0.0);
    object->setProperty("realValue", 0.5);
    QCoreApplication::processEvents();
    QCOMPARE(object->property("value").toInt(), 50000);
    QCOMPARE(object->property("realValue").toDouble(), 0.5);

    object->setProperty("realValue", 0.00001);
    QCoreApplication::processEvents();
    QCOMPARE(object->property("value").toInt(), 1);
    QCOMPARE(object->property("realValue").toDouble(), 0.00001);
}

void MatrixScriptFloatControls_Test::unboundedPreservesLegacyBehavior()
{
    std::unique_ptr<QObject> object = createDoubleSpinObject();
    QVERIFY2(object != nullptr, "Failed to create CustomDoubleSpinBox");

    object->setProperty("suffix", "");
    object->setProperty("decimals", 2);
    object->setProperty("realFrom", 0.0);
    object->setProperty("realTo", 100.0);
    object->setProperty("realStep", 0.5);
    object->setProperty("realValue", 0.0);
    object->setProperty("boundedControl", false);
    QCoreApplication::processEvents();

    QCOMPARE(object->property("from").toInt(), 0);
    QCOMPARE(object->property("to").toInt(), 10000);
    QCOMPARE(object->property("stepSize").toInt(), 50);
    QCOMPARE(validateText(object.get(), QStringLiteral("50")), QValidator::Acceptable);

    object->setProperty("decimals", 5);
    object->setProperty("realFrom", 0.00001);
    object->setProperty("realTo", 1.0);
    object->setProperty("realStep", 0.00001);
    QCoreApplication::processEvents();

    QCOMPARE(validateText(object.get(), QStringLiteral("0.00001")), QValidator::Intermediate);
}

void MatrixScriptFloatControls_Test::invalidBoundsPreservePropertyAndAuthoredValue()
{
    Doc doc(nullptr, 1);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral(".*Float bounds must be ordered as min < max.*")));
    std::unique_ptr<RGBScript> script(makeScript(
        &doc,
        QStringLiteral("Invalid Bounds Preserve"),
        QStringLiteral("name:gain|display:Gain|type:float|values:1,0|write:setGain|read:getGain"),
        0.25));
    QVERIFY(script != nullptr);
    QCOMPARE(script->properties().count(), 1);
    const RGBScriptProperty prop = script->properties().first();
    QVERIFY(!prop.m_floatHasBounds);
    QVERIFY(script->setProperty(QStringLiteral("gain"), QStringLiteral("0.12345678912345")));
    QCOMPARE(script->property(QStringLiteral("gain")), QStringLiteral("0.12345678912345"));

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString scriptName = QStringLiteral("InvalidBoundsSaveLoad");
    const QString scriptPath = temp.filePath(QStringLiteral("invalid_bounds_saveload.js"));
    QVERIFY(writeTempScript(scriptPath, scriptName,
                            QStringLiteral("name:gain|display:Gain|type:float|values:1,0|write:setGain|read:getGain"),
                            0.25));

    Doc roundTripDoc(nullptr, 1);
    const QDir stockScripts(QFINDTESTDATA("../../../resources/rgbscripts"));
    QVERIFY(roundTripDoc.rgbScriptsCache()->load(stockScripts));
    QVERIFY(roundTripDoc.rgbScriptsCache()->load(QDir(temp.path())));
    QString resolvedName = scriptName;
    const QStringList invalidNames = roundTripDoc.rgbScriptsCache()->names();
    if (!invalidNames.contains(resolvedName))
    {
        for (const QString &name : invalidNames)
        {
            if (name.contains(scriptName))
            {
                resolvedName = name;
                break;
            }
        }
    }
    QVERIFY(invalidNames.contains(resolvedName));

    RGBMatrix source(&roundTripDoc);
    RGBScript *invalidScript = roundTripDoc.rgbScriptsCache()->script(resolvedName);
    QVERIFY(invalidScript != nullptr);
    source.setAlgorithm(invalidScript);
    QVERIFY(source.algorithm() != nullptr);
    source.setProperty(QStringLiteral("gain"), QStringLiteral("0.12345678912345"));
    QCOMPARE(source.property(QStringLiteral("gain")), QStringLiteral("0.12345678912345"));

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter writer(&buffer);
    QVERIFY(source.saveXML(&writer));
    writer.setDevice(nullptr);

    buffer.close();
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader reader(&buffer);
    QVERIFY(reader.readNextStartElement());

    RGBMatrix reloaded(&roundTripDoc);
    QVERIFY(reloaded.loadXML(reader));
    QCOMPARE(reloaded.property(QStringLiteral("gain")), QStringLiteral("0.12345678912345"));
    buffer.close();
}

void MatrixScriptFloatControls_Test::boundedFloatSaveLoadRoundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString scriptName = QStringLiteral("BoundedSaveLoad");
    const QString scriptPath = temp.filePath(QStringLiteral("bounded_saveload.js"));
    QVERIFY(writeTempScript(scriptPath, scriptName, boundedFloatDescriptor(0.00001, 1.0), 0.00001));

    Doc doc(nullptr, 1);
    const QDir stockScripts(QFINDTESTDATA("../../../resources/rgbscripts"));
    QVERIFY(doc.rgbScriptsCache()->load(stockScripts));
    QVERIFY(doc.rgbScriptsCache()->load(QDir(temp.path())));
    QString resolvedName = scriptName;
    const QStringList boundedNames = doc.rgbScriptsCache()->names();
    if (!boundedNames.contains(resolvedName))
    {
        for (const QString &name : boundedNames)
        {
            if (name.contains(scriptName))
            {
                resolvedName = name;
                break;
            }
        }
    }
    QVERIFY(boundedNames.contains(resolvedName));

    RGBMatrix source(&doc);
    RGBScript *boundedScript = doc.rgbScriptsCache()->script(resolvedName);
    QVERIFY(boundedScript != nullptr);
    source.setAlgorithm(boundedScript);
    QVERIFY(source.algorithm() != nullptr);
    source.setProperty(QStringLiteral("gain"), QStringLiteral("0.00001"));
    QCOMPARE(source.property(QStringLiteral("gain")), QStringLiteral("0.00001"));

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter writer(&buffer);
    QVERIFY(source.saveXML(&writer));
    writer.setDevice(nullptr);

    buffer.close();
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader reader(&buffer);
    QVERIFY(reader.readNextStartElement());
    QCOMPARE(reader.name().toString(), QStringLiteral("Function"));

    RGBMatrix reloaded(&doc);
    QVERIFY(reloaded.loadXML(reader));
    QCOMPARE(reloaded.property(QStringLiteral("gain")), QStringLiteral("0.00001"));
    buffer.close();
}

void MatrixScriptFloatControls_Test::editorAuthoredFloatValue_data()
{
    QTest::addColumn<QString>("editor");
    QTest::addColumn<double>("authoredValue");

    auto addRows = [](const QString &editor) {
        QTest::newRow(QString("%1-authored-minimum-1e-5").arg(editor).toUtf8().constData())
            << editor << 0.00001;
        QTest::newRow(QString("%1-authored-negative").arg(editor).toUtf8().constData())
            << editor << -0.5;
        QTest::newRow(QString("%1-authored-high-precision").arg(editor).toUtf8().constData())
            << editor << 123.456789;
    };
    addRows(QStringLiteral("rgb"));
    addRows(QStringLiteral("hue"));
}

void MatrixScriptFloatControls_Test::editorAuthoredFloatValue()
{
    QFETCH(QString, editor);
    QFETCH(double, authoredValue);

    Doc doc(nullptr, 1);
    QQuickView view;
    const QDir rgbDir(QFINDTESTDATA("../../../resources/rgbscripts"));
    QVERIFY(doc.rgbScriptsCache()->load(rgbDir));

    QPair<QString, QString> result;
    if (editor == QStringLiteral("rgb"))
    {
        result = writeAuthoredFloat<RGBMatrixEditor, RGBMatrix>(&doc, &view,
                                                                -1.0, 250.0, 0.25, authoredValue);
    }
    else
    {
        result = writeAuthoredFloat<HUEMatrixEditor, HUEMatrix>(&doc, &view,
                                                                -1.0, 250.0, 0.25, authoredValue);
    }

    const QString expected = QString::number(authoredValue, 'g', 15);
    QVERIFY2(!result.first.isEmpty(), "Matrix property write did not complete");
    QCOMPARE(result.first, expected);
    bool ok = false;
    const double scriptValue = result.second.toDouble(&ok);
    QVERIFY2(ok, "Script property readback is not numeric");
    const double tolerance = qMax(1e-12, std::fabs(authoredValue) * 1e-12);
    QVERIFY(std::fabs(scriptValue - authoredValue) <= tolerance);
    if (std::fabs(authoredValue) <= 0.00001)
        QVERIFY(scriptValue > 0.0);
}

QTEST_MAIN(MatrixScriptFloatControls_Test)

#include "matrixscriptfloatcontrols_test.moc"
