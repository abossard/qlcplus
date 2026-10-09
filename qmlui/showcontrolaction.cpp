#include "showcontrolaction.h"

#include <cmath>

#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "rgbmatrix.h"
#include "showcommandrecorder.h"
#include "vcanimation.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vcxypad.h"
#include "vcxypadpreset.h"
#include "vcanimationpreset.h"
#include "rgbtext.h"
#include "virtualconsole.h"
#include "vcpage.h"
#include "vcsoloframe.h"
#include "collection.h"
#include "inputoutputmap.h"

ShowControlCoupling ShowControlAction::coupling(Doc *doc, VirtualConsole *vc,
                                               VCWidget *control, quint32 functionId)
{
    const auto soloGroupOf = [](VCWidget *widget)
    {
        for (VCWidget *parent = qobject_cast<VCWidget *>(widget->parent()); parent != nullptr;
             parent = qobject_cast<VCWidget *>(parent->parent()))
        {
            if (parent->type() == VCWidget::SoloFrameWidget)
                return parent->id();
            if (parent->type() != VCWidget::FrameWidget)
                break;
        }
        return ShowCommand::InvalidId;
    };
    ShowControlCoupling result;
    result.functionId = functionId;
    if (control != nullptr)
    {
        result.controlId = control->recordingId();
        result.soloGroup = soloGroupOf(control);
    }
    QList<quint32> pending{functionId};
    while (doc != nullptr && !pending.isEmpty())
    {
        auto *collection = qobject_cast<Collection *>(doc->function(pending.takeFirst()));
        if (collection == nullptr)
            continue;
        for (quint32 member : collection->functions())
        {
            if (member == functionId || result.startsFunctions.contains(member) || doc->function(member) == nullptr)
                continue;
            result.startsFunctions.append(member);
            pending.append(member);
        }
    }
    for (int page = 0; vc != nullptr && (control == nullptr || !result.startsFunctions.isEmpty()) &&
                       page < vc->pagesCount(); ++page)
    {
        for (VCWidget *widget : vc->page(page)->children(true))
        {
            const auto *button = qobject_cast<VCButton *>(widget);
            if (button == nullptr || (button->functionID() != functionId &&
                                     !result.startsFunctions.contains(button->functionID())))
                continue;
            const quint32 solo = soloGroupOf(widget);
            if (solo != ShowCommand::InvalidId)
                result.memberSoloGroups.append(qMakePair(button->functionID(), solo));
        }
    }
    return result;
}

ShowControlAction::ReceiptPlan ShowControlAction::planReceipt(const ShowControlCoupling &done,
                                                             ShowCommandFsm::ShowButtonOp operation,
                                                             const ShowControlCoupling *next)
{
    ReceiptPlan plan;
    plan.depends = next == nullptr || ShowCommandFsm::controlDependsOn(*next, done);
    if (!plan.depends)
        return plan;

    ShowFunctionExpectation own;
    own.functionId = done.functionId;
    own.expectLive = operation == ShowCommandFsm::ShowButtonOp::Start;
    plan.functions.append(own);
    const auto member = [&](quint32 functionId)
    {
        ShowFunctionExpectation child;
        child.functionId = functionId;
        child.causeFunctionId = done.functionId;
        plan.functions.append(child);
    };
    if (own.expectLive && next != nullptr)
    {
        if (done.startsFunctions.contains(next->functionId))
            member(next->functionId);
        // Wait for the started member before the next operation takes its Solo Frame.
        for (const QPair<quint32, quint32> &solo : done.memberSoloGroups)
        {
            if (next->soloGroup != ShowCommand::InvalidId && solo.second == next->soloGroup &&
                solo.first != done.functionId)
                member(solo.first);
        }
    }
    return plan;
}

ShowControlAction::Resolution ShowControlAction::resolve(Doc *doc, VirtualConsole *vc,
                                                         const ShowCommand &command)
{
    Resolution result;
    QVector<ShowControlSnapshot> snapshots;
    const auto controls = vc != nullptr ? vc->widgetsByRecordingId(command.controlId) : QList<VCWidget *>();
    for (VCWidget *control : controls)
        snapshots.append(configuration(doc, control).snapshot);
    result.status = ShowCommandFsm::resolveControl(command, snapshots, &result.reason);
    if (result.status == ShowControlStatus::Ready)
    {
        result.reason = readiness(controls.first(), command);
        if (!result.reason.isEmpty())
            result.status = ShowControlStatus::Incompatible;
        else
            result.control = controls.first();
    }
    return result;
}

QVector<ShowControlAction::ClosureFact> ShowControlAction::closure(Doc *doc, VirtualConsole *vc,
                                                                 VCWidget *control,
                                                                 const FunctionParent &owner)
{
    QVector<ClosureFact> result;
    const auto add = [&](VCWidget *member, bool stopped, bool conditional)
    {
        for (const auto &known : result)
            if (known.control == member)
                return;
        ClosureFact fact;
        fact.control = member;
        fact.before = fact.after = observe(member);
        if (stopped)
            fact.after.scalar = 0;
        fact.conditionalOnStart = conditional;
        Function *function = doc != nullptr ? doc->function(functionId(member)) : nullptr;
        fact.ownerBefore = function != nullptr && function->hasSource(owner);
        result.append(fact);
    };
    add(control, false, false);
    if (auto *button = qobject_cast<VCButton *>(control))
    {
        const auto native = coupling(doc, vc, control, functionId(control));
        auto starts = native.memberSoloGroups;
        if (native.soloGroup != ShowCommand::InvalidId)
            starts.prepend(qMakePair(native.functionId, native.soloGroup));
        for (const auto &start : starts)
        {
            auto *frame = vc != nullptr ? qobject_cast<VCSoloFrame *>(vc->widget(start.second)) : nullptr;
            if (frame == nullptr)
                continue;
            for (VCWidget *child : frame->children(true))
                if (auto *sibling = qobject_cast<VCButton *>(child); sibling != nullptr && sibling != button)
                    add(sibling, sibling->soloStartStops(start.first, frame->excludeMonitoredFunctions()), true);
        }
    }
    else if (auto *slider = qobject_cast<VCSlider *>(control))
    {
        auto *frame = qobject_cast<VCWidget *>(slider->parent());
        if (slider->sliderMode() == VCSlider::Submaster && frame != nullptr)
            for (VCSlider *child : frame->findChildren<VCSlider *>())
                add(child, false, false);
    }
    return result;
}

quint32 ShowControlAction::functionId(VCWidget *control, const ShowCommandInput *input)
{
    if (const auto *button = qobject_cast<VCButton *>(control))
        return button->functionID();
    if (const auto *animation = qobject_cast<VCAnimation *>(control))
        return animation->functionID();
    if (const auto *pad = qobject_cast<VCXYPad *>(control))
    {
        const auto *choice = input != nullptr ? std::get_if<ShowCommandChoice>(&input->payload.value) : nullptr;
        const int selected = choice != nullptr ? choice->choice : pad->activePresetId();
        const auto *preset = selected >= 0 ? pad->findPreset(quint8(selected)) : nullptr;
        return preset != nullptr && (preset->m_type == VCXYPadPreset::EFX || preset->m_type == VCXYPadPreset::Scene)
            ? preset->m_funcID : ShowCommand::InvalidId;
    }
    const auto *slider = qobject_cast<VCSlider *>(control);
    return slider != nullptr && slider->sliderMode() == VCSlider::Adjust
        ? slider->controlledFunction() : ShowCommand::InvalidId;
}

void ShowControlAction::connectWriter(VCWidget *control, QObject *receiver)
{
    QObject::connect(control, SIGNAL(recordedWriteRetired(quint64,int,quint32,int,QString)),
                     receiver, SLOT(slotRecordedWriteRetired(quint64,int,quint32,int,QString)), Qt::UniqueConnection);
}

void ShowControlAction::cancelWriter(VCWidget *control, quint64 generation)
{
    if (auto *slider = qobject_cast<VCSlider *>(control))
        slider->cancelRecordedWrite(generation);
    else if (auto *pad = qobject_cast<VCXYPad *>(control))
        pad->cancelPendingRecordedWrite(generation);
}

std::optional<ShowFunctionExpectation> ShowControlAction::writerExpectation(quint32 functionId, int effect)
{
    if (effect == VCSlider::RecordedWriteNoEffect)
        return std::nullopt;
    return ShowFunctionExpectation{functionId, effect == VCSlider::RecordedWriteStarted, ShowCommand::InvalidId};
}

int ShowControlAction::scalarState(VCWidget *control)
{
    if (const auto *button = qobject_cast<VCButton *>(control))
        return button->state() == VCButton::Active ? 1 : 0;
    if (const auto *slider = qobject_cast<VCSlider *>(control))
        return slider->value();
    if (const auto *animation = qobject_cast<VCAnimation *>(control))
        return animation->faderLevel();
    return 0;
}

