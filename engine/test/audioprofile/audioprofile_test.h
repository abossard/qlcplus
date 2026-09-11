#pragma once

#include <QObject>

class AudioProfile_Test : public QObject
{
    Q_OBJECT
private slots:
    void versionTwoRoundTrip_data();
    void versionTwoRoundTrip();
    void profileBinding();
};
