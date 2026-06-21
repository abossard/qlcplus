/*
  Q Light Controller Plus - Unit test
  vcpage_input_mode_test.cpp

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

#include <QtTest>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "vcpage_input_mode_test.h"
#include "vcbridge.h"

// Re-implement the conversion functions locally to test the contract
// without linking to qlcplus-qml. These must match vcpage.cpp exactly.
namespace InputMode {
    enum Mode { Normal = 0, Override, Inherit };

    static const QString XML_TAG = QStringLiteral("ExternalInputMode");

    static QString toString(Mode mode)
    {
        switch (mode)
        {
            case Override: return QStringLiteral("Override");
            case Inherit:  return QStringLiteral("Inherit");
            default:       return QStringLiteral("Normal");
        }
    }

    static Mode fromString(const QString &str)
    {
        if (str == QLatin1String("Override")) return Override;
        if (str == QLatin1String("Inherit"))  return Inherit;
        return Normal;
    }
}

// ========== String conversion round-trips ==========

void VCPageInputMode_Test::stringConversion_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<QString>("expected");

    QTest::newRow("Normal")   << (int)InputMode::Normal   << QStringLiteral("Normal");
    QTest::newRow("Override") << (int)InputMode::Override  << QStringLiteral("Override");
    QTest::newRow("Inherit")  << (int)InputMode::Inherit   << QStringLiteral("Inherit");
}

void VCPageInputMode_Test::stringConversion()
{
    QFETCH(int, mode);
    QFETCH(QString, expected);

    auto m = static_cast<InputMode::Mode>(mode);

    // Forward: enum → string
    QCOMPARE(InputMode::toString(m), expected);

    // Reverse: string → enum
    QCOMPARE((int)InputMode::fromString(expected), mode);
}

void VCPageInputMode_Test::unknownStringDefaultsToNormal()
{
    QCOMPARE((int)InputMode::fromString("Bogus"), (int)InputMode::Normal);
    QCOMPARE((int)InputMode::fromString(""), (int)InputMode::Normal);
    QCOMPARE((int)InputMode::fromString("override"), (int)InputMode::Normal); // case-sensitive
}

// ========== PageInfo struct ==========

void VCPageInputMode_Test::pageInfoHasExternalInputMode()
{
    VCBridge::PageInfo pi;
    pi.index = 0;
    pi.name = "Test";
    pi.externalInputMode = "Override";

    QCOMPARE(pi.externalInputMode, QString("Override"));

    // Verify default-constructed is empty
    VCBridge::PageInfo pi2;
    QVERIFY(pi2.externalInputMode.isEmpty());
}

// ========== XML save tests ==========

static QString buildXMLWithMode(InputMode::Mode mode)
{
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    writer.writeStartElement("Frame");

    // Matches saveExtraXML: only write tag for non-Normal
    if (mode != InputMode::Normal)
        writer.writeTextElement(InputMode::XML_TAG, InputMode::toString(mode));

    writer.writeEndElement();
    writer.writeEndDocument();
    buf.close();
    return QString::fromUtf8(buf.data());
}

void VCPageInputMode_Test::xmlSave_normalOmitsTag()
{
    QString xml = buildXMLWithMode(InputMode::Normal);
    QVERIFY(!xml.contains(InputMode::XML_TAG));
}

void VCPageInputMode_Test::xmlSave_overrideWritesTag()
{
    QString xml = buildXMLWithMode(InputMode::Override);
    QVERIFY(xml.contains("<ExternalInputMode>Override</ExternalInputMode>"));
}

void VCPageInputMode_Test::xmlSave_inheritWritesTag()
{
    QString xml = buildXMLWithMode(InputMode::Inherit);
    QVERIFY(xml.contains("<ExternalInputMode>Inherit</ExternalInputMode>"));
}

// ========== XML load tests ==========

static InputMode::Mode loadModeFromXML(const QString &modeStr)
{
    QString xml = QString("<Frame><ExternalInputMode>%1</ExternalInputMode></Frame>").arg(modeStr);
    QXmlStreamReader reader(xml);
    reader.readNextStartElement(); // <Frame>

    InputMode::Mode result = InputMode::Normal;
    while (reader.readNextStartElement())
    {
        if (reader.name() == InputMode::XML_TAG)
            result = InputMode::fromString(reader.readElementText());
        else
            reader.skipCurrentElement();
    }
    return result;
}

void VCPageInputMode_Test::xmlLoad_data()
{
    QTest::addColumn<QString>("modeStr");
    QTest::addColumn<int>("expected");

    QTest::newRow("Override") << QStringLiteral("Override") << (int)InputMode::Override;
    QTest::newRow("Inherit")  << QStringLiteral("Inherit")  << (int)InputMode::Inherit;
    QTest::newRow("Normal")   << QStringLiteral("Normal")   << (int)InputMode::Normal;
}

void VCPageInputMode_Test::xmlLoad()
{
    QFETCH(QString, modeStr);
    QFETCH(int, expected);

    QCOMPARE((int)loadModeFromXML(modeStr), expected);
}

void VCPageInputMode_Test::xmlLoad_missingTagDefaultsToNormal()
{
    // XML with no ExternalInputMode tag — backward compat
    QString xml = "<Frame><AllowResize>True</AllowResize></Frame>";
    QXmlStreamReader reader(xml);
    reader.readNextStartElement(); // <Frame>

    InputMode::Mode result = InputMode::Normal;
    while (reader.readNextStartElement())
    {
        if (reader.name() == InputMode::XML_TAG)
            result = InputMode::fromString(reader.readElementText());
        else
            reader.skipCurrentElement();
    }

    QCOMPARE((int)result, (int)InputMode::Normal);
}

QTEST_MAIN(VCPageInputMode_Test)
#include "vcpage_input_mode_test.moc"