ShowControlConfiguration ShowControlAction::configuration(Doc *doc, VCWidget *control,
                                                          const ShowCommandInput *input)
{
    ShowControlConfiguration result;
    auto &snapshot = result.snapshot;
    snapshot.enabled = !control->isDisabled();
    result.functionId = functionId(control, input);
    result.functionLifetime = doc != nullptr ? doc->function(result.functionId) : nullptr;
    if (auto *button = qobject_cast<VCButton *>(control))
    {
        switch (button->actionType())
        {
            case VCButton::Toggle: snapshot.role = ShowControlRole::ToggleButton; break;
            case VCButton::Flash: snapshot.role = ShowControlRole::FlashButton; break;
            case VCButton::Blackout: snapshot.role = ShowControlRole::BlackoutButton; break;
            case VCButton::Freeze: snapshot.role = ShowControlRole::FreezeButton; break;
            case VCButton::FreezeHold: snapshot.role = ShowControlRole::FreezeHoldButton; break;
            case VCButton::StopAll: break;
        }
        snapshot.bound = doc != nullptr && doc->function(button->functionID()) != nullptr;
    }
    if (auto *slider = qobject_cast<VCSlider *>(control))
    {
        result.sliderClickAndGoType = int(slider->clickAndGoType());
        result.sliderLow = slider->rangeLowLimit();
        result.sliderHigh = slider->rangeHighLimit();
        switch (slider->sliderMode())
        {
            case VCSlider::Level:
                snapshot.role = ShowControlRole::LevelSlider;
                result.levelChannels = slider->levelChannels();
                break;
            case VCSlider::Submaster: snapshot.role = ShowControlRole::SubmasterSlider; break;
            case VCSlider::GrandMaster: snapshot.role = ShowControlRole::GrandMasterSlider; break;
            case VCSlider::Adjust:
            {
                Function *function = doc != nullptr ? doc->function(slider->controlledFunction()) : nullptr;
                snapshot.role = ShowControlRole::AdjustSlider;
                snapshot.bound = function != nullptr;
                if (slider->controlledAttribute() == Function::Intensity)
                    snapshot.attribute = QStringLiteral("Intensity");
                else if (function != nullptr)
                    snapshot.attribute = Function::typeToString(function->type()) + QLatin1Char(':') +
                                         QString::number(slider->controlledAttribute());
                break;
            }
        }
    }
    if (auto *pad = qobject_cast<VCXYPad *>(control))
    {
        snapshot.role = ShowControlRole::XYPad;
        result.nativeBinding.insert(QStringLiteral("floor"), pad->floorControl());
        result.nativeBinding.insert(QStringLiteral("inverted"), !pad->floorControl() && pad->invertedAppearance());
        const QVector3D size = pad->floorSize();
        result.nativeBinding.insert(QStringLiteral("stage"),
                                    QVariantList{size.x(), size.y(), size.z()});
        const QPointF horizontal = pad->horizontalRange(), vertical = pad->verticalRange();
        result.nativeBinding.insert(QStringLiteral("ranges"),
                                    QVariantList{horizontal.x(), horizontal.y(), vertical.x(), vertical.y()});
        QVariantList heads;
        const auto retainHead = [&](const GroupHead &head)
        {
            Fixture *fixture = doc != nullptr ? doc->fixture(head.fxi) : nullptr;
            result.nativeLifetimes.append(fixture);
            heads.append(QVariantList{head.fxi, head.head, fixture != nullptr ? int(fixture->universe()) : -1,
                                      fixture != nullptr ? int(fixture->address()) : -1,
                                      fixture != nullptr ? int(fixture->channels()) : -1,
                                      fixture != nullptr ? int(fixture->heads()) : -1});
        };
        for (const auto &fixture : pad->fixtures())
        {
            heads.append(QVariantList{fixture.m_head.fxi, fixture.m_head.head, fixture.m_groupID,
                                      fixture.m_xMin, fixture.m_xMax, fixture.m_yMin, fixture.m_yMax,
                                      fixture.m_xReverse, fixture.m_yReverse, fixture.m_enabled});
            if (fixture.m_groupID != FixtureGroup::invalidId())
                result.nativeLifetimes.append(doc != nullptr ? doc->fixtureGroup(fixture.m_groupID) : nullptr);
            for (const GroupHead &head : pad->entryHeads(fixture))
                retainHead(head);
        }
        result.nativeBinding.insert(QStringLiteral("heads"), heads);
        QVariantList choices;
        const auto *acceptedChoice = input != nullptr ? std::get_if<ShowCommandChoice>(&input->payload.value) : nullptr;
        for (const auto *preset : pad->m_presets)
        {
            if (preset->m_id != pad->activePresetId() &&
                (acceptedChoice == nullptr || preset->m_id != acceptedChoice->choice))
                continue;
            result.nativeLifetimes.append(const_cast<VCXYPadPreset *>(preset));
            QVariantList choice{preset->m_id, int(preset->m_type), preset->m_funcID, preset->m_fxGroupID};
            if (preset->m_type == VCXYPadPreset::Scene || preset->m_type == VCXYPadPreset::EFX)
                result.nativeLifetimes.append(doc != nullptr ? doc->function(preset->m_funcID) : nullptr);
            if (preset->m_type == VCXYPadPreset::FixtureGroup)
            {
                if (preset->m_fxGroupID != FixtureGroup::invalidId())
                    result.nativeLifetimes.append(doc != nullptr ? doc->fixtureGroup(preset->m_fxGroupID) : nullptr);
                for (const GroupHead &head : pad->presetHeads(preset))
                {
                    choice.append(QVariantList{head.fxi, head.head});
                    result.nativeLifetimes.append(doc != nullptr ? doc->fixture(head.fxi) : nullptr);
                }
            }
            choices.append(choice);
        }
        result.nativeBinding.insert(QStringLiteral("choices"), choices);
    }
    if (auto *animation = qobject_cast<VCAnimation *>(control))
    {
        snapshot.role = ShowControlRole::AnimationFader;
        snapshot.bound = doc != nullptr && qobject_cast<RGBMatrix *>(doc->function(animation->functionID())) != nullptr;
        QVariantList choices;
        const auto *content = input != nullptr ? std::get_if<ShowCommandContent>(&input->payload.value) : nullptr;
        const auto *color = input != nullptr ? std::get_if<ShowCommandMatrixColor>(&input->payload.value) : nullptr;
        const int selected = content != nullptr ? content->choice : color != nullptr ? color->choice : -1;
        for (const auto *preset : animation->m_controls)
        {
            if (preset->m_id != selected)
                continue;
            result.nativeLifetimes.append(const_cast<VCAnimationPreset *>(preset));
            choices.append(QVariantList{preset->m_id, int(preset->m_type), preset->colorIndex(),
                                       int(preset->widgetType())});
        }
        result.nativeBinding.insert(QStringLiteral("choices"), choices);
        if (content != nullptr)
        {
            QVariantList metadata;
            for (const QVariant &property : animation->algorithmProperties(content->algorithm))
            {
                QVariantMap definition = property.toMap();
                definition.remove(QStringLiteral("value"));
                metadata.append(definition);
            }
            result.nativeBinding.insert(QStringLiteral("algorithm"), content->algorithm);
            result.nativeBinding.insert(QStringLiteral("metadata"), metadata);
        }
    }
    return result;
}

