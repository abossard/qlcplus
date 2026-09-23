/*
  Q Light Controller Plus
  vcbutton.cpp

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

#include "chaseraction.h"
#include "qlcmacros.h"
#include "vcbutton.h"
#include "chaser.h"
#include "showcommandrecorder.h"
#include "tardis.h"
#include "doc.h"

/** ************** XML Tags and Attributes ************** */

#define KXMLQLCVCButtonFunction     QStringLiteral("Function")
#define KXMLQLCVCButtonFunctionID   QStringLiteral("ID")

#define KXMLQLCVCButtonAction           QStringLiteral("Action")
#define KXMLQLCVCButtonActionFlash      QStringLiteral("Flash")
#define KXMLQLCVCButtonActionToggle     QStringLiteral("Toggle")
#define KXMLQLCVCButtonActionBlackout   QStringLiteral("Blackout")
#define KXMLQLCVCButtonActionStopAll    QStringLiteral("StopAll")
#define KXMLQLCVCButtonActionFreeze     QStringLiteral("Freeze")
#define KXMLQLCVCButtonActionFreezeHold QStringLiteral("FreezeHold")

#define KXMLQLCVCButtonFlashOverride    QStringLiteral("Override")
#define KXMLQLCVCButtonFlashForceLTP    QStringLiteral("ForceLTP")

#define KXMLQLCVCButtonStopAllFadeTime  QStringLiteral("FadeOut")

#define KXMLQLCVCButtonIntensity        QStringLiteral("Intensity")
#define KXMLQLCVCButtonIntensityAdjust  QStringLiteral("Adjust")

/** **************** External Control IDs ***************** */

#define INPUT_PRESSURE_ID   0

VCButton::VCButton(Doc *doc, QObject *parent)
    : VCWidget(doc, parent)
    , m_functionID(Function::invalidId())
    , m_flashOverrides(false)
    , m_flashForceLTP(false)
    , m_state(Inactive)
    , m_actionType(Toggle)
    , m_stopAllFadeOutTime(0)
    , m_startupIntensityEnabled(false)
    , m_startupIntensity(1.0)
    , m_inputPressed(false)
    , m_holdsMomentaryFreeze(false)
{
    setType(VCWidget::ButtonWidget);

    if (m_doc != nullptr)
    {
        connect(m_doc->inputOutputMap(), &InputOutputMap::frozenLatchChanged,
                this, &VCButton::slotFrozenLatchChanged);
        connect(m_doc->inputOutputMap(), &InputOutputMap::frozenMomentaryChanged,
                this, &VCButton::slotFrozenMomentaryChanged);
    }

    registerExternalControl(INPUT_PRESSURE_ID, tr("Pressure"), true);
}

VCButton::~VCButton()
{
    releaseHeldActivation();

    if (m_item)
        delete m_item;
}

QString VCButton::defaultCaption() const
{
    return tr("Button %1").arg(id() + 1);
}

void VCButton::setupLookAndFeel(qreal pixelDensity, int page)
{
    VCWidget::setupLookAndFeel(pixelDensity, page);

    setBackgroundColor(QColor("#444"));
}

void VCButton::render(QQuickView *view, QQuickItem *parent)
{
    if (view == nullptr || parent == nullptr)
        return;

    QQmlComponent *component = new QQmlComponent(view->engine(), QUrl("qrc:/VCButtonItem.qml"));

    if (component->isError())
    {
        qDebug() << component->errors();
        delete component;
        return;
    }

    m_item = qobject_cast<QQuickItem*>(component->create());
    if (m_item == nullptr)
        qWarning() << Q_FUNC_INFO << "Unable to create button component" << component->errors();
    delete component;
    if (m_item == nullptr)
        return;

    m_item->setParentItem(parent);
    m_item->setProperty("buttonObj", QVariant::fromValue(this));
}

QString VCButton::propertiesResource() const
{
    return QString("qrc:/VCButtonProperties.qml");
}

VCWidget *VCButton::createCopy(VCWidget *parent) const
{
    Q_ASSERT(parent != nullptr);

    VCButton *button = new VCButton(m_doc, parent);
    if (button->copyFrom(this) == false)
    {
        delete button;
        button = nullptr;
    }

    return button;
}

