#ifndef DDP_RACE_TEST_H
#define DDP_RACE_TEST_H

#include <QObject>

class DDP_Race_Test final : public QObject
{
    Q_OBJECT

private slots:
    void overlappingCoverage_data();
    void overlappingCoverage();
    void concurrentRefresh_data();
    void concurrentRefresh();
    void ownedPendingBuffer_data();
    void ownedPendingBuffer();
    void pendingEndpointChange_data();
    void pendingEndpointChange();
    void throttleAndKeepAlive_data();
    void throttleAndKeepAlive();
    void pixelCountChangeDropsPending_data();
    void pixelCountChangeDropsPending();
};

#endif