QString ShowControlAction::readiness(VCWidget *control, const ShowCommand &command)
{
    if (ShowCommand::hasTypedPayload(command.action))
    {
        const QString invalid = command.payload.validate(command.action);
        if (!invalid.isEmpty())
            return invalid;
        if (const auto *choice = std::get_if<ShowCommandChoice>(&command.payload.value))
        {
            auto *pad = qobject_cast<VCXYPad *>(control);
            const auto *preset = pad != nullptr ? pad->findPreset(quint8(choice->choice)) : nullptr;
            if (preset == nullptr)
                return QStringLiteral("XY choice no longer exists");
            const bool compatible = command.action == ShowCommandAction::SetXYPadPositionPreset
                ? preset->m_type == VCXYPadPreset::Position
                : command.action == ShowCommandAction::SetXYPadGroupPreset
                    ? preset->m_type == VCXYPadPreset::FixtureGroup
                    : preset->m_type == VCXYPadPreset::EFX || preset->m_type == VCXYPadPreset::Scene;
            if (!compatible)
                return QStringLiteral("XY choice has an incompatible type");
            if (command.action == ShowCommandAction::SetXYPadFunctionPreset)
            {
                Function *function = pad->m_doc->function(preset->m_funcID);
                if (function == nullptr || function->type() !=
                    (preset->m_type == VCXYPadPreset::EFX ? Function::EFXType : Function::SceneType))
                    return QStringLiteral("XY choice function is missing or incompatible");
            }
            if (command.action == ShowCommandAction::SetXYPadGroupPreset)
            {
                const auto heads = pad->presetHeads(preset);
                if (heads.isEmpty())
                    return QStringLiteral("XY choice has no current heads");
                for (const GroupHead &head : heads)
                {
                    const auto *fixture = pad->m_doc->fixture(head.fxi);
                    if (fixture == nullptr || head.head < 0 || head.head >= int(fixture->heads()))
                        return QStringLiteral("XY choice head is missing or incompatible");
                }
            }
        }
        if (const auto *color = std::get_if<ShowCommandMatrixColor>(&command.payload.value))
        {
            auto *animation = qobject_cast<VCAnimation *>(control);
            if (animation == nullptr || animation->currentMatrix() == nullptr ||
                color->index >= RGBAlgorithmColorDisplayCount)
                return QStringLiteral("Matrix color slot is unavailable");
            if (color->choice >= 0)
            {
                const auto *preset = animation->findControl(quint8(color->choice));
                if (preset == nullptr || preset->colorIndex() != color->index ||
                    (color->operation == ShowCommandMatrixColor::Operation::Component &&
                     (preset->widgetType() != VCAnimationPreset::Knob ||
                      (preset->m_color.red() != 0 ? 0 : preset->m_color.green() != 0 ? 1 : 2) != color->component)))
                    return QStringLiteral("Matrix color choice is incompatible");
            }
        }
        if (const auto *content = std::get_if<ShowCommandContent>(&command.payload.value))
        {
            auto *animation = qobject_cast<VCAnimation *>(control);
            if (animation == nullptr || animation->currentMatrix() == nullptr ||
                !animation->runtimeAlgorithms().contains(content->algorithm))
                return QStringLiteral("named Matrix algorithm is unavailable");
            if (content->choice >= 0)
            {
                const auto *preset = animation->findControl(quint8(content->choice));
                if (preset == nullptr ||
                    (content->algorithm == QStringLiteral("Text")
                        ? preset->m_type != VCAnimationPreset::Text
                        : preset->m_type != VCAnimationPreset::Animation))
                    return QStringLiteral("Matrix content choice is incompatible");
            }
            QMap<QString, QVariantMap> metadata;
            bool available = false;
            const QVariantList definitions = animation->algorithmProperties(content->algorithm, &available);
            if (!available)
                return QStringLiteral("named Matrix algorithm definition is unavailable");
            for (const QVariant &entry : definitions)
            {
                const QVariantMap property = entry.toMap();
                metadata.insert(property.value("name").toString(), property);
            }
            for (auto it = content->properties.cbegin(); it != content->properties.cend(); ++it)
            {
                if (!metadata.contains(it.key()))
                    return QStringLiteral("Matrix property '%1' is unavailable").arg(it.key());
                const auto &value = it.value();
                const QVariantMap property = metadata.value(it.key());
                const QString type = value.type == ShowCommandProperty::Type::List ? "List"
                    : value.type == ShowCommandProperty::Type::Range ? "Range"
                    : value.type == ShowCommandProperty::Type::Float ? "Float" : "String";
                if (property.value("type").toString() != type)
                    return QStringLiteral("Matrix property '%1' changed type").arg(it.key());
                if (type == "List" && !property.value("listValues").toStringList().contains(value.text))
                    return QStringLiteral("Matrix list property '%1' has an unavailable value").arg(it.key());
                if ((type == "Range" || type == "Float") &&
                    ((property.contains("min") && value.number < property.value("min").toDouble()) ||
                     (property.contains("max") && value.number > property.value("max").toDouble())))
                    return QStringLiteral("Matrix property '%1' is outside its current bounds").arg(it.key());
            }
        }
    }
    if (command.action == ShowCommandAction::SetXYPadFloor)
    {
        const auto *pad = qobject_cast<VCXYPad *>(control);
        if (pad == nullptr || !pad->floorControl())
            return QStringLiteral("XY pad is not in floor mode");
        ShowCommandFloor point;
        if (!ShowCommandFloor::decode(command.nativeArgument(), &point))
            return QStringLiteral("floor coordinates must be finite nonnegative metres");
        const QRectF area = pad->floorRangeArea();
        if (point.x < area.left() || point.x > area.right() ||
            point.z < area.top() || point.z > area.bottom() ||
            point.y > pad->floorHeightMax())
            return QStringLiteral("floor target is outside the native reachable bounds");
    }
    return QString();
}

QVariantMap ShowControlAction::editorInfo(VCWidget *control, const QString &algorithm)
{
    if (auto *animation = qobject_cast<VCAnimation *>(control))
        return {{"algorithms", animation->runtimeAlgorithms()},
                {"properties", animation->algorithmProperties(algorithm)}};
    if (auto *pad = qobject_cast<VCXYPad *>(control))
        return {{"choices", pad->presetsList()}, {"floorControl", pad->floorControl()},
                {"floorRangeArea", pad->floorRangeArea()}, {"floorHeightMax", pad->floorHeightMax()}};
    return {};
}

ShowCommandInput ShowControlAction::acceptedInput(const ShowControlRequest &request)
{
    ShowCommandInput input = request.input;
    if (input.action == ShowCommandAction::SetSliderChannel &&
        std::holds_alternative<std::monostate>(input.payload.value))
    {
        input.attribute.clear();
        input.payload.value = ShowCommandChannel{request.rawValue, request.accepted.snapshot.attribute};
    }
    else if (input.action == ShowCommandAction::SetSliderPosition)
    {
        input.attribute = request.accepted.snapshot.attribute;
        input.position = (qreal(request.rawValue) - request.accepted.sliderLow) /
                         (request.accepted.sliderHigh - request.accepted.sliderLow);
    }
    else if (input.action == ShowCommandAction::SetSliderReset)
        input.attribute = request.accepted.snapshot.attribute;
    return input;
}

ShowControlAction::CapturePlan ShowControlAction::planCapture(const ShowControlRequest &request,
                                                             const Values &before,
                                                             const ShowControlRequest *previous)
{
    CapturePlan plan;
    plan.input = request.input;
    plan.supported = ShowCommand::isControlAction(plan.input.action);
    if (const auto *channel = std::get_if<ShowCommandChannel>(&plan.input.payload.value))
    {
        const auto *prior = previous != nullptr
            ? std::get_if<ShowCommandChannel>(&previous->input.payload.value) : nullptr;
        plan.unchanged = prior != nullptr ? channel->binding == prior->binding && channel->value == prior->value
                                        : channel->value == before.scalar;
        return plan;
    }
    if (ShowCommand::hasTypedPayload(plan.input.action))
        return plan;
    if (plan.input.action == ShowCommandAction::SetSliderPosition ||
        plan.input.action == ShowCommandAction::SetAnimationFader)
        plan.unchanged = request.rawValue == (previous != nullptr ? previous->rawValue : before.scalar);
    if (plan.input.action == ShowCommandAction::SetXYPadPosition)
    {
        const auto *point = std::get_if<ShowCommandPanTilt>(&plan.input.payload.value);
        const auto *prior = previous != nullptr
            ? std::get_if<ShowCommandPanTilt>(&previous->input.payload.value) : nullptr;
        plan.unchanged = point != nullptr && QPointF(point->pan, point->tilt) ==
            (prior != nullptr ? QPointF(prior->pan, prior->tilt) : before.point);
    }
    return plan;
}

