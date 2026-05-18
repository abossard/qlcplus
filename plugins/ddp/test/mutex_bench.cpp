// Measure DDPController mutex cost:
//  A) single-thread baseline (uncontended)
//  B) N threads each pinned to their own universe, sending concurrently
// Reports per-call latency percentiles for sendDmx.
#include <QCoreApplication>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QSharedPointer>
#include <QThread>
#include <QElapsedTimer>
#include <QUdpSocket>
#include "ddpcontroller.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

namespace {
QByteArray rgbFrame(int px, char r=0x11, char g=0x22, char b=0x33) {
    QByteArray b2(px*3, 0);
    for (int i=0;i<px;i++){ b2[i*3]=r; b2[i*3+1]=g; b2[i*3+2]=b; }
    return b2;
}
void pct(const char *label, std::vector<qint64> &v) {
    std::sort(v.begin(), v.end());
    auto p = [&](double q){ return v[size_t(q*(v.size()-1))]; };
    std::printf("  %-32s p50=%6lld ns  p95=%6lld ns  p99=%7lld ns  max=%8lld ns\n",
                label, (long long)p(0.50), (long long)p(0.95),
                (long long)p(0.99), (long long)v.back());
}
} // anon

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    QNetworkInterface iface;
    QNetworkAddressEntry entry; entry.setIp(QHostAddress::LocalHost);
    QUdpSocket rx; rx.bind(QHostAddress::LocalHost, 0);
    quint16 rxPort = rx.localPort();

    const int FRAMES = 5000;
    const int PIXELS_PER_UNI = 500; // 1500 bytes = 2 chunks worst case (>1440 by next test)

    // ============ A) single-thread baseline ============
    {
        DDPController ctrl(iface, entry, 0);
        ctrl.addUniverse(0);
        ctrl.setDestAddress(0, "127.0.0.1");
        ctrl.setDestPort(0, rxPort);
        ctrl.setDestId(0, 1);
        ctrl.setMaxFps(0);
        ctrl.setMaxFpsBypassForTest(true);
        ctrl.setTransmissionMode(0, DDPController::Partial);
        QByteArray frame = rgbFrame(PIXELS_PER_UNI);
        // warmup
        for (int i = 0; i < 100; i++) {
            frame[(i*7) % frame.size()] = char(i);
            ctrl.sendDmx(0, frame, true);
        }
        std::vector<qint64> times; times.reserve(FRAMES);
        QElapsedTimer t;
        for (int i = 0; i < FRAMES; i++) {
            frame[(i*11) % frame.size()] = char(i*3);
            t.start();
            ctrl.sendDmx(0, frame, true);
            times.push_back(t.nsecsElapsed());
        }
        std::printf("A) single-thread (1 universe), Partial mode, mutating frames:\n");
        pct("sendDmx latency", times);
        // drain receiver
        while (rx.hasPendingDatagrams()) { QByteArray d; d.resize(rx.pendingDatagramSize()); rx.readDatagram(d.data(), d.size()); }
    }

    // ============ B) N threads, each on own universe ============
    for (int N : {2, 4, 8}) {
        DDPController ctrl(iface, entry, 0);
        for (quint32 u = 0; u < quint32(N); u++) {
            ctrl.addUniverse(u);
            ctrl.setDestAddress(u, "127.0.0.1");
            ctrl.setDestPort(u, rxPort);
            ctrl.setDestId(u, 1);
            ctrl.setTransmissionMode(u, DDPController::Partial);
            // each universe gets a distinct ddpOffset so they don't trample
            ctrl.setDDPOffset(u, u * PIXELS_PER_UNI * 3);
        }
        ctrl.setMaxFps(0);
        ctrl.setMaxFpsBypassForTest(true);

        std::vector<std::vector<qint64>> perUni(N);
        for (auto &v : perUni) v.reserve(FRAMES);

        std::atomic<int> ready{0};
        std::atomic<bool> go{false};

        std::vector<std::thread> threads;
        for (int u = 0; u < N; u++) {
            threads.emplace_back([&, u]() {
                QByteArray frame = rgbFrame(PIXELS_PER_UNI);
                // warmup
                for (int i = 0; i < 100; i++) {
                    frame[(i*7) % frame.size()] = char(i + u);
                    ctrl.sendDmx(u, frame, true);
                }
                ready.fetch_add(1);
                while (!go.load()) { /* spin */ }
                QElapsedTimer t;
                for (int i = 0; i < FRAMES; i++) {
                    frame[(i*11) % frame.size()] = char(i*3 + u);
                    t.start();
                    ctrl.sendDmx(u, frame, true);
                    perUni[u].push_back(t.nsecsElapsed());
                }
            });
        }
        while (ready.load() < N) {}
        go.store(true);
        for (auto &t : threads) t.join();

        std::printf("B) %d threads each on own universe (mutex contended):\n", N);
        std::vector<qint64> all;
        for (auto &v : perUni) all.insert(all.end(), v.begin(), v.end());
        pct("  aggregated sendDmx latency", all);
        for (int u = 0; u < N; u++) pct(QString("  universe %1").arg(u).toUtf8().constData(), perUni[u]);

        while (rx.hasPendingDatagrams()) { QByteArray d; d.resize(rx.pendingDatagramSize()); rx.readDatagram(d.data(), d.size()); }
    }

    return 0;
}
