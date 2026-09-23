/*
  Q Light Controller Plus
  vcbutton.h

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

#ifndef VCBUTTON_H
#define VCBUTTON_H

#include "vcwidget.h"

#define KXMLQLCVCButton QStringLiteral("Button")

class FunctionParent;

class VCButton : public VCWidget
{
    Q_OBJECT

    Q_PROPERTY(ButtonAction actionType READ actionType WRITE setActionType NOTIFY actionTypeChanged)
    Q_PROPERTY(ButtonState state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(quint32 functionID READ functionID WRITE setFunctionID NOTIFY functionIDChanged)
    Q_PROPERTY(bool startupIntensityEnabled READ startupIntensityEnabled WRITE setStartupIntensityEnabled NOTIFY startupIntensityEnabledChanged)
    Q_PROPERTY(qreal startupIntensity READ startupIntensity WRITE setStartupIntensity NOTIFY startupIntensityChanged)
    Q_PROPERTY(int stopAllFadeOutTime READ stopAllFadeOutTime WRITE setStopAllFadeOutTime NOTIFY stopAllFadeOutTimeChanged)
    Q_PROPERTY(bool flashOverrides READ flashOverrides WRITE setFlashOverride NOTIFY flashOverrideChanged)
    Q_PROPERTY(bool flashForceLTP READ flashForceLTP WRITE setFlashForceLTP NOTIFY flashForceLTPChanged)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    VCButton(Doc* doc = nullptr, QObject *parent = nullptr);
    virtual ~VCButton();

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

protected:
    /** @reimp */
    bool copyFrom(const VCWidget* widget) override;

    /*********************************************************************
     * Function attachment
     *********************************************************************/
public:
    /**
     * Attach a function to a VCButton. This function is started when the
     * button is pressed down.
     *
     * @param function An ID of a function to attach
     */
    Q_INVOKABLE void setFunctionID(quint32 fid);

    /**
     * Get the ID of the function attached to a VCButton
     *
     * @return The ID of the attached function or Function::invalidId()
     *         if there isn't one
     */
    quint32 functionID() const;

    /** @reimp */
    void adjustFunctionIntensity(Function *f, qreal value) override;

    /** @reimp */
    void adjustIntensity(qreal val) override;

    /**
     *  The actual method used to request a change of state of this
     *  Button. Depending on the action type this will start/stop
     *  the attached Function, if any */
    Q_INVOKABLE void requestStateChange(bool pressed);

    /**
     *  Real user input: a pointer/touch gesture on this button, a mapped
     *  external controller event or a key sequence. Resolves what the user
     *  asked for, applies it live exactly once and offers the resolved command
     *  to the recorder.
     *
     *  The provenance comes from the ingress that delivered the event, never
     *  from the shared input slot, which scripts, synthetic sources and audio
     *  reach as well. requestStateChange() stays the programmatic entry point
     *  and never authors anything. */
    Q_INVOKABLE void requestUserStateChange(bool pressed);

    /** Same, for an ingress that knows where the event came from. Nothing
     *  upgrades an unknown origin: a route that cannot say is not the user. */
    void requestUserStateChange(bool pressed, ShowCommandOrigin origin);

    /** @reimp */
    void notifyFunctionStarting(VCWidget *widget, quint32 fid, qreal fIntensity, bool excludeMonitored) override;

    /** @reimp – when a Flash button is hidden (e.g. a multipage frame switches
     *  page) while pressed, its release event never arrives, so release the
     *  flash here to avoid the button staying stuck on. A held while-pressed
     *  Freeze button drops its activation for the same reason. */
    void setVisible(bool isVisible) override;

    /** @reimp – a disabled control stops receiving input, so a held
     *  while-pressed Freeze activation must be dropped. */
    void setDisabled(bool disable) override;

private:
    /** Drop this button's while-pressed Freeze activation, if it holds one.
     *  Used wherever the control can stop receiving its release event. */
    void releaseHeldActivation();

public:

signals:
    void functionIDChanged(quint32 id);