ShowCommandInput ShowControlAction::acceptedPreset(VCWidget *control, int choice, int knobValue,
                                                   ShowCommandOrigin origin)
{
    ShowCommandInput input;
    input.origin = origin;
    const auto reject = [&](const QString &reason) {
        if (auto *recorder = ShowCommandRecorder::instance())
            recorder->reportUnsupported(reason, origin, control);
        return ShowCommandInput();
    };
    if (choice < 0 || choice > 255)
        return input;
    if (auto *pad = qobject_cast<VCXYPad *>(control))
    {
        const auto *preset = pad->findPreset(quint8(choice));
        if (preset == nullptr)
            return input;
        input.role = ShowControlRole::XYPad;
        ShowCommandChoice value;
        value.choice = choice;
        value.active = preset->m_type == VCXYPadPreset::Position || pad->activePresetId() != choice;
        if (preset->m_type == VCXYPadPreset::Position)
        {
            input.action = ShowCommandAction::SetXYPadPositionPreset;
            value.point = {preset->m_dmxPos.x(), preset->m_dmxPos.y()};
        }
        else if (preset->m_type == VCXYPadPreset::FixtureGroup)
            input.action = ShowCommandAction::SetXYPadGroupPreset;
        else
            input.action = ShowCommandAction::SetXYPadFunctionPreset;
        input.payload.value = value;
    }
    else if (auto *animation = qobject_cast<VCAnimation *>(control))
    {
        const auto *preset = animation->findControl(quint8(choice));
        if (preset == nullptr)
            return input;
        input.role = ShowControlRole::AnimationFader;
        const int index = preset->colorIndex();
        if (index >= 0)
        {
            input.action = ShowCommandAction::SetAnimationColor;
            ShowCommandMatrixColor value;
            value.index = index;
            value.choice = choice;
            if (preset->widgetType() == VCAnimationPreset::Knob)
            {
                if (knobValue < 0)
                    return ShowCommandInput();
                value.operation = ShowCommandMatrixColor::Operation::Component;
                value.component = preset->m_color.red() != 0 ? 0 : preset->m_color.green() != 0 ? 1 : 2;
                value.value = qBound(0, knobValue, 255);
            }
            else if (preset->m_type >= VCAnimationPreset::Color1Reset &&
                     preset->m_type <= VCAnimationPreset::Color5Reset)
            {
                value.operation = ShowCommandMatrixColor::Operation::Reset;
                value.choice = -1;
            }
            else
                value.color = {quint8(preset->m_color.red()), quint8(preset->m_color.green()),
                               quint8(preset->m_color.blue()), 0, 0, 0};
            input.payload.value = value;
        }
        else
        {
            input.action = ShowCommandAction::SetAnimationContent;
            ShowCommandContent content;
            content.choice = choice;
            content.algorithm = preset->m_type == VCAnimationPreset::Text ? QStringLiteral("Text") : preset->m_resource;
            if (preset->m_type == VCAnimationPreset::Text)
                content.text = preset->m_resource;
            const QVariantList metadata = animation->algorithmProperties(content.algorithm);
            for (const QVariant &entry : metadata)
            {
                const QVariantMap property = entry.toMap();
                const QString key = property.value("name").toString();
                if (!preset->m_properties.contains(key))
                    continue;
                ShowCommandProperty value;
                const QString type = property.value("type").toString();
                const QString accepted = preset->m_properties.value(key);
                if (type == "List" || type == "String")
                {
                    value.type = type == "List" ? ShowCommandProperty::Type::List : ShowCommandProperty::Type::String;
                    value.text = accepted;
                }
                else
                {
                    value.type = type == "Range" ? ShowCommandProperty::Type::Range : ShowCommandProperty::Type::Float;
                    bool valid = false;
                    value.number = accepted.toDouble(&valid);
                    if (!valid)
                        return reject(QStringLiteral("Matrix property '%1' is not numeric").arg(key));
                }
                content.properties.insert(key, value);
            }
            for (auto it = preset->m_properties.cbegin(); it != preset->m_properties.cend(); ++it)
                if (!content.properties.contains(it.key()))
                    return reject(QStringLiteral("Matrix property '%1' is unavailable").arg(it.key()));
            input.payload.value = content;
        }
    }
    return input;
}

bool ShowControlAction::submit(Doc *doc, VCWidget *control, const ShowCommandInput &input)
{
    if (!ShowCommand::hasTypedPayload(input.action))
        return false;
    if (auto *recorder = ShowCommandRecorder::instance())
    {
        ShowControlRequest request;
        request.control = control;
        request.input = input;
        recorder->requestUserControl(request);
        return true;
    }
    return apply(doc, control, input, FunctionParent(FunctionParent::ManualVCWidget, control->id()),
                 false).refusal.isEmpty();
}

static ShowCommand candidateFor(VCWidget *control, const ShowCommandInput &input)
{
    ShowCommand candidate;
    candidate.id = 0;
    candidate.action = input.action;
    candidate.controlId = control->recordingId().isNull()
        ? QUuid(QStringLiteral("{00000000-0000-0000-0000-000000000001}")) : control->recordingId();
    candidate.role = input.role;
    candidate.attribute = input.attribute;
    candidate.on = input.on;
    candidate.position = input.position;
    candidate.payload = input.payload;
    return candidate.canonicalized();
}

QString ShowControlAction::preflight(VCWidget *control, const ShowCommandInput &input)
{
    const ShowCommand candidate = candidateFor(control, input);
    const QString reason = ShowCommand::validate(candidate);
    return reason.isEmpty() ? readiness(control, candidate) : reason;
}