bool VCButton::copyFrom(const VCWidget* widget)
{
    const VCButton *button = qobject_cast <const VCButton*> (widget);
    if (button == nullptr)
        return false;

    /* Copy button-specific stuff */
    //setIconPath(button->iconPath()); // TODO ?
    setFunctionID(button->functionID());
    setStartupIntensityEnabled(button->startupIntensityEnabled());
    setStartupIntensity(button->startupIntensity());
    setStopAllFadeOutTime(button->stopAllFadeOutTime());
    setActionType(button->actionType());
    setState(button->state());

    setFlashForceLTP(button->flashForceLTP());
    setFlashOverride(button->flashOverrides());

    /* Copy common stuff */
    return VCWidget::copyFrom(widget);
}

/*********************************************************************
 * Function attachment
 *********************************************************************/

void VCButton::setFunctionID(quint32 fid)
{
    bool running = false;

    if (m_functionID == fid)
        return;

    Function *current = m_doc->function(m_functionID);
    Function *function = m_doc->function(fid);

    if (current != nullptr)
    {
        /* Get rid of old function connections */
        disconnect(current, SIGNAL(running(quint32)),
                this, SLOT(slotFunctionRunning(quint32)));
        disconnect(current, SIGNAL(stopped(quint32)),
                this, SLOT(slotFunctionStopped(quint32)));
        disconnect(current, SIGNAL(flashing(quint32,bool)),
                this, SLOT(slotFunctionFlashing(quint32,bool)));

        if (current->isRunning())
        {
            running = true;
            current->stop(functionParent());
            resetIntensityOverrideAttribute();
        }

    }

    if (function != nullptr)
    {
        /* Connect to the new function */
        connect(function, SIGNAL(running(quint32)),
                this, SLOT(slotFunctionRunning(quint32)));
        connect(function, SIGNAL(stopped(quint32)),
                this, SLOT(slotFunctionStopped(quint32)));
        connect(function, SIGNAL(flashing(quint32,bool)),
                this, SLOT(slotFunctionFlashing(quint32,bool)));

        m_functionID = fid;
        if ((isEditing() && caption().isEmpty()) || caption() == defaultCaption())
            setCaption(function->name());

        if (running)
        {
            function->start(m_doc->masterTimer(), functionParent());
            setState(Active);
        }
        emit functionIDChanged(fid);
    }
    else
    {
        /* No function attachment */
        m_functionID = Function::invalidId();
        emit functionIDChanged(-1);
    }

    Tardis::instance()->enqueueAction(Tardis::VCButtonSetFunctionID, id(),
                                      current ? current->id() : Function::invalidId(),
                                      function ? function->id() : Function::invalidId());
}

quint32 VCButton::functionID() const
{
    return m_functionID;
}

void VCButton::adjustFunctionIntensity(Function *f, qreal value)
{
    qreal finalValue = startupIntensityEnabled() ? startupIntensity() * value : value;

    VCWidget::adjustFunctionIntensity(f, finalValue);
}

void VCButton::adjustIntensity(qreal val)
{
    VCWidget::adjustIntensity(val);

    if (state() == Active)
    {
        Function *f = m_doc->function(m_functionID);
        if (f == nullptr)
            return;

        adjustFunctionIntensity(f, val);
    }
}

void VCButton::notifyFunctionStarting(VCWidget *widget, quint32 fid, qreal fIntensity, bool excludeMonitored)
{
    Q_UNUSED(widget)
    Q_UNUSED(fIntensity)

    qDebug() << "notifyFunctionStarting" << widget->caption() << fid << fIntensity;

    if (fid == m_functionID || m_functionID == Function::invalidId())
        return;

    if (excludeMonitored)
    {
        // stop the controlled Function only if actively started
        // by this Button or if monitoring the startup Function
        if (state() != Active && m_functionID != m_doc->startupFunction())
            return;
    }

    if (actionType() == VCButton::Toggle)
    {
        Function *f = m_doc->function(m_functionID);
        if (f != NULL)
        {
            f->stop(functionParent());
            resetIntensityOverrideAttribute();
        }
    }
}

void VCButton::slotFunctionRunning(quint32 fid)
{
    if (fid == m_functionID && actionType() == Toggle)
    {
        if (state() == Inactive)
            setState(Monitoring);
        emit functionStarting(this, m_functionID);
    }
}

void VCButton::slotFunctionStopped(quint32 fid)
{
    if (fid == m_functionID && actionType() == Toggle)
    {
        resetIntensityOverrideAttribute();
        setState(Inactive);
    }
}