protected slots:
    /** Handler for function running signal */
    void slotFunctionRunning(quint32 fid);

    /** Handler for function stop signal */
    void slotFunctionStopped(quint32 fid);

    /** Basically the same as slotFunctionStopped() but for flash signal */
    void slotFunctionFlashing(quint32 fid, bool state);

    /** Handler for the persistent workspace-global freeze latch. Latched Freeze
     *  buttons display the latch, never the aggregate freeze state. */
    void slotFrozenLatchChanged(bool latched);

    /** Handler for the shared momentary freeze flag. While-pressed Freeze
     *  buttons all mirror it, as every Flash button mirrors Function::flashing. */
    void slotFrozenMomentaryChanged(bool held);

private:
    FunctionParent functionParent() const;

protected:
    /** The ID of the Function that this button is controlling */
    quint32 m_functionID;

    /*****************************************************************************
    * Flash Properties
    *****************************************************************************/
public:
    /** Gets if flashing overrides newer values */
    bool flashOverrides() const;
    /** Sets if flashing should override values */
    void setFlashOverride(bool shouldOverride);
    /** Gets if flash channels should behave like LTP channels */
    bool flashForceLTP() const;
    /** Sets if the flash channels should behave like LTP channels */
    void setFlashForceLTP(bool forceLTP);

private:
    bool m_flashOverrides;
    bool m_flashForceLTP;

signals:
    void flashOverrideChanged(bool shouldOverride);
    void flashForceLTPChanged(bool forceLTP);

    /*********************************************************************
     * Button state
     *********************************************************************/
public:
    enum ButtonState
    {
        Inactive,
        Monitoring,
        Active
    };
    Q_ENUM(ButtonState)

    /** Get/Set the button pressure state */
    ButtonState state() const;
    void setState(ButtonState state);

signals:
    /** Signal emitted when the button has actually changed the graphic state */
    void stateChanged(int state);

protected:
    ButtonState m_state;

    /*********************************************************************
     * Button action
     *********************************************************************/
public:
    /**
     * Toggle: Start/stop the assigned function.
     * Flash: Keep the function running as long as the button is kept down.
     * Blackout: Toggle blackout on/off.
     * StopAll: Stop all functions (panic button).
     * Freeze: Invert the persistent workspace-global freeze latch.
     * FreezeHold: Hold the shared momentary freeze flag while pressed.
     *
     * Append new actions at the end: the numeric values are persisted in
     * Tardis undo records and in saved workspaces.
     */
    enum ButtonAction { Toggle, Flash, Blackout, StopAll, Freeze, FreezeHold };
    Q_ENUM(ButtonAction)

    ButtonAction actionType() const;

    void setActionType(ButtonAction actionType);

    static QString actionToString(ButtonAction action);
    static ButtonAction stringToAction(const QString& str);

    void setStopAllFadeOutTime(int ms);
    int stopAllFadeOutTime() const;

signals:
    void actionTypeChanged(ButtonAction actionType);
    void stopAllFadeOutTimeChanged();

protected:
    ButtonAction m_actionType;
    /** if button action is StopAll, this indicates the time
     *  in milliseconds of fadeout before stopping */
    int m_stopAllFadeOutTime;

    /*****************************************************************************
     * Function startup intensity adjustment
     *****************************************************************************/
public:
    /** Get/Set if a startup intensity amount should be applied
     *  when starting the attached Function */
    bool startupIntensityEnabled() const;
    void setStartupIntensityEnabled(bool enable);

    /** Get/Set the amount of intensity adjustment applied
     *  when starting the attached Function */
    qreal startupIntensity() const;
    void setStartupIntensity(qreal fraction);

signals:
    void startupIntensityEnabledChanged();
    void startupIntensityChanged();

protected:
    bool m_startupIntensityEnabled;
    qreal m_startupIntensity;

    /*********************************************************************
     * External input
     *********************************************************************/
public:
    /** @reimp */
    void updateFeedback() override;

public slots:
    /** @reimp */
    void slotInputValueChanged(quint8 id, uchar value) override;

private:
    /** Last pressure state seen on the external control, so that key auto-repeat
     *  and duplicate non-zero pressure toggle Freeze only on the press edge. */
    bool m_inputPressed;

    /** True while this specific button holds the shared momentary Freeze flag.
     *  The visible state mirrors the shared component, so it cannot tell whether
     *  this button is the one holding it. */
    bool m_holdsMomentaryFreeze;

    /*********************************************************************
     * Load & Save
     *********************************************************************/

public:
    /** @reimp */
    bool loadXML(QXmlStreamReader &root) override;

    /** @reimp */
    bool saveXML(QXmlStreamWriter *doc) const override;
};

#endif