ShowControlAction::Receipt ShowControlAction::apply(Doc *doc, VCWidget *control,
                                                   const ShowCommandInput &input,
                                                   const FunctionParent &owner, bool replay,
                                                   bool strictRelease, const ShowControlRequest *live)
{
    Receipt result;
    result.control = control;
    auto *animationTarget = qobject_cast<VCAnimation *>(control);
    auto *matrixTarget = animationTarget != nullptr ? animationTarget->currentMatrix() : nullptr;
    QMutexLocker algorithmLocker(matrixTarget != nullptr ? &matrixTarget->algorithmMutex() : nullptr);
    if (matrixTarget != nullptr)
        matrixTarget->beginCallbackOperation();
    result.atomicNativeState = matrixTarget != nullptr;
    result.before = observe(control);
    const auto finish = [&]() {
        result.after = observe(control);
        if (matrixTarget != nullptr && (result.refusal.isEmpty() || result.executionFailed))
        {
            const QString error = matrixTarget->callbackError();
            if (!error.isEmpty())
            {
                result.executionFailed = true;
                result.refusal = QStringLiteral("notApplied: native Matrix callback failed: ") + error;
            }
        }
        if (qobject_cast<VCSlider *>(control) != nullptr)
        {
            const bool value = input.action == ShowCommandAction::SetSliderPosition ||
                               input.action == ShowCommandAction::SetSliderChannel;
            result.unchanged = value && result.before.scalar == result.after.scalar &&
                               result.before.overriding == result.after.overriding;
            result.requested = input.action == ShowCommandAction::SetButtonState;
        }
        if (replay && result.refusal.isEmpty())
        {
            if (result.writerGeneration != 0)
                result.writes.append(qMakePair(QPointer<VCWidget>(control), result.writerGeneration));
            const auto sliderReceipt = [&](VCSlider *slider, quint64 generation)
            {
                if (generation == 0 && slider->sliderMode() == VCSlider::Adjust &&
                    slider->controlledAttribute() == Function::Intensity &&
                    doc->function(slider->controlledFunction()) != nullptr)
                    result.functions.append({slider->controlledFunction(), true, ShowCommand::InvalidId});
            };
            if (auto *slider = qobject_cast<VCSlider *>(control))
            {
                if (input.action != ShowCommandAction::SetButtonState &&
                    input.action != ShowCommandAction::SetSliderReset)
                    sliderReceipt(slider, result.writerGeneration);
                VCWidget *frame = qobject_cast<VCWidget *>(slider->parent());
                if (slider->sliderMode() == VCSlider::Submaster && frame != nullptr)
                {
                    for (VCSlider *child : frame->findChildren<VCSlider *>())
                    {
                        if (child == slider)
                            continue;
                        const quint64 generation = child->awaitPendingWrite();
                        if (generation != 0)
                            result.writes.append(qMakePair(QPointer<VCWidget>(child), generation));
                        sliderReceipt(child, generation);
                    }
                }
            }
        }
        return result;
    };
    const ShowCommand candidate = candidateFor(control, input);
    result.refusal = preflight(control, input);
    if (!result.refusal.isEmpty())
        return finish();

    if (auto *button = qobject_cast<VCButton *>(control))
    {
        if (!replay && input.role == ShowControlRole::ToggleButton && !input.on &&
            button->state() == VCButton::Inactive)
        {
            Function *target = doc != nullptr ? doc->function(button->functionID()) : nullptr;
            if (target != nullptr && target->isRunning())
                button->releaseToMonitoring();
        }
        result.buttonOp = replay ? button->applyRecordedState(input.on, owner, strictRelease)
                                 : button->applyUserState(input.on);
    }
    else if (auto *slider = qobject_cast<VCSlider *>(control))
    {
        if (input.action == ShowCommandAction::SetButtonState)
            slider->flashFunction(input.on);
        else if (input.action == ShowCommandAction::SetSliderReset)
            slider->applyRecordedReset();
        else if (const auto *channel = std::get_if<ShowCommandChannel>(&input.payload.value))
        {
            if (replay)
                result.writerGeneration = slider->applyRecordedValue(channel->value, owner, strictRelease);
            else
                slider->setValue(channel->value, true, live == nullptr || live->updateFeedback);
        }
        else
        {
            if (input.action == ShowCommandAction::SetSliderColors)
            {
                ShowCommandColors colors;
                ShowCommandColors::decode(candidate.nativeArgument(), &colors);
                const QColor rgb(colors.red, colors.green, colors.blue);
                const QColor wauv(colors.white, colors.amber, colors.ultraviolet);
                if (replay)
                    result.writerGeneration = slider->applyRecordedColors(
                        rgb, wauv, int(std::lround(input.position * 255.0)), owner, true, strictRelease);
                else
                    slider->setClickAndGoColors(rgb, wauv);
            }
            else if (replay)
                result.writerGeneration = input.action == ShowCommandAction::SetSliderPosition &&
                                          slider->clickAndGoType() == VCSlider::CnGPreset
                    ? slider->applyRecordedValue(int(std::lround(input.position * 255.0)), owner, strictRelease)
                    : slider->applyRecordedPosition(input.position, owner);
            else if (input.action != ShowCommandAction::SetSliderColors)
                slider->setValue(live != nullptr ? live->rawValue
                                 : int(std::lround(slider->rangeLowLimit() +
                                     input.position * (slider->rangeHighLimit() - slider->rangeLowLimit()))),
                                 true, live == nullptr || live->updateFeedback);
        }
    }
    else if (auto *pad = qobject_cast<VCXYPad *>(control))
    {
        if (const auto *ranges = std::get_if<ShowCommandRanges>(&input.payload.value))
        {
            result.writerGeneration = pad->updateRanges(QPointF(ranges->horizontal.pan, ranges->horizontal.tilt),
                                                        QPointF(ranges->vertical.pan, ranges->vertical.tilt), replay);
        }
        else if (const auto *choice = std::get_if<ShowCommandChoice>(&input.payload.value))
        {
            auto *preset = pad->findPreset(quint8(choice->choice));
            if (pad->m_activePresetId >= 0 &&
                (pad->m_activePresetId != choice->choice || !choice->active))
            {
                const quint32 previousFunction = functionId(pad);
                pad->deactivatePreset(pad->findPreset(quint8(pad->m_activePresetId)), replay);
                if (previousFunction != ShowCommand::InvalidId &&
                    (input.action != ShowCommandAction::SetXYPadFunctionPreset ||
                     previousFunction != preset->m_funcID))
                    result.functions.append({previousFunction, false, ShowCommand::InvalidId});
            }
            if (!choice->active)
                pad->setActivePresetId(-1);
            else if (input.action == ShowCommandAction::SetXYPadPositionPreset)
            {
                const QPointF position(choice->point.pan, choice->point.tilt);
                if (replay)
                    result.writerGeneration = pad->applyRecordedPosition(position);
                else
                    pad->setCurrentPosition(position);
                pad->setActivePresetId(choice->choice);
            }
            else if (pad->activatePreset(preset, owner, replay))
                pad->setActivePresetId(choice->choice);
            if (input.action == ShowCommandAction::SetXYPadFunctionPreset)
                result.functions.append({preset->m_funcID, choice->active, ShowCommand::InvalidId});
            if (replay && (input.action == ShowCommandAction::SetXYPadGroupPreset ||
                           result.before.headEnabled != observe(pad).headEnabled))
                result.writerGeneration = pad->awaitPendingWrite();
        }
        else if (input.action == ShowCommandAction::SetXYPadFloor)
        {
            ShowCommandFloor point;
            ShowCommandFloor::decode(candidate.nativeArgument(), &point);
            if (!pad->floorControl())
            {
                result.refusal = QStringLiteral("XY pad is not in floor mode");
                return finish();
            }
            const QVector3D position(point.x, point.y, point.z);
            if (replay)
                result.writerGeneration = pad->applyRecordedFloorPosition(position);
            else
                pad->setFloorPosition(position);
        }
        else
        {
            ShowCommandPanTilt point;
            ShowCommandPanTilt::decode(candidate.nativeArgument(), &point);
            if (replay)
                result.writerGeneration = pad->applyRecordedPosition(QPointF(point.pan, point.tilt));
            else
                pad->setCurrentPosition(QPointF(point.pan, point.tilt));
        }
    }
    else if (auto *animation = qobject_cast<VCAnimation *>(control))
    {
        if (const auto *color = std::get_if<ShowCommandMatrixColor>(&input.payload.value))
        {
            QColor accepted;
            if (color->operation == ShowCommandMatrixColor::Operation::Replace)
                accepted = QColor(color->color.red, color->color.green, color->color.blue);
            else if (color->operation == ShowCommandMatrixColor::Operation::Component)
            {
                accepted = animation->colorAt(color->index);
                if (!accepted.isValid())
                    accepted = QColor(0, 0, 0);
                if (color->component == 0) accepted.setRed(color->value);
                else if (color->component == 1) accepted.setGreen(color->value);
                else accepted.setBlue(color->value);
            }
            animation->setColorAt(color->index, accepted);
            if (color->choice >= 0 && color->operation != ShowCommandMatrixColor::Operation::Component)
                animation->setActivePresetId(color->choice);
            return finish();
        }
        if (const auto *content = std::get_if<ShowCommandContent>(&input.payload.value))
        {
            QMap<QString, QString> properties;
            for (auto it = content->properties.cbegin(); it != content->properties.cend(); ++it)
            {
                const auto &value = it.value();
                properties.insert(it.key(), value.type == ShowCommandProperty::Type::Range ||
                    value.type == ShowCommandProperty::Type::Float
                    ? QString::number(value.number, 'g', 17) : value.text);
            }
            if (!animation->setRuntimeContent(content->algorithm, content->text, properties, content->choice))
            {
                result.executionFailed = true;
                result.refusal = QStringLiteral("notApplied: native Matrix content callback failed");
            }
            return finish();
        }
        const int target = int(std::lround(input.position * 255.0));
        if (replay)
            animation->applyRecordedFaderLevel(target, owner, strictRelease);
        else if (ShowCommandFsm::isUserOrigin(input.origin) && animation->faderLevel() == target)
            animation->applyRecordedFaderLevel(target, animation->functionParent());
        else
            animation->setFaderLevel(target);
        if (doc->function(animation->functionID()) != nullptr)
            result.functions.append({animation->functionID(), target != 0, ShowCommand::InvalidId});
    }
    else
        result.refusal = QStringLiteral("not a supported native control");
    return finish();
}

ShowControlAction::State ShowControlAction::observe(VCWidget *control)
{
    State state;
    state.scalar = scalarState(control);
    if (auto *button = qobject_cast<VCButton *>(control))
        state.role = configuration(nullptr, button).snapshot.role;
    else if (auto *slider = qobject_cast<VCSlider *>(control))
    {
        state.role = slider->sliderMode() == VCSlider::Adjust ? ShowControlRole::AdjustSlider
            : slider->sliderMode() == VCSlider::Level ? ShowControlRole::LevelSlider
            : slider->sliderMode() == VCSlider::Submaster ? ShowControlRole::SubmasterSlider
                                                        : ShowControlRole::GrandMasterSlider;
        state.overriding = slider->isOverriding();
        state.colors = {slider->cngPrimaryColor(), slider->cngSecondaryColor()};
        state.nativeColorValue = slider->usesNativeColorValue();
    }
    else if (auto *pad = qobject_cast<VCXYPad *>(control))
    {
        state.role = ShowControlRole::XYPad;
        state.point = pad->currentPosition();
        state.floor = pad->floorPosition();
        state.floorMode = pad->floorControl();
        state.floorSize = pad->floorSize();
        state.horizontal = pad->horizontalRange();
        state.vertical = pad->verticalRange();
        state.choice = pad->activePresetId();
        state.choiceOwner = pad->m_presetOwner;
        state.choiceLifetime = state.choice >= 0 ? pad->findPreset(quint8(state.choice)) : nullptr;
        const auto native = configuration(pad->m_doc, pad);
        state.choiceFunctionLifetime = native.functionLifetime;
        state.binding = native.nativeBinding;
        state.binding.remove(QStringLiteral("ranges"));
        state.lifetimes = native.nativeLifetimes;
        for (const auto &fixture : pad->fixtures())
            state.headEnabled.append(fixture.m_enabled);
    }
    else if (auto *animation = qobject_cast<VCAnimation *>(control))
    {
        auto *matrix = animation->currentMatrix();
        QMutexLocker algorithmLocker(matrix != nullptr ? &matrix->algorithmMutex() : nullptr);
        state.role = ShowControlRole::AnimationFader;
        const auto native = configuration(animation->m_doc, animation);
        state.binding.insert(QStringLiteral("function"), native.functionId);
        state.lifetimes.append(native.functionLifetime);
        state.choice = animation->activePresetId();
        state.choiceLifetime = state.choice >= 0 ? animation->findControl(quint8(state.choice)) : nullptr;
        if (const auto *preset = animation->findControl(quint8(state.choice)); state.choice >= 0 && preset != nullptr)
            state.binding.insert(QStringLiteral("choiceDefinition"),
                                 QVariantList{int(preset->m_type), preset->colorIndex(), int(preset->widgetType())});
        for (int index = 0; index < RGBAlgorithmColorDisplayCount; ++index)
            state.colors.append(animation->colorAt(index));
        state.algorithm = animation->runtimeAlgorithms().value(animation->m_localAlgorithmIndex);
        state.algorithmOverride = animation->m_localAlgorithmIndex >= 0;
        state.contentOverride = animation->m_localContent;
        state.text = animation->m_localText;
        state.properties = animation->m_localProperties;
        if (matrix != nullptr)
        {
            state.matrixGroupLifetime = animation->m_doc->fixtureGroup(matrix->fixtureGroup());
            const auto *group = animation->m_doc->fixtureGroup(matrix->fixtureGroup());
            state.matrixGroupSize = group != nullptr ? group->size() : QSize();
            state.binding.insert(QStringLiteral("algorithmGeneration"), matrix->algorithmGeneration());
            if (state.algorithm.isEmpty() && matrix->algorithm() != nullptr)
                state.algorithm = matrix->algorithm()->name();
            QVariantList metadata;
            for (const QVariant &entry : animation->algorithmProperties(state.algorithm))
            {
                QVariantMap definition = entry.toMap();
                definition.remove(QStringLiteral("value"));
                metadata.append(definition);
            }
            state.binding.insert(QStringLiteral("contentMetadata"), metadata);
            if (!animation->m_localContent)
            {
                if (auto *text = dynamic_cast<RGBText *>(matrix->algorithm()))
                    state.text = text->text();
                for (const QVariant &entry : animation->algorithmProperties(state.algorithm))
                {
                    const QString name = entry.toMap().value("name").toString();
                    state.properties.insert(name, matrix->property(name));
                }
            }
        }
    }
    return state;
}