void VCButton::slotFunctionFlashing(quint32 fid, bool state)
{
    // Do not change the state of the button for Blackout or Stop All Functions buttons
    if (actionType() != Toggle && actionType() != Flash)
        return;

    if (fid != m_functionID)
        return;

    // if the function was flashed by another button, and the function is still running, keep the button pushed
    Function* f = m_doc->function(m_functionID);
    if (state == false && actionType() == Toggle && f != nullptr && f->isRunning())
    {
        return;
    }

    setState(state ? Active : Inactive);
}

FunctionParent VCButton::functionParent() const
{
    return FunctionParent(FunctionParent::ManualVCWidget, id());
}

/*****************************************************************************
 * Flash Properties
 *****************************************************************************/

bool VCButton::flashOverrides() const
{
    return m_flashOverrides;
}

void VCButton::setFlashOverride(bool shouldOverride)
{
    if (m_flashOverrides == shouldOverride)
        return;

    m_flashOverrides = shouldOverride;
    emit flashOverrideChanged(shouldOverride);
}

bool VCButton::flashForceLTP() const
{
    return m_flashForceLTP;
}

void VCButton::setFlashForceLTP(bool forceLTP)
{
    if (m_flashForceLTP == forceLTP)
        return;

    m_flashForceLTP = forceLTP;
    emit flashForceLTPChanged(forceLTP);
}

/*********************************************************************
 * Button state
 *********************************************************************/

VCButton::ButtonState VCButton::state() const
{
    return m_state;
}

void VCButton::setState(ButtonState state)
{
    if (state == m_state)
        return;

    m_state = state;

    emit stateChanged(m_state);

    updateFeedback();
}

void VCButton::slotFrozenLatchChanged(bool latched)
{
    if (actionType() != Freeze)
        return;

    setState(latched ? Active : Inactive);
}

void VCButton::slotFrozenMomentaryChanged(bool held)
{
    if (actionType() != FreezeHold)
        return;

    setState(held ? Active : Inactive);
}

void VCButton::releaseHeldActivation()
{
    // A held control that goes away never receives its release event, so it
    // must drop its own activation. Only this button's own hold counts: the
    // visible state mirrors the shared component, so an unpressed button would
    // otherwise release a hold belonging to another control.
    if (actionType() == FreezeHold && m_holdsMomentaryFreeze)
        requestStateChange(false);
}

void VCButton::setDisabled(bool disable)
{
    if (disable)
        releaseHeldActivation();

    VCWidget::setDisabled(disable);
}

void VCButton::setVisible(bool isVisible)
{
    // A Flash button relies on a release event to unflash. When it gets hidden
    // (e.g. its multipage frame switches page) while active, that release never
    // comes, leaving the button stuck on and the function flashed. Release it.
    if (isVisible == false && actionType() == Flash && state() == Active)
        requestStateChange(false);

    if (isVisible == false)
        releaseHeldActivation();

    VCWidget::setVisible(isVisible);
}

void VCButton::requestUserStateChange(bool pressed)
{
    // The QML pointer and touch handlers are the only callers of this overload.
    requestUserStateChange(pressed, ShowCommandOrigin::Pointer);
}

/** Only a person operating a control authors anything, and only their input may
 *  change how a control resolves while recording. */
static bool isUserOrigin(ShowCommandOrigin origin)
{
    return origin == ShowCommandOrigin::Pointer ||
           origin == ShowCommandOrigin::Midi ||
           origin == ShowCommandOrigin::Keyboard;
}

