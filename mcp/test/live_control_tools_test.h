#ifndef LIVE_CONTROL_TOOLS_TEST_H
#define LIVE_CONTROL_TOOLS_TEST_H

#include <QObject>
#include <QTemporaryDir>
#include <memory>

class App;
class VCBridgeV5;
class WorkspaceBridgeV5;

class McpLiveControlTools_Test : public QObject
{
    Q_OBJECT
public:
    McpLiveControlTools_Test();
    ~McpLiveControlTools_Test() override;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void performMode_data();
    void performMode();
    void recording_data();
    void recording();
    void buttonInput_data();
    void buttonInput();
    void quitApp_data();
    void quitApp();
    void djTelemetry_data();
    void djTelemetry();
    void drivenShows_data();
    void drivenShows();
    void invalidArguments_data();
    void invalidArguments();
    void buttonRefusals_data();
    void buttonRefusals();
    void unavailableContext_data();
    void unavailableContext();
    void failedRecordingFinalization();
    void registration_data();
    void registration();
    void freezeHoldRelease_data();
    void freezeHoldRelease();
    void serverSurface_data();
    void serverSurface();
    void latchedToggle_data();
    void latchedToggle();

private:
    QTemporaryDir m_settings;
    QByteArray m_previousHome;
    std::unique_ptr<App> m_app;
    std::unique_ptr<VCBridgeV5> m_vc;
    std::unique_ptr<WorkspaceBridgeV5> m_workspace;
};

#endif