ShowControlAction::RestorePlan ShowControlAction::planRestore(const Values &before, const Values &after,
                                                             const Values &current,
                                                             bool sameBindingLifetimes, bool sameChoiceLifetime)
{
    RestorePlan plan;
    plan.scalar = before.scalar != after.scalar;
    plan.overriding = before.overriding != after.overriding;
    plan.point = before.point != after.point;
    plan.floor = before.floor != after.floor;
    plan.ranges = before.horizontal != after.horizontal || before.vertical != after.vertical;
    plan.choice = before.choice != after.choice || before.headEnabled != after.headEnabled ||
                  !(before.choiceOwner == after.choiceOwner);
    plan.colors = before.colors != after.colors || before.nativeColorValue != after.nativeColorValue;
    plan.content = before.algorithm != after.algorithm || before.text != after.text ||
                   before.properties != after.properties || before.algorithmOverride != after.algorithmOverride ||
                   before.contentOverride != after.contentOverride;
    plan.superseded = before.role != current.role ||
        ((before.role == ShowControlRole::XYPad || before.role == ShowControlRole::AnimationFader) &&
         (current.binding != after.binding || !sameBindingLifetimes)) ||
        (plan.scalar && current.scalar != after.scalar) ||
        (plan.overriding && current.overriding != after.overriding) ||
        (plan.point && current.point != after.point) ||
        (plan.floor && (current.floor != after.floor || current.floorMode != after.floorMode ||
                        current.floorSize != after.floorSize)) ||
        (plan.ranges && (current.horizontal != after.horizontal || current.vertical != after.vertical)) ||
        (plan.choice && (current.choice != after.choice || !sameChoiceLifetime ||
                         current.headEnabled != after.headEnabled ||
                         !(current.choiceOwner == after.choiceOwner))) ||
        (plan.colors && (current.colors != after.colors || current.nativeColorValue != after.nativeColorValue)) ||
        (plan.content && (current.algorithm != after.algorithm || current.text != after.text ||
                         current.properties != after.properties || current.algorithmOverride != after.algorithmOverride ||
                         current.contentOverride != after.contentOverride));
    return plan;
}

ShowControlAction::Receipt ShowControlAction::restore(Doc *doc, VCWidget *control, const State &before,
                                                     const State &after, const FunctionParent &owner)
{
    Receipt receipt;
    receipt.control = control;
    auto *animationTarget = qobject_cast<VCAnimation *>(control);
    auto *matrixTarget = animationTarget != nullptr ? animationTarget->currentMatrix() : nullptr;
    QMutexLocker algorithmLocker(matrixTarget != nullptr ? &matrixTarget->algorithmMutex() : nullptr);
    if (matrixTarget != nullptr)
        matrixTarget->beginCallbackOperation();
    receipt.before = observe(control);
    const RestorePlan plan = planRestore(before, after, receipt.before,
                                        receipt.before.lifetimes == after.lifetimes,
                                        receipt.before.choiceLifetime == after.choiceLifetime);
    if (plan.superseded)
    {
        receipt.refusal = QStringLiteral("native operation was superseded");
        receipt.after = receipt.before;
        return receipt;
    }
    if (auto *slider = qobject_cast<VCSlider *>(control))
    {
        if ((plan.colors || (plan.scalar && before.nativeColorValue)) && before.colors.count() == 2)
            receipt.writerGeneration = slider->applyRecordedColors(
                before.colors[0], before.colors[1], before.scalar, owner, before.nativeColorValue, true);
        else if (plan.scalar)
            receipt.writerGeneration = slider->applyRecordedValue(before.scalar, owner, true);
        if (plan.overriding)
            slider->setIsOverriding(before.overriding);
    }
    else if (auto *pad = qobject_cast<VCXYPad *>(control))
    {
        if (plan.choice && before.choice >= 0)
        {
            const auto *preset = pad->findPreset(quint8(before.choice));
            if (preset == nullptr || before.choiceLifetime != preset ||
                ((preset->m_type == VCXYPadPreset::EFX || preset->m_type == VCXYPadPreset::Scene) &&
                 (doc->function(preset->m_funcID) == nullptr ||
                  before.choiceFunctionLifetime != doc->function(preset->m_funcID))) ||
                (preset->m_type == VCXYPadPreset::FixtureGroup && pad->presetHeads(preset).isEmpty()))
            {
                receipt.refusal = QStringLiteral("previous native choice is no longer compatible");
                receipt.after = receipt.before;
                return receipt;
            }
        }
        if (plan.choice)
        {
            if (pad->m_activePresetId >= 0)
            {
                const quint32 activeFunction = functionId(pad);
                pad->deactivatePreset(pad->findPreset(quint8(pad->m_activePresetId)));
                if (activeFunction != ShowCommand::InvalidId)
                    receipt.functions.append({activeFunction, false, ShowCommand::InvalidId});
            }
            if (before.choice >= 0)
            {
                auto *preset = pad->findPreset(quint8(before.choice));
                if (preset != nullptr && preset->m_type != VCXYPadPreset::Position)
                {
                    pad->activatePreset(preset, before.choiceOwner);
                    if (preset->m_type == VCXYPadPreset::EFX || preset->m_type == VCXYPadPreset::Scene)
                        receipt.functions.append({preset->m_funcID, true, ShowCommand::InvalidId});
                }
            }
            pad->setActivePresetId(before.choice);
            if (before.headEnabled != receipt.before.headEnabled)
                receipt.writerGeneration = pad->updateSelection(before.headEnabled, true);
        }
        if (plan.ranges)
            receipt.writerGeneration = pad->applyRecordedRanges(before.horizontal, before.vertical);
        if (plan.floor)
            receipt.writerGeneration = pad->applyRecordedFloorPosition(before.floor);
        else if (plan.point)
            receipt.writerGeneration = pad->applyRecordedPosition(before.point);
    }
    else if (auto *animation = qobject_cast<VCAnimation *>(control))
    {
        QVariantList metadata;
        bool available = true;
        if (plan.content)
            for (const QVariant &entry : animation->algorithmProperties(before.algorithm, &available))
            {
                QVariantMap definition = entry.toMap();
                definition.remove(QStringLiteral("value"));
                metadata.append(definition);
            }
        const auto *priorChoice = before.choice >= 0 ? animation->findControl(quint8(before.choice)) : nullptr;
        if ((plan.choice && before.choice >= 0 &&
             (before.choiceLifetime.isNull() || priorChoice != before.choiceLifetime ||
              QVariantList{int(priorChoice->m_type), priorChoice->colorIndex(), int(priorChoice->widgetType())} !=
                  before.binding.value(QStringLiteral("choiceDefinition")).toList())) ||
            (plan.content && (!available || !animation->runtimeAlgorithms().contains(before.algorithm) ||
             receipt.before.matrixGroupLifetime.isNull() ||
             receipt.before.matrixGroupLifetime != after.matrixGroupLifetime ||
             receipt.before.matrixGroupSize != after.matrixGroupSize ||
             receipt.before.matrixGroupSize.isEmpty() ||
             metadata != before.binding.value(QStringLiteral("contentMetadata")).toList())))
        {
            receipt.refusal = QStringLiteral("previous Matrix content is no longer compatible");
            receipt.after = receipt.before;
            return receipt;
        }
        if (plan.content)
        {
            ShowCommandContent target;
            target.algorithm = before.algorithm;
            target.text = before.text;
            for (const QVariant &entry : metadata)
            {
                const QVariantMap definition = entry.toMap();
                const QString name = definition.value("name").toString();
                if (!before.properties.contains(name))
                    continue;
                ShowCommandProperty value;
                const QString type = definition.value("type").toString();
                const QString accepted = before.properties.value(name);
                if (type == "List" || type == "String")
                {
                    value.type = type == "List" ? ShowCommandProperty::Type::List : ShowCommandProperty::Type::String;
                    value.text = accepted;
                }
                else
                {
                    value.type = type == "Range" ? ShowCommandProperty::Type::Range : ShowCommandProperty::Type::Float;
                    bool valid = false;
                    value.number = accepted.toDouble(&valid);
                    if (!valid)
                        receipt.refusal = QStringLiteral("previous Matrix property is not numeric");
                }
                target.properties.insert(name, value);
            }
            ShowCommand candidate;
            candidate.action = ShowCommandAction::SetAnimationContent;
            candidate.payload.value = target;
            if (target.properties.count() != before.properties.count())
                receipt.refusal = QStringLiteral("previous Matrix property is unavailable");
            if (receipt.refusal.isEmpty())
                receipt.refusal = readiness(animation, candidate);
            if (!receipt.refusal.isEmpty())
            {
                receipt.after = receipt.before;
                return receipt;
            }
        }
        if (plan.colors)
            for (int index = 0; index < before.colors.count(); ++index)
                animation->setColorAt(index, before.colors[index]);
        if (plan.content)
        {
            if (before.contentOverride)
            {
                if (!animation->setRuntimeContent(before.algorithm, before.text, before.properties, before.choice))
                {
                    receipt.executionFailed = true;
                    receipt.refusal = QStringLiteral("notApplied: native Matrix content callback failed");
                }
            }
            else
            {
                animation->m_localContent = false;
                animation->m_localText.clear();
                animation->m_localProperties.clear();
                animation->setRuntimeAlgorithmIndex(before.algorithmOverride
                    ? animation->runtimeAlgorithms().indexOf(before.algorithm) : -1);
                if (auto *matrix = animation->currentMatrix())
                {
                    if (auto *text = dynamic_cast<RGBText *>(matrix->algorithm()))
                        text->setText(before.text);
                    for (auto it = before.properties.cbegin(); it != before.properties.cend(); ++it)
                    {
                        if (!matrix->setProperty(it.key(), it.value()))
                        {
                            receipt.executionFailed = true;
                            receipt.refusal = QStringLiteral("notApplied: native Matrix content callback failed");
                        }
                    }
                }
            }
        }
        if (plan.choice)
            animation->setActivePresetId(before.choice);
        if (plan.scalar)
        {
            animation->applyRecordedFaderLevel(before.scalar, owner, true);
            if (doc->function(animation->functionID()) != nullptr)
                receipt.functions.append({animation->functionID(), before.scalar != 0, ShowCommand::InvalidId});
        }
    }
    else if (auto *button = qobject_cast<VCButton *>(control))
        if (plan.scalar)
            receipt.buttonOp = button->applyRecordedState(before.scalar != 0, owner, true);
    receipt.after = observe(control);
    if (matrixTarget != nullptr)
    {
        const QString error = matrixTarget->callbackError();
        if (!error.isEmpty())
        {
            receipt.executionFailed = true;
            receipt.refusal = QStringLiteral("notApplied: native Matrix callback failed: ") + error;
        }
    }
    return receipt;
}

