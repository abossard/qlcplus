#ifndef MASTERTIMER_UNIX_TEST_H
#define MASTERTIMER_UNIX_TEST_H

#include <QObject>

class MasterTimerUnix_Test : public QObject
{
    Q_OBJECT

private slots:
    void diagnostics_data();
    void diagnostics();
    void healthMessages_data();
    void healthMessages();
    void schedulingClockFailure_data();
    void schedulingClockFailure();

    /* Runtime control while the production loop is already running (C0-2/C0-8). */
    void runtimeEnableStartsSamplingAndResetsMarkers();
    void enableDisableEnableResetsPriorMarkers();
    void resetWhileEnabledStartsFreshCapture();
};

#endif