void VCButton::requestUserStateChange(bool pressed, ShowCommandOrigin origin)
{
    ShowCommandRecorder *recorder = ShowCommandRecorder::instance();
    const bool authoring = recorder != nullptr && recorder->isAuthoring() && isUserOrigin(origin);
    Function *f = m_doc->function(m_functionID);

    if (authoring == false || actionType() != Toggle || f == nullptr)
    {
        requestStateChange(pressed);
        if (authoring && actionType() != Toggle)
            recorder->reportUnsupported(tr("%1 buttons").arg(actionToString(actionType())));
        return;
    }

    if (hasSoloParent())
    {
        // Starting inside a Solo Frame also stops its siblings. Recording the
        // start alone would replay something the operator never saw.
        requestStateChange(pressed);
        recorder->reportUnsupported(tr("buttons inside a Solo Frame"));
        return;
    }

    // Resolve first, then apply exactly that. A button monitoring a Function the
    // Show itself started reads as "on" to the operator, so their first click is
    // a stop; requestStateChange() would restart it instead.
    const bool stopIntent = state() != Inactive;
    if (stopIntent && state() == Monitoring)
    {
        f->stop(functionParent());
        resetIntensityOverrideAttribute();
        setState(Inactive);
        Tardis::instance()->enqueueAction(Tardis::VCButtonSetPressed, id(), false, pressed);
    }
    else
    {
        requestStateChange(pressed);
    }

    QVector<ShowCommandInput> resolved;
    ShowCommandInput command;
    command.origin = origin;
    command.functionId = m_functionID;
    command.action = stopIntent ? ShowCommandAction::Stop : ShowCommandAction::Start;
    resolved.append(command);

    // A button that starts its Function at a reduced intensity needs that value
    // recorded as its own ordered step: Start carries no value.
    if (stopIntent == false)
    {
        const qreal startIntensity = startupIntensityEnabled() ? startupIntensity() * intensity()
                                                               : intensity();
        if (qFuzzyCompare(startIntensity, qreal(1.0)) == false)
        {
            ShowCommandInput value;
            value.origin = origin;
            value.functionId = m_functionID;
            value.action = ShowCommandAction::SetIntensity;
            value.intensity = CLAMP(startIntensity, qreal(0), qreal(1));
            resolved.append(value);
        }
    }

    recorder->submitUserInput(resolved);
}

void VCButton::requestStateChange(bool pressed)
{
    qDebug() << "Requested button state" << pressed;

    switch(actionType())
    {
        case Toggle:
        {
            Function *f = m_doc->function(m_functionID);
            if (f == NULL)
                return;

            // if the button is in a SoloFrame and the function is running but was
            // started by a different function (a chaser or collection), turn other
            // functions off and start this one.
            if (state() == Active && !(hasSoloParent() && f->startedAsChild()))
            {
                f->stop(functionParent());
                resetIntensityOverrideAttribute();
                setState(Inactive);
            }
            else
            {
                adjustFunctionIntensity(f, intensity());

                // starting a Chaser is a special case, since it is necessary
                // to use Chaser Actions to properly start the first
                // Chaser step with the right intensity
                if (f->type() == Function::ChaserType || f->type() == Function::SequenceType)
                {
                    ChaserAction action;
                    action.m_action = ChaserSetStepIndex;
                    action.m_stepIndex = 0;
                    action.m_masterIntensity = intensity();
                    action.m_stepIntensity = 1.0;
                    action.m_fadeMode = Chaser::FromFunction;

                    Chaser *chaser = qobject_cast<Chaser*>(f);
                    chaser->setAction(action);
                }

                f->start(m_doc->masterTimer(), functionParent());
                setState(Active);
                emit functionStarting(this, m_functionID);
            }
        }
        break;
        case Flash:
        {
            Function *f = m_doc->function(m_functionID);
            if (f != nullptr)
            {
                if (state() == Inactive && pressed == true)
                {
                    f->flash(m_doc->masterTimer(), flashOverrides(), flashForceLTP());
                    setState(Active);
                }
                else if (state() == Active && pressed == false)
                {
                    f->unFlash(m_doc->masterTimer());
                    setState(Inactive);
                }
            }
        }
        break;
        case Blackout:
        {
            m_doc->inputOutputMap()->toggleBlackout();
            setState(pressed ? Active : Inactive);
        }
        break;
        case StopAll:
        {
            if (stopAllFadeOutTime() == 0)
                m_doc->masterTimer()->stopAllFunctions();
            else
                m_doc->masterTimer()->fadeAndStopAll(stopAllFadeOutTime());
        }
        break;
        case Freeze:
        {
            // Desired-state command: WebAccess, audio triggers and Tardis all
            // pass the latch state they want. The physical pointer, touch, key
            // and controller adapters invert the latch on the press edge and
            // never call this on release.
            m_doc->inputOutputMap()->setFrozen(pressed);
        }
        break;
        case FreezeHold:
        {
            // One shared momentary flag, Flash-style: any release clears it
            // even while another control is held, and it never touches the latch.
            m_holdsMomentaryFreeze = pressed;
            m_doc->inputOutputMap()->setFrozenMomentary(pressed);
        }
        break;
        default:
        break;
    }

    Tardis::instance()->enqueueAction(Tardis::VCButtonSetPressed, id(), false, pressed);
}


/*********************************************************************
 * Button action
 *********************************************************************/