bool ShowControlAction::release(Doc *doc, VCWidget *control, const FunctionParent &owner,
                                const State *actual)
{
    if (actual != nullptr && actual->role == ShowControlRole::AnimationFader)
    {
        auto *function = qobject_cast<Function *>(actual->lifetimes.value(0));
        if (function == nullptr || !function->hasSource(owner))
            return false;
        function->stop(owner);
        return true;
    }
    if (auto *pad = qobject_cast<VCXYPad *>(control))
    {
        if (!(pad->m_presetOwner == owner) || pad->m_activePresetId < 0)
            return false;
        auto *preset = pad->findPreset(quint8(pad->m_activePresetId));
        if (preset == nullptr || (preset->m_type != VCXYPadPreset::EFX && preset->m_type != VCXYPadPreset::Scene))
            return false;
        pad->deactivatePreset(preset);
        pad->setActivePresetId(-1);
        return true;
    }

    if (auto *button = qobject_cast<VCButton *>(control))
        if (button->actionType() != VCButton::Toggle)
            return false;
    if (auto *slider = qobject_cast<VCSlider *>(control))
        if (slider->sliderMode() != VCSlider::Adjust || slider->controlledAttribute() != Function::Intensity)
            return false;
    Function *function = doc->function(functionId(control));
    if (function == nullptr || !function->hasSource(owner))
        return false;
    if (owner.type() == FunctionParent::ManualVCWidget)
        function->stopSource(owner);
    else
        function->stop(owner);
    return true;
}

void ShowControlAction::releaseHold(VirtualConsole *vc, VCWidget *control, ShowControlRole role,
                                    const FunctionParent &owner, const Holds &user, bool stopping)
{
    const auto flashHeld = [&](quint32 target)
    {
        if (target == ShowCommand::InvalidId || vc == nullptr)
            return false;
        for (quint32 id : user.flash)
            if (auto *other = qobject_cast<VCButton *>(vc->widget(id));
                other != nullptr && other != control && other->actionType() == VCButton::Flash &&
                other->functionID() == target)
                return true;
        for (quint32 id : user.sliderFlash)
            if (auto *other = qobject_cast<VCSlider *>(vc->widget(id));
                other != nullptr && other != control && other->sliderMode() == VCSlider::Adjust &&
                other->controlledFunction() == target)
                return true;
        return false;
    };
    if (role == ShowControlRole::FreezeHoldButton)
    {
        auto *button = qobject_cast<VCButton *>(control);
        if (button == nullptr || button->actionType() != VCButton::FreezeHold ||
            button->state() != VCButton::Active)
            return;
        if (stopping)
            for (int page = 0; vc != nullptr && page < vc->pagesCount(); ++page)
                for (VCWidget *widget : vc->page(page)->children(true))
                    if (auto *other = qobject_cast<VCButton *>(widget);
                        other != nullptr && other != button && other->actionType() == VCButton::FreezeHold &&
                        other->state() == VCButton::Active)
                        return;
        button->applyRecordedState(false, owner, true);
    }
    else if (role == ShowControlRole::FlashButton)
    {
        auto *button = qobject_cast<VCButton *>(control);
        if (button != nullptr && button->actionType() == VCButton::Flash &&
            button->state() == VCButton::Active && !user.flash.contains(button->id()) &&
            !flashHeld(button->functionID()))
            button->applyRecordedState(false, owner, true);
    }
    else if (auto *slider = qobject_cast<VCSlider *>(control))
    {
        const quint32 target = slider->sliderMode() == VCSlider::Adjust
            ? slider->controlledFunction() : ShowCommand::InvalidId;
        if (!user.sliderFlash.contains(slider->id()) && !flashHeld(target))
            slider->flashFunction(false);
    }
}

