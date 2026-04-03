/*
  Q Light Controller Plus - Unit test
  vc_validation_test.cpp

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

#include "vc_validation_test.h"
#include "vc_tools_common.h"

using Json = nlohmann::json;

// ========== Type resolution ==========

void VCValidation_Test::typeFromString_button()
{
    QCOMPARE(VCType::fromString("button"), (int)VCType::Button);
}

void VCValidation_Test::typeFromString_allTypes()
{
    QCOMPARE(VCType::fromString("button"),       (int)VCType::Button);
    QCOMPARE(VCType::fromString("slider"),       (int)VCType::Slider);
    QCOMPARE(VCType::fromString("xypad"),        (int)VCType::XYPad);
    QCOMPARE(VCType::fromString("frame"),        (int)VCType::Frame);
    QCOMPARE(VCType::fromString("soloframe"),    (int)VCType::SoloFrame);
    QCOMPARE(VCType::fromString("speedDial"),    (int)VCType::SpeedDial);
    QCOMPARE(VCType::fromString("cuelist"),      (int)VCType::CueList);
    QCOMPARE(VCType::fromString("label"),        (int)VCType::Label);
    QCOMPARE(VCType::fromString("audioTrigger"), (int)VCType::AudioTriggers);
    QCOMPARE(VCType::fromString("matrix"),       (int)VCType::Animation);
    QCOMPARE(VCType::fromString("clock"),        (int)VCType::Clock);
}

void VCValidation_Test::typeFromString_invalid()
{
    QCOMPARE(VCType::fromString("nonexistent"), (int)VCType::Unknown);
    QCOMPARE(VCType::fromString(""),            (int)VCType::Unknown);
    QCOMPARE(VCType::fromString("Button"),      (int)VCType::Unknown);  // case-sensitive
}

void VCValidation_Test::typeToString_roundTrip()
{
    // Every type should survive a toString → fromString round-trip
    const int allTypes[] = {
        VCType::Button, VCType::Slider, VCType::XYPad, VCType::Frame,
        VCType::SoloFrame, VCType::SpeedDial, VCType::CueList,
        VCType::Label, VCType::AudioTriggers, VCType::Animation, VCType::Clock
    };
    for (int t : allTypes)
    {
        std::string name = VCType::toString(t);
        QVERIFY2(!name.empty(), qPrintable(QString("toString(%1) returned empty").arg(t)));
        QVERIFY2(name != "unknown", qPrintable(QString("toString(%1) returned 'unknown'").arg(t)));
        int back = VCType::fromString(name);
        QCOMPARE(back, t);
    }
}

// ========== Field validation — create mode ==========

void VCValidation_Test::createValidation_buttonFieldsValid()
{
    Json item = {
        {"type", "button"}, {"parentID", 1}, {"caption", "Go"},
        {"action", "toggle"}, {"iconPath", "/icon.png"},
        {"startupIntensityEnabled", true}, {"startupIntensity", 0.8},
        {"flashOverride", false}, {"flashForceLTP", true},
        {"stopAllFadeTime", 500}
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Button, true);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::createValidation_sliderFieldsValid()
{
    Json item = {
        {"type", "slider"}, {"parentID", 1}, {"caption", "Dim"},
        {"mode", "level"}, {"widgetStyle", "slider"},
        {"clickAndGoType", "colors"}, {"valueDisplayStyle", "percentage"},
        {"invertedAppearance", false}, {"rangeLowLimit", 0},
        {"rangeHighLimit", 255}, {"monitorEnabled", true}
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Slider, true);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::createValidation_frameFieldsValid()
{
    Json item = {
        {"type", "frame"}, {"parentID", 0},
        {"caption", "Effects"}, {"pageIndex", 0}, {"solo", false},
        {"multipageMode", true}, {"totalPages", 3}, {"pagesLoop", true},
        {"headerVisible", true}, {"enableButtonVisible", false}
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Frame, true);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::createValidation_framePageLabelsValid()
{
    Json item = {
        {"type", "frame"}, {"pageIndex", 0},
        {"multipageMode", true}, {"totalPages", 3}, {"pagesLoop", true},
        {"pageLabels", {"Intro", "Drop", "Outro"}}
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Frame, true);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::createValidation_matrixFieldsValid()
{
    Json item = {
        {"type", "matrix"}, {"parentID", 1}, {"caption", "Matrix"},
        {"functionID", 42}, {"instantApply", true}, {"visibilityMask", 255}
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Animation, true);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::createValidation_allTypesAcceptCommonFields()
{
    const int allTypes[] = {
        VCType::Button, VCType::Slider, VCType::XYPad, VCType::Frame,
        VCType::SoloFrame, VCType::SpeedDial, VCType::CueList,
        VCType::Label, VCType::AudioTriggers, VCType::Animation, VCType::Clock
    };
    // Common create fields should be accepted by every type
    Json item = {
        {"type", "button"}, {"parentID", 1}, {"caption", "Test"},
        {"x", 10}, {"y", 20}, {"width", 100}, {"height", 60},
        {"bgColor", "#112233"}, {"fgColor", "#ffffff"}
    };

    for (int t : allTypes)
    {
        item["type"] = VCType::toString(t);
        auto err = VCValidate::validateFieldsForType(item, t, true);
        QVERIFY2(err.empty(), qPrintable(
            QString("Type %1 rejected common fields: %2")
                .arg(QString::fromStdString(VCType::toString(t)))
                .arg(QString::fromStdString(err))));
    }
}

void VCValidation_Test::createValidation_buttonFieldOnSlider_rejected()
{
    Json item = {
        {"type", "slider"}, {"parentID", 1}, {"caption", "Test"},
        {"mode", "level"},
        {"action", "flash"}  // button-only field
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Slider, true);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["widgetType"].get<std::string>(), std::string("slider"));
    auto invalidFields = errJson["invalidFields"];
    QVERIFY(invalidFields.size() == 1);
    QCOMPARE(invalidFields[0].get<std::string>(), std::string("action"));
}

void VCValidation_Test::createValidation_sliderFieldOnButton_rejected()
{
    Json item = {
        {"type", "button"}, {"parentID", 1}, {"caption", "Test"},
        {"channels", Json::array({{{"fixtureID", 0}, {"channel", 0}}})}  // slider-only field
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Button, true);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["widgetType"].get<std::string>(), std::string("button"));
    QVERIFY(errJson["invalidFields"].size() == 1);
    QCOMPARE(errJson["invalidFields"][0].get<std::string>(), std::string("channels"));
}

void VCValidation_Test::createValidation_matrixFieldOnClock_rejected()
{
    Json item = {
        {"type", "clock"}, {"parentID", 1}, {"caption", "Clock"},
        {"instantApply", true}  // matrix-only field
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Clock, true);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["widgetType"].get<std::string>(), std::string("clock"));
    QCOMPARE(errJson["invalidFields"][0].get<std::string>(), std::string("instantApply"));
}

void VCValidation_Test::createValidation_unknownField_rejected()
{
    Json item = {
        {"type", "button"}, {"parentID", 1}, {"caption", "Test"},
        {"nonExistentField", 42}
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Button, true);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["invalidFields"][0].get<std::string>(), std::string("nonExistentField"));
}

void VCValidation_Test::createValidation_errorContainsWidgetType()
{
    Json item = {
        {"type", "label"}, {"parentID", 1}, {"caption", "Test"},
        {"action", "toggle"}  // not valid for label
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Label, true);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["widgetType"].get<std::string>(), std::string("label"));
}

void VCValidation_Test::createValidation_errorContainsInvalidField()
{
    Json item = {
        {"type", "cuelist"}, {"parentID", 1}, {"caption", "Test"},
        {"bogusA", 1}, {"bogusB", 2}
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::CueList, true);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    auto fields = errJson["invalidFields"];
    // Both bogus fields should appear
    std::set<std::string> reported;
    for (const auto &f : fields)
        reported.insert(f.get<std::string>());
    QVERIFY(reported.count("bogusA") == 1);
    QVERIFY(reported.count("bogusB") == 1);
}

void VCValidation_Test::createValidation_errorListsAllowedFields()
{
    Json item = {
        {"type", "button"}, {"parentID", 1}, {"caption", "Test"},
        {"nope", 1}
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Button, true);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    // The error string should mention allowed fields
    std::string errorMsg = errJson["error"].get<std::string>();
    QVERIFY2(errorMsg.find("Allowed:") != std::string::npos,
             "Error message should list allowed fields");
    // Should mention at least 'caption' and 'parentID' in allowed
    QVERIFY(errorMsg.find("caption") != std::string::npos);
    QVERIFY(errorMsg.find("parentID") != std::string::npos);
}

// ========== Field validation — update mode ==========

void VCValidation_Test::updateValidation_buttonFieldsValid()
{
    Json item = {
        {"widgetID", 42}, {"caption", "Updated"},
        {"action", "flash"}, {"functionID", 10},
        {"startupIntensityEnabled", true}, {"startupIntensity", 0.5}
    };
    auto err = VCValidate::validateFieldsForType(item, VCType::Button, false);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::updateValidation_commonFieldsOnAnyType()
{
    const int allTypes[] = {
        VCType::Button, VCType::Slider, VCType::XYPad, VCType::Frame,
        VCType::SoloFrame, VCType::SpeedDial, VCType::CueList,
        VCType::Label, VCType::AudioTriggers, VCType::Animation, VCType::Clock
    };
    Json item = {
        {"widgetID", 1}, {"caption", "Test"},
        {"x", 10}, {"y", 20}, {"width", 100}, {"height", 60},
        {"bgColor", "#aabbcc"}, {"fgColor", "#112233"},
        {"font", {{"family", "Arial"}, {"size", 12}}},
        {"disabled", false}
    };
    for (int t : allTypes)
    {
        auto err = VCValidate::validateFieldsForType(item, t, false);
        QVERIFY2(err.empty(), qPrintable(
            QString("Type %1 rejected common update fields: %2")
                .arg(QString::fromStdString(VCType::toString(t)))
                .arg(QString::fromStdString(err))));
    }
}

void VCValidation_Test::updateValidation_catchValuesOnSlider_valid()
{
    Json item = {{"widgetID", 1}, {"catchValues", true}};
    auto err = VCValidate::validateFieldsForType(item, VCType::Slider, false);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::updateValidation_catchValuesOnButton_rejected()
{
    Json item = {{"widgetID", 1}, {"catchValues", true}};
    auto err = VCValidate::validateFieldsForType(item, VCType::Button, false);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["invalidFields"][0].get<std::string>(), std::string("catchValues"));
}

void VCValidation_Test::updateValidation_multipageModeOnFrame_valid()
{
    Json item = {{"widgetID", 1}, {"multipageMode", true}, {"totalPages", 4}};
    auto err = VCValidate::validateFieldsForType(item, VCType::Frame, false);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::updateValidation_pageLabelsOnFrame_valid()
{
    Json item = {{"widgetID", 1}, {"pageLabels", {"Phase 1", "Phase 2"}}};
    auto err = VCValidate::validateFieldsForType(item, VCType::Frame, false);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::updateValidation_pageLabelsOnButton_rejected()
{
    Json item = {{"widgetID", 1}, {"pageLabels", {"Phase 1"}}};
    auto err = VCValidate::validateFieldsForType(item, VCType::Button, false);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["invalidFields"][0].get<std::string>(), std::string("pageLabels"));
}

void VCValidation_Test::updateValidation_multipageModeOnLabel_rejected()
{
    Json item = {{"widgetID", 1}, {"multipageMode", true}};
    auto err = VCValidate::validateFieldsForType(item, VCType::Label, false);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["invalidFields"][0].get<std::string>(), std::string("multipageMode"));
}

void VCValidation_Test::updateValidation_barsNumberOnAudioTrigger_valid()
{
    Json item = {{"widgetID", 1}, {"barsNumber", 16}};
    auto err = VCValidate::validateFieldsForType(item, VCType::AudioTriggers, false);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::updateValidation_barsNumberOnButton_rejected()
{
    Json item = {{"widgetID", 1}, {"barsNumber", 16}};
    auto err = VCValidate::validateFieldsForType(item, VCType::Button, false);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["invalidFields"][0].get<std::string>(), std::string("barsNumber"));
}

// ========== Value validation ==========

void VCValidation_Test::valueValidation_validAction()
{
    for (const char *a : {"toggle", "flash", "blackout", "stopall"})
    {
        Json item = {{"action", a}};
        auto err = VCValidate::validateFieldValues(item, VCType::Button);
        QVERIFY2(err.empty(), qPrintable(
            QString("action '%1' rejected: %2").arg(a).arg(QString::fromStdString(err))));
    }
}

void VCValidation_Test::valueValidation_invalidAction()
{
    Json item = {{"action", "blink"}};
    auto err = VCValidate::validateFieldValues(item, VCType::Button);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["field"].get<std::string>(), std::string("action"));
}

void VCValidation_Test::valueValidation_validMode()
{
    for (const char *m : {"level", "playback", "submaster", "grandmaster"})
    {
        Json item = {{"mode", m}};
        auto err = VCValidate::validateFieldValues(item, VCType::Slider);
        QVERIFY2(err.empty(), qPrintable(
            QString("mode '%1' rejected: %2").arg(m).arg(QString::fromStdString(err))));
    }
}

void VCValidation_Test::valueValidation_invalidMode()
{
    Json item = {{"mode", "turbo"}};
    auto err = VCValidate::validateFieldValues(item, VCType::Slider);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["field"].get<std::string>(), std::string("mode"));
}

void VCValidation_Test::valueValidation_validColor()
{
    Json item = {{"bgColor", "#ff00aa"}, {"fgColor", "#AABB00"}};
    auto err = VCValidate::validateFieldValues(item, VCType::Button);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::valueValidation_invalidColor()
{
    // Missing # prefix
    Json item1 = {{"bgColor", "ff00aa"}};
    auto err1 = VCValidate::validateFieldValues(item1, VCType::Button);
    QVERIFY(!err1.empty());

    // Too short
    Json item2 = {{"bgColor", "#fff"}};
    auto err2 = VCValidate::validateFieldValues(item2, VCType::Button);
    QVERIFY(!err2.empty());

    // Invalid hex chars
    Json item3 = {{"bgColor", "#gghhii"}};
    auto err3 = VCValidate::validateFieldValues(item3, VCType::Button);
    QVERIFY(!err3.empty());
}

void VCValidation_Test::valueValidation_startupIntensityInRange()
{
    for (double v : {0.0, 0.5, 1.0})
    {
        Json item = {{"startupIntensity", v}};
        auto err = VCValidate::validateFieldValues(item, VCType::Button);
        QVERIFY2(err.empty(), qPrintable(
            QString("startupIntensity %1 rejected: %2").arg(v).arg(QString::fromStdString(err))));
    }
}

void VCValidation_Test::valueValidation_startupIntensityOutOfRange()
{
    Json item1 = {{"startupIntensity", -0.1}};
    QVERIFY(!VCValidate::validateFieldValues(item1, VCType::Button).empty());

    Json item2 = {{"startupIntensity", 1.1}};
    QVERIFY(!VCValidate::validateFieldValues(item2, VCType::Button).empty());
}

void VCValidation_Test::valueValidation_rangeLimitInRange()
{
    Json item = {{"rangeLowLimit", 0.0}, {"rangeHighLimit", 255.0}};
    auto err = VCValidate::validateFieldValues(item, VCType::Slider);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::valueValidation_rangeLimitOutOfRange()
{
    Json item1 = {{"rangeLowLimit", -1.0}};
    QVERIFY(!VCValidate::validateFieldValues(item1, VCType::Slider).empty());

    Json item2 = {{"rangeHighLimit", 256.0}};
    QVERIFY(!VCValidate::validateFieldValues(item2, VCType::Slider).empty());
}

void VCValidation_Test::valueValidation_validClockType()
{
    for (const char *ct : {"clock", "stopwatch", "countdown"})
    {
        Json item = {{"clockType", ct}};
        auto err = VCValidate::validateFieldValues(item, VCType::Clock);
        QVERIFY2(err.empty(), qPrintable(
            QString("clockType '%1' rejected: %2").arg(ct).arg(QString::fromStdString(err))));
    }
}

void VCValidation_Test::valueValidation_invalidClockType()
{
    Json item = {{"clockType", "timer"}};
    auto err = VCValidate::validateFieldValues(item, VCType::Clock);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["field"].get<std::string>(), std::string("clockType"));
}

void VCValidation_Test::valueValidation_validWidgetStyle()
{
    for (const char *s : {"slider", "knob"})
    {
        Json item = {{"widgetStyle", s}};
        auto err = VCValidate::validateFieldValues(item, VCType::Slider);
        QVERIFY2(err.empty(), qPrintable(
            QString("widgetStyle '%1' rejected: %2").arg(s).arg(QString::fromStdString(err))));
    }
}

void VCValidation_Test::valueValidation_invalidWidgetStyle()
{
    Json item = {{"widgetStyle", "dial"}};
    auto err = VCValidate::validateFieldValues(item, VCType::Slider);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["field"].get<std::string>(), std::string("widgetStyle"));
}

void VCValidation_Test::valueValidation_validFont()
{
    Json item = {{"font", {{"family", "Helvetica"}, {"size", 14}, {"bold", true}, {"italic", false}}}};
    auto err = VCValidate::validateFieldValues(item, VCType::Button);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::valueValidation_invalidFontField()
{
    Json item = {{"font", {{"family", "Arial"}, {"weight", 700}}}};  // "weight" not allowed
    auto err = VCValidate::validateFieldValues(item, VCType::Button);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["field"].get<std::string>(), std::string("font"));
}

void VCValidation_Test::valueValidation_validPosition()
{
    Json item = {{"position", {{"x", 0.5}, {"y", 0.5}}}};
    auto err = VCValidate::validateFieldValues(item, VCType::XYPad);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::valueValidation_positionOutOfRange()
{
    Json item = {{"position", {{"x", 1.5}, {"y", 0.5}}}};
    auto err = VCValidate::validateFieldValues(item, VCType::XYPad);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["field"].get<std::string>(), std::string("position.x"));
}

void VCValidation_Test::valueValidation_invalidType()
{
    Json item = {{"type", "bogusType"}};
    auto err = VCValidate::validateFieldValues(item, VCType::Unknown);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QCOMPARE(errJson["field"].get<std::string>(), std::string("type"));
}

// ========== Full validation pipeline ==========

void VCValidation_Test::fullValidation_buttonCreateValid()
{
    Json item = {
        {"type", "button"}, {"parentID", 1}, {"caption", "Go"},
        {"action", "toggle"}, {"bgColor", "#003300"}, {"fgColor", "#ffffff"}
    };
    auto err = VCValidate::validate(item, VCType::Button, true);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::fullValidation_buttonCreateInvalidField()
{
    Json item = {
        {"type", "button"}, {"parentID", 1}, {"caption", "Go"},
        {"mode", "level"}  // slider field on button
    };
    auto err = VCValidate::validate(item, VCType::Button, true);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    // Should be caught by field validation (not value validation)
    QVERIFY(errJson.contains("invalidFields"));
    QCOMPARE(errJson["invalidFields"][0].get<std::string>(), std::string("mode"));
}

void VCValidation_Test::fullValidation_buttonCreateInvalidValue()
{
    Json item = {
        {"type", "button"}, {"parentID", 1}, {"caption", "Go"},
        {"action", "explode"}  // valid field, invalid value
    };
    auto err = VCValidate::validate(item, VCType::Button, true);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    // Should be caught by value validation
    QCOMPARE(errJson["field"].get<std::string>(), std::string("action"));
}

void VCValidation_Test::fullValidation_sliderUpdateValid()
{
    Json item = {
        {"widgetID", 5}, {"caption", "Master"},
        {"mode", "grandmaster"}, {"gmValueMode", "reduce"}, {"gmChannelMode", "allchannels"},
        {"bgColor", "#001122"}
    };
    auto err = VCValidate::validate(item, VCType::Slider, false);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::fullValidation_sliderUpdateCrossTypeField()
{
    Json item = {
        {"widgetID", 5}, {"caption", "Test"},
        {"action", "flash"}  // button field on slider update
    };
    auto err = VCValidate::validate(item, VCType::Slider, false);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QVERIFY(errJson.contains("invalidFields"));
    QCOMPARE(errJson["invalidFields"][0].get<std::string>(), std::string("action"));
}

// ========== parentID / pageIndex exclusivity ==========

void VCValidation_Test::parentOrPage_frameWithPageIndex_valid()
{
    Json item = {{"type", "frame"}, {"pageIndex", 0}, {"caption", "Top"}};
    auto err = VCValidate::validateParentOrPage(item, VCType::Frame);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::parentOrPage_frameWithParentID_valid()
{
    Json item = {{"type", "frame"}, {"parentID", 42}, {"caption", "Nested"}};
    auto err = VCValidate::validateParentOrPage(item, VCType::Frame);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::parentOrPage_frameWithBoth_rejected()
{
    Json item = {{"type", "frame"}, {"parentID", 0}, {"pageIndex", 0}};
    auto err = VCValidate::validateParentOrPage(item, VCType::Frame);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QVERIFY(errJson["error"].get<std::string>().find("mutually exclusive") != std::string::npos);
}

void VCValidation_Test::parentOrPage_frameWithNeither_rejected()
{
    Json item = {{"type", "frame"}, {"caption", "Orphan"}};
    auto err = VCValidate::validateParentOrPage(item, VCType::Frame);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QVERIFY(errJson["error"].get<std::string>().find("either pageIndex or parentID") != std::string::npos);
}

void VCValidation_Test::parentOrPage_soloframeWithPageIndex_valid()
{
    Json item = {{"type", "soloframe"}, {"pageIndex", 1}};
    auto err = VCValidate::validateParentOrPage(item, VCType::SoloFrame);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::parentOrPage_soloframeWithBoth_rejected()
{
    Json item = {{"type", "soloframe"}, {"parentID", 0}, {"pageIndex", 1}};
    auto err = VCValidate::validateParentOrPage(item, VCType::SoloFrame);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QVERIFY(errJson["error"].get<std::string>().find("mutually exclusive") != std::string::npos);
}

void VCValidation_Test::parentOrPage_buttonWithParentID_valid()
{
    Json item = {{"type", "button"}, {"parentID", 10}};
    auto err = VCValidate::validateParentOrPage(item, VCType::Button);
    QVERIFY2(err.empty(), err.c_str());
}

void VCValidation_Test::parentOrPage_buttonWithPageIndex_rejected()
{
    Json item = {{"type", "button"}, {"parentID", 10}, {"pageIndex", 0}};
    auto err = VCValidate::validateParentOrPage(item, VCType::Button);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QVERIFY(errJson["error"].get<std::string>().find("not valid for widget type") != std::string::npos);
}

void VCValidation_Test::parentOrPage_buttonWithNeither_rejected()
{
    Json item = {{"type", "button"}, {"caption", "Go"}};
    auto err = VCValidate::validateParentOrPage(item, VCType::Button);
    QVERIFY(!err.empty());
    auto errJson = Json::parse(err);
    QVERIFY(errJson["error"].get<std::string>().find("parentID is required") != std::string::npos);
}

QTEST_MAIN(VCValidation_Test)