VCButton::ButtonAction VCButton::actionType() const
{
    return m_actionType;
}

void VCButton::setActionType(ButtonAction actionType)
{
    if (m_actionType == actionType)
        return;

    Tardis::instance()->enqueueAction(Tardis::VCButtonSetActionType, id(), m_actionType, actionType);

    // Reconfiguring must not strand this button's hold, and the next action
    // must start from a disarmed press edge.
    releaseHeldActivation();
    m_inputPressed = false;

    ButtonAction previous = m_actionType;
    m_actionType = actionType;
    emit actionTypeChanged(actionType);

    // Configuring, copying or loading a Freeze button displays its own mode's
    // component. It must never change either component.
    if (m_doc == nullptr)
        return;

    if (m_actionType == Freeze)
        setState(m_doc->inputOutputMap()->frozenLatch() ? Active : Inactive);
    else if (m_actionType == FreezeHold)
        setState(m_doc->inputOutputMap()->frozenMomentary() ? Active : Inactive);
    else if (previous == Freeze || previous == FreezeHold)
        setState(Inactive);
}

QString VCButton::actionToString(VCButton::ButtonAction action)
{
    if (action == Flash)
        return QString(KXMLQLCVCButtonActionFlash);
    else if (action == Blackout)
        return QString(KXMLQLCVCButtonActionBlackout);
    else if (action == StopAll)
        return QString(KXMLQLCVCButtonActionStopAll);
    else if (action == Freeze)
        return QString(KXMLQLCVCButtonActionFreeze);
    else if (action == FreezeHold)
        return QString(KXMLQLCVCButtonActionFreezeHold);
    else
        return QString(KXMLQLCVCButtonActionToggle);
}

VCButton::ButtonAction VCButton::stringToAction(const QString& str)
{
    if (str == KXMLQLCVCButtonActionFlash)
        return Flash;
    else if (str == KXMLQLCVCButtonActionBlackout)
        return Blackout;
    else if (str == KXMLQLCVCButtonActionStopAll)
        return StopAll;
    else if (str == KXMLQLCVCButtonActionFreeze)
        return Freeze;
    else if (str == KXMLQLCVCButtonActionFreezeHold)
        return FreezeHold;
    else
        return Toggle;
}

void VCButton::setStopAllFadeOutTime(int ms)
{
    if (ms == m_stopAllFadeOutTime)
        return;

    m_stopAllFadeOutTime = ms;
    emit stopAllFadeOutTimeChanged();
}

int VCButton::stopAllFadeOutTime() const
{
    return m_stopAllFadeOutTime;
}


/*****************************************************************************
 * Function startup intensity adjustment
 *****************************************************************************/

bool VCButton::startupIntensityEnabled() const
{
    return m_startupIntensityEnabled;
}

void VCButton::setStartupIntensityEnabled(bool enable)
{
    if (enable == m_startupIntensityEnabled)
        return;

    Tardis::instance()->enqueueAction(Tardis::VCButtonEnableStartupIntensity, id(), m_startupIntensityEnabled, enable);

    m_startupIntensityEnabled = enable;
    emit startupIntensityEnabledChanged();
}

qreal VCButton::startupIntensity() const
{
    return m_startupIntensity;
}

void VCButton::setStartupIntensity(qreal fraction)
{
    if (fraction == m_startupIntensity)
        return;

    qreal newVal = CLAMP(fraction, qreal(0), qreal(1));
    Tardis::instance()->enqueueAction(Tardis::VCButtonSetStartupIntensity, id(), m_startupIntensity, newVal);

    m_startupIntensity = newVal;
    emit startupIntensityChanged();
}

/*********************************************************************
 * External input
 *********************************************************************/

void VCButton::updateFeedback()
{
    if (m_state == Inactive)
        sendFeedback(0, INPUT_PRESSURE_ID, VCWidget::LowerValue);
    else if (m_state == Monitoring)
        sendFeedback(0, INPUT_PRESSURE_ID, VCWidget::MonitorValue);
    else
        sendFeedback(UCHAR_MAX, INPUT_PRESSURE_ID, VCWidget::UpperValue);
}