ShowControlAction::Holds ShowControlAction::stop(Doc *doc, VirtualConsole *vc, const Stop &state)
{
    const auto releaseHolds = [&](const QSet<QUuid> &holds, ShowControlRole role)
    {
        for (const QUuid &id : holds)
            for (VCWidget *control : vc != nullptr ? vc->widgetsByRecordingId(id) : QList<VCWidget *>())
                releaseHold(vc, control, role, state.owner, state.user, true);
    };
    releaseHolds(state.freeze, ShowControlRole::FreezeHoldButton);
    releaseHolds(state.flash, ShowControlRole::FlashButton);
    releaseHolds(state.sliderFlash, ShowControlRole::AdjustSlider);
    Holds retained;
    for (quint32 id : state.user.freeze)
        if (auto *button = vc != nullptr ? qobject_cast<VCButton *>(vc->widget(id)) : nullptr;
            button != nullptr && button->actionType() == VCButton::FreezeHold)
            retained.freeze.insert(id);
    for (quint32 id : state.user.flash)
        if (vc != nullptr && qobject_cast<VCButton *>(vc->widget(id)) != nullptr)
            retained.flash.insert(id);
    for (quint32 id : state.user.sliderFlash)
        if (vc != nullptr && qobject_cast<VCSlider *>(vc->widget(id)) != nullptr)
            retained.sliderFlash.insert(id);
    bool touchedFreeze = false, touchedBlackout = false;
    QSet<VCWidget *> released;
    QSet<QObject *> releasedMatrices;
    for (const Restoration &effect : state.effects)
    {
        VCWidget *control = effect.control;
        if (effect.after.role == ShowControlRole::AnimationFader)
        {
            QObject *matrix = effect.after.lifetimes.value(0);
            if (effect.releaseOwner && matrix != nullptr && !releasedMatrices.contains(matrix))
            {
                release(doc, control, effect.owner, &effect.after);
                releasedMatrices.insert(matrix);
            }
            continue;
        }
        if (control == nullptr)
            continue;
        auto *button = qobject_cast<VCButton *>(control);
        if (button != nullptr)
        {
            touchedFreeze |= button->actionType() == VCButton::Freeze;
            touchedBlackout |= button->actionType() == VCButton::Blackout;
        }
        if (!effect.releaseOwner || released.contains(control))
            continue;
        release(doc, control, effect.owner);
        if (button != nullptr && button->actionType() == VCButton::Toggle)
            if (Function *target = doc->function(button->functionID()); target != nullptr && target->isRunning())
                button->releaseToMonitoring();
        released.insert(control);
    }
    if (!state.freeze.isEmpty())
        doc->inputOutputMap()->setFrozenMomentary(!retained.freeze.isEmpty());
    if (touchedFreeze || state.freezeIntent)
        doc->inputOutputMap()->setFrozen(state.freezeIntent.value_or(false));
    if (touchedBlackout || state.blackoutIntent)
        doc->inputOutputMap()->setBlackout(state.blackoutIntent.value_or(false));
    return retained;
}

QVector<QMetaObject::Connection> ShowControlAction::observeConfiguration(
    VCWidget *control, QObject *receiver, const std::function<void()> &refresh)
{
    QVector<QMetaObject::Connection> connections;
    if (auto *button = qobject_cast<VCButton *>(control))
    {
        connections.append(QObject::connect(button, &VCButton::functionIDChanged, receiver, refresh));
        connections.append(QObject::connect(button, &VCButton::actionTypeChanged, receiver, refresh));
    }
    else if (auto *slider = qobject_cast<VCSlider *>(control))
    {
        connections.append(QObject::connect(slider, &VCSlider::sliderModeChanged, receiver, refresh));
        connections.append(QObject::connect(slider, &VCSlider::controlledFunctionChanged, receiver, refresh));
        connections.append(QObject::connect(slider, &VCSlider::controlledAttributeChanged, receiver, refresh));
        connections.append(QObject::connect(slider, &VCSlider::channelsCountChanged, receiver, refresh));
    }
    else if (auto *pad = qobject_cast<VCXYPad *>(control))
    {
        connections.append(QObject::connect(pad, &VCXYPad::floorControlChanged, receiver, refresh));
        connections.append(QObject::connect(pad, &VCXYPad::invertedAppearanceChanged, receiver, refresh));
        connections.append(QObject::connect(pad, &VCXYPad::floorSizeChanged, receiver, refresh));
        connections.append(QObject::connect(pad, &VCXYPad::fixtureListChanged, receiver, refresh));
        connections.append(QObject::connect(pad, &VCXYPad::presetsListChanged, receiver, refresh));
    }
    else if (auto *animation = qobject_cast<VCAnimation *>(control))
    {
        connections.append(QObject::connect(animation, &VCAnimation::functionIDChanged, receiver, refresh));
        connections.append(QObject::connect(animation, &VCAnimation::presetsListChanged, receiver, refresh));
    }
    return connections;
}

QVector<ShowControlAction::Receipt> ShowControlAction::restoreBatch(
        Doc *doc, VirtualConsole *vc, const QVector<Restoration> &targets,
        const QHash<VCWidget *, State> &latest, const QSet<quint32> &flashHolds,
        const QSet<quint32> &sliderFlashHolds)
{
    const auto changedElsewhere = [&](VCWidget *control)
    {
        const auto current = observe(control);
        for (const auto &target : targets)
            if (target.control == control &&
                (target.superseded || planRestore(target.before, target.after, current,
                                                 current.lifetimes == target.after.lifetimes,
                                                 current.choiceLifetime == target.after.choiceLifetime).superseded))
                return true;
        const auto prior = latest.value(control);
        return current.role == ShowControlRole::XYPad ? current.point != prior.point
                                                     : current.scalar != prior.scalar;
    };
    const auto reachedFrames = [&](VCButton *button)
    {
        QSet<quint32> frames;
        const auto native = coupling(doc, vc, button, functionId(button));
        if (native.soloGroup != ShowCommand::InvalidId)
            frames.insert(native.soloGroup);
        for (const auto &member : native.memberSoloGroups)
            frames.insert(member.second);
        return frames;
    };
    QSet<quint32> guardedFrames;
    for (const auto &target : targets)
    {
        auto *button = qobject_cast<VCButton *>(target.control);
        if (button == nullptr)
            continue;
        for (quint32 frameId : reachedFrames(button))
        {
            auto *frame = vc != nullptr ? qobject_cast<VCSoloFrame *>(vc->widget(frameId)) : nullptr;
            if (frame == nullptr)
                continue;
            for (VCWidget *child : frame->children(true))
                if (qobject_cast<VCButton *>(child) != nullptr && changedElsewhere(child))
                    guardedFrames.insert(frameId);
        }
    }
    QList<VCWidget *> controls;
    for (int page = 0; vc != nullptr && page < vc->pagesCount(); ++page)
        controls += vc->page(page)->children(true);
    const auto heldElsewhere = [&](VCButton *button, VCButton::ButtonAction action)
    {
        for (VCWidget *control : controls)
        {
            auto *other = qobject_cast<VCButton *>(control);
            if (other != nullptr && other != button && other->actionType() == action &&
                (action == VCButton::FreezeHold || other->functionID() == button->functionID()) &&
                (other->state() == VCButton::Active ||
                 (action == VCButton::Flash && flashHolds.contains(other->id()))))
                return true;
            if (auto *slider = qobject_cast<VCSlider *>(control); action == VCButton::Flash &&
                slider != nullptr && slider->sliderMode() == VCSlider::Adjust &&
                slider->controlledFunction() == button->functionID() && sliderFlashHolds.contains(slider->id()))
                return true;
        }
        return false;
    };
    QVector<Receipt> receipts;
    for (int pass = 0; pass < 3; ++pass)
        for (const auto &target : targets)
        {
            VCWidget *control = target.control;
            if (control == nullptr)
                continue;
            auto *button = qobject_cast<VCButton *>(control);
            if ((button != nullptr && scalarState(control) == target.before.scalar) || changedElsewhere(control))
            {
                if (pass == 1 && target.releaseOwner)
                    release(doc, control, target.owner);
                continue;
            }
            if (button == nullptr && pass == 1)
            {
                auto receipt = restore(doc, control, target.before, target.after, target.owner);
                if (auto *slider = qobject_cast<VCSlider *>(control); slider != nullptr &&
                    receipt.writerGeneration == 0 && slider->sliderMode() == VCSlider::Adjust &&
                    slider->controlledAttribute() == Function::Intensity &&
                    doc->function(slider->controlledFunction()) != nullptr)
                    receipt.functions.append({slider->controlledFunction(), true, ShowCommand::InvalidId});
                receipts.append(receipt);
                if (target.releaseOwner)
                    release(doc, control, target.owner);
            }
            else if (button != nullptr && pass == (target.before.scalar != 0 ? 2 : 0))
            {
                if (target.before.scalar != 0 && reachedFrames(button).intersects(guardedFrames))
                    continue;
                if (target.before.scalar == 0 &&
                    ((button->actionType() == VCButton::FreezeHold && heldElsewhere(button, VCButton::FreezeHold)) ||
                     (button->actionType() == VCButton::Flash && heldElsewhere(button, VCButton::Flash))))
                    continue;
                if (target.before.scalar == 0 && heldElsewhere(button, VCButton::Toggle) &&
                    button->releaseToMonitoring())
                    continue;
                receipts.append(restore(doc, control, target.before, target.after, target.owner));
            }
        }
    return receipts;
}