void VCButton::slotInputValueChanged(quint8 id, uchar value)
{
    if (id != INPUT_PRESSURE_ID)
        return;

    if (actionType() == Flash || actionType() == FreezeHold)
    {
        // Hold while pressed. The state guard makes duplicate non-zero pressure
        // idempotent, and zero pressure releases.
        if (state() == Inactive && value > 0)
            requestUserStateChange(true, inputOrigin());
        else if (state() == Active && value == 0)
            requestUserStateChange(false, inputOrigin());
    }
    else if (actionType() == Freeze)
    {
        // Invert the latch on the press edge only: key auto-repeat and duplicate
        // non-zero pressure must not flip it twice. A release re-arms the edge
        // without touching the latch.
        bool pressed = value > 0;
        if (pressed == m_inputPressed)
            return;

        m_inputPressed = pressed;
        if (pressed)
            requestUserStateChange(state() != Active, inputOrigin());
    }
    else
    {
        // A monitored button looks lit to the operator, so a press has to reach
        // the resolver like any other. The old Inactive/Active guards swallowed
        // it and left the control dead.
        if (value > 0)
            requestUserStateChange(state() == Inactive, inputOrigin());
    }
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool VCButton::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCVCButton)
    {
        qWarning() << Q_FUNC_INFO << "Button node not found";
        return false;
    }

    /* Widget commons */
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
        else if (root.name() == KXMLQLCVCButtonFunction)
        {
            QString str = root.attributes().value(KXMLQLCVCButtonFunctionID).toString();
            setFunctionID(str.toUInt());
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCVCButtonAction)
        {
            QXmlStreamAttributes attrs = root.attributes();
            if (attrs.hasAttribute(KXMLQLCVCButtonStopAllFadeTime))
                setStopAllFadeOutTime(attrs.value(KXMLQLCVCButtonStopAllFadeTime).toInt());

            if (attrs.hasAttribute(KXMLQLCVCButtonFlashOverride))
                    setFlashOverride(attrs.value(KXMLQLCVCButtonFlashOverride).toInt());

            if (attrs.hasAttribute(KXMLQLCVCButtonFlashForceLTP))
                    setFlashForceLTP(attrs.value(KXMLQLCVCButtonFlashForceLTP).toInt());

            setActionType(stringToAction(root.readElementText()));
        }
        else if (root.name() == KXMLQLCVCButtonIntensity)
        {
            bool adjust;
            if (root.attributes().value(KXMLQLCVCButtonIntensityAdjust).toString() == KXMLQLCTrue)
                adjust = true;
            else
                adjust = false;
            setStartupIntensity(qreal(root.readElementText().toInt()) / qreal(100));
            setStartupIntensityEnabled(adjust);
        }
        else if (root.name() == KXMLQLCVCWidgetInput)
        {
            loadXMLInputSource(root, INPUT_PRESSURE_ID);
        }
        else if (root.name() == KXMLQLCVCWidgetKey)
        {
            loadXMLInputKey(root, INPUT_PRESSURE_ID);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown button tag:" << root.name().toString();
            root.skipCurrentElement();
        }
    }

    return true;
}

bool VCButton::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);

    /* VC button entry */
    doc->writeStartElement(KXMLQLCVCButton);

    saveXMLCommon(doc);

    /* Window state */
    saveXMLWindowState(doc);

    /* Appearance */
    saveXMLAppearance(doc);

    /* Function */
    doc->writeStartElement(KXMLQLCVCButtonFunction);
    doc->writeAttribute(KXMLQLCVCButtonFunctionID, QString::number(functionID()));
    doc->writeEndElement();

    /* Action */
    doc->writeStartElement(KXMLQLCVCButtonAction);

    if (actionType() == StopAll && stopAllFadeOutTime() != 0)
    {
        doc->writeAttribute(KXMLQLCVCButtonStopAllFadeTime, QString::number(stopAllFadeOutTime()));
    }
    else if (actionType() == Flash)
    {
        doc->writeAttribute(KXMLQLCVCButtonFlashOverride, QString::number(flashOverrides()));
        doc->writeAttribute(KXMLQLCVCButtonFlashForceLTP, QString::number(flashForceLTP()));
    }

    doc->writeCharacters(actionToString(actionType()));
    doc->writeEndElement();

    /* External control */
    saveXMLInputControl(doc, INPUT_PRESSURE_ID, false);

    /* Intensity adjustment */
    if (startupIntensityEnabled())
    {
        doc->writeStartElement(KXMLQLCVCButtonIntensity);
        doc->writeCharacters(QString::number(int(startupIntensity() * 100)));
        doc->writeEndElement();
    }

    /* End the <Button> tag */
    doc->writeEndElement();

    return true;
}
