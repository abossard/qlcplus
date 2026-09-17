#include "ddp_race_test.h"

#include <QtTest>
#include <QtEndian>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QSemaphore>
#include <QUdpSocket>
#include <array>
#include <atomic>
#include <memory>
#include <random>
#include <thread>

#include "ddpplugin.h"
#include "fadechannel.h"
#include "genericfader.h"
#include "grandmaster.h"
#include "outputpatch.h"
#include "universe.h"

namespace {

// Like universe_test, expose one engine tick without starting its free-running loop.
class TickUniverse : public Universe
{
public:
    using Universe::Universe;
    using Universe::processFaders;
};

class Receiver
{
public:
    Receiver()
    {
        worker = std::thread([this] {
            QUdpSocket socket;
            if (!socket.bind(QHostAddress::LocalHost, 0))
            {
                ready.release();
                return;
            }
            socket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 1 << 20);
            port = socket.localPort();
            ready.release();
            while (!stopping)
            {
                if (!socket.hasPendingDatagrams())
                    socket.waitForReadyRead(10);
                while (socket.hasPendingDatagrams())
                {
                    QByteArray packet(int(socket.pendingDatagramSize()), 0);
                    const auto count = socket.readDatagram(packet.data(), packet.size());
                    QMutexLocker lock(&mutex);
                    if (count != packet.size())
                        errors++;
                    packets.append(packet);
                }
            }
        });
        ready.acquire();
    }

    ~Receiver()
    {
        stopping = true;
        worker.join();
    }

    QVector<QByteArray> snapshot()
    {
        QMutexLocker lock(&mutex);
        return packets;
    }

    std::atomic<quint16> port{0};
    std::atomic<int> errors{0};

private:
    QSemaphore ready;
    QMutex mutex;
    QVector<QByteArray> packets;
    std::atomic<bool> stopping{false};
    std::thread worker;
};

QByteArray rowPixels(int row, int bpp)
{
    static const uchar colors[4][4] = {
        {19, 53, 97, 7}, {37, 83, 131, 11}, {61, 109, 173, 13}, {89, 149, 211, 17}
    };
    QByteArray data(80 * bpp, 0);
    for (int pixel = 0; pixel < 80; pixel++)
        for (int component = 0; component < bpp; component++)
            data[pixel * bpp + component] = char(colors[row][component] + pixel % 7);
    return data;
}

struct EngineWire
{
    EngineWire(int bpp, quint16 port)
    {
        plugin.init();
        line = plugin.outputs().indexOf("127.0.0.1");
        if (line < 0)
            return;
        for (int row = 0; row < 4; row++)
        {
            universes[row].reset(new TickUniverse(row, &gm));
            auto &universe = *universes[row];
            if (!universe.setOutputPatch(&plugin, line))
                return;
            auto *patch = universe.outputPatch(0);
            patch->setPluginParameter(DDP_IP, "127.0.0.1");
            patch->setPluginParameter(DDP_DESTPORT, port);
            patch->setPluginParameter(DDP_OFFSET, row * 80 * bpp);
            patch->setPluginParameter(DDP_COMPONENTS, bpp == 4 ? "RGBW" : "RGB");
            patch->setPluginParameter(DDP_TRANSMITMODE, "Partial");
            patch->setPluginParameter(DDP_MAXFPS, 0);
            patch->setPluginParameter(DDP_KEEPALIVEMS, 2400);
            faders[row] = universe.requestFader();
        }
        controller = plugin.getIOMapping().at(line).controller;
    }

    void setRow(int row, const QByteArray &data)
    {
        for (int channel = 0; channel < data.size(); channel++)
        {
            if (channel >= universes[row]->totalChannels())
                universes[row]->setChannelCapability(channel, QLCChannel::Intensity);
            FadeChannel fade;
            fade.addChannel(channel);
            fade.setFlags(FadeChannel::Intensity | FadeChannel::HTP);
            fade.setTarget(uchar(data[channel]));
            faders[row]->replace(fade);
        }
    }

    // Every job enters from a different OS thread. Ordered mode also holds all
    // later reporters behind barriers while an earlier callback finishes.
    void callbacks(const QVector<int> &order, bool simultaneous = false)
    {
        QElapsedTimer elapsed;
        elapsed.start();
        QSemaphore ready, done, entered, proceed;
        std::array<QSemaphore, 4> gates;
        std::vector<std::thread> threads;
        std::vector<QMetaObject::Connection> connections;
        if (simultaneous)
            for (int row : order)
            {
                faders[row]->setMonitoring(true);
                connections.push_back(QObject::connect(
                    faders[row].data(), &GenericFader::preWriteData, faders[row].data(),
                    [&] {
                        entered.release();
                        if (!proceed.tryAcquire(1, 2000))
                            qFatal("Concurrent fader barrier timed out");
                    }, Qt::DirectConnection));
            }
        for (int index = 0; index < order.size(); index++)
            threads.emplace_back([&, index] {
                ready.release();
                gates[index].acquire();
                universes[order[index]]->processFaders(20);
                done.release();
            });
        ready.acquire(order.size());
        for (int index = 0; index < order.size(); index++)
        {
            gates[index].release();
            if (!simultaneous)
                done.acquire();
        }
        if (simultaneous)
        {
            if (!entered.tryAcquire(order.size(), 2000))
                qFatal("Not all concurrent engine callbacks entered within 2 seconds");
            proceed.release(order.size());
            done.acquire(order.size());
        }
        for (auto &thread : threads)
            thread.join();
        for (const auto &connection : connections)
            QObject::disconnect(connection);
        for (int row : order)
            faders[row]->setMonitoring(false);
        maxCallbackBatchMs = qMax(maxCallbackBatchMs, elapsed.elapsed());
    }

    DDPPlugin plugin;
    GrandMaster gm;
    std::array<std::unique_ptr<TickUniverse>, 4> universes;
    std::array<QSharedPointer<GenericFader>, 4> faders;
    QSharedPointer<DDPController> controller;
    int line = -1;
    qint64 maxCallbackBatchMs = 0;
};

QByteArray rgba(const QByteArray &pixels, int bpp)
{
    if (bpp == 4)
        return pixels;
    QByteArray result;
    for (int pixel = 0; pixel < pixels.size() / bpp; pixel++)
    {
        result.append(pixels.mid(pixel * bpp, bpp));
        result.append(char(0));
    }
    return result;
}

void verifyPushes(const QVector<QByteArray> &packets, const QVector<QByteArray> &expected, int bpp,
                  const QString &suffix = QString())
{
    QByteArray pixels(expected.front().size(), 0);
    int pushes = 0;
    int burstSequence = 0;
    int previousSequence = 0;
    QFile evidence;
    const QString directory = qEnvironmentVariable("DDP_RACE_EVIDENCE");
    if (!directory.isEmpty())
    {
        evidence.setFileName(directory + "/ddp-race-" + QTest::currentTestFunction()
                             + "-" + QTest::currentDataTag() + suffix + ".jsonl");
        QVERIFY(evidence.open(QIODevice::WriteOnly | QIODevice::Truncate));
    }
    for (const auto &packet : packets)
    {
        QVERIFY(packet.size() >= 10);
        const auto *header = reinterpret_cast<const uchar *>(packet.constData());
        const int offset = qFromBigEndian<quint32>(header + 4);
        const int length = qFromBigEndian<quint16>(header + 8);
        const int sequence = header[1] & 15;
        QCOMPARE(header[0] & 0xc0, 0x40);
        QCOMPARE(header[2], uchar(bpp == 4 ? DDP_DATATYPE_RGBW888 : DDP_DATATYPE_RGB888));
        QCOMPARE(header[3], uchar(1));
        QCOMPARE(packet.size(), 10 + length);
        QVERIFY(offset >= 0 && offset + length <= pixels.size());
        QCOMPARE(offset % bpp, 0);
        QCOMPARE(length % bpp, 0);
        if (burstSequence == 0)
        {
            QCOMPARE(sequence, previousSequence % 15 + 1);
            burstSequence = sequence;
        }
        QCOMPARE(sequence, burstSequence);
        std::copy(packet.cbegin() + 10, packet.cend(), pixels.begin() + offset);
        QJsonObject record{{"hex", QString::fromLatin1(packet.toHex())}};
        if (header[0] & DDP_FLAGS_PUSH)
        {
            QVERIFY(pushes < expected.size());
            record["expected_hex"] = QString::fromLatin1(rgba(expected[pushes], bpp).toHex());
            if (evidence.isOpen())
                evidence.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n');
            QCOMPARE(pixels, expected[pushes]);
            pushes++;
            previousSequence = sequence;
            burstSequence = 0;
        }
        else if (evidence.isOpen())
            evidence.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n');
    }
    QCOMPARE(burstSequence, 0);
    QCOMPARE(pushes, expected.size());
}

}

void DDP_Race_Test::overlappingCoverage_data()
{
    QTest::addColumn<int>("lastReporter");
    QTest::newRow("row2-tail") << 0;
    QTest::newRow("row3-tail") << 1;
}

void DDP_Race_Test::overlappingCoverage()
{
    QFETCH(int, lastReporter);
    Receiver receiver;
    QVERIFY(receiver.port != 0);
    EngineWire wire(4, receiver.port);
    QVERIFY(wire.controller);
    QByteArray intended;
    for (int row = 0; row < 4; row++)
    {
        QByteArray data = rowPixels(row, 4);
        intended += data;
        if (row < 2)
            data.append(QByteArray(160, 0));
        wire.setRow(row, data);
    }
    wire.callbacks({0, 1, 2, 3});
    QCOMPARE(wire.universes[0]->usedChannels(), ushort(480));
    QCOMPARE(wire.universes[1]->usedChannels(), ushort(480));
    QCOMPARE(wire.universes[2]->usedChannels(), ushort(320));
    wire.universes[lastReporter]->outputPatch(0)->setPluginParameter(DDP_KEEPALIVEMS, 100);
    QTest::qSleep(110);
    QVector<int> order;
    for (int row = 0; row < 4; row++)
        if (row != lastReporter)
            order.append(row);
    order.append(lastReporter);
    wire.callbacks(order);
    QTRY_COMPARE(receiver.snapshot().size(), qsizetype(wire.controller->getPacketSentNumber()));
    QCOMPARE(receiver.errors.load(), 0);
    // The configured 480-byte coverage really addresses the next row's first
    // 40 pixels. Preserve this witness, not a truncation of legitimate zeros.
    QByteArray overlapped = intended;
    overlapped.replace((lastReporter + 1) * 320, 160, QByteArray(160, 0));
    const auto packets = receiver.snapshot();
    QCOMPARE(packets.size(), 8);
    const auto &last = packets.last();
    QCOMPARE(qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(last.constData()) + 4),
             quint32(lastReporter * 320));
    QCOMPARE(last.mid(10 + 320), QByteArray(160, 0));
    for (int row = 0; row < 4; row++)
        QCOMPARE(wire.universes[row]->preGMValues().left(320), rowPixels(row, 4));
    verifyPushes(packets, {intended, overlapped}, 4);
}

void DDP_Race_Test::concurrentRefresh_data()
{
    QTest::addColumn<int>("bpp");
    QTest::addColumn<int>("schedule");
    for (int bpp : {3, 4})
        for (int schedule = 0; schedule < 5; schedule++)
            QTest::newRow(qPrintable(QString("%1-schedule%2").arg(bpp).arg(schedule))) << bpp << schedule;
}

void DDP_Race_Test::concurrentRefresh()
{
    QFETCH(int, bpp);
    QFETCH(int, schedule);
    Receiver receiver;
    QVERIFY(receiver.port != 0);
    EngineWire wire(bpp, receiver.port);
    QVERIFY(wire.controller);
    std::array<QByteArray, 4> rows;
    QByteArray intended;
    for (int row = 0; row < 4; row++)
    {
        rows[row] = rowPixels(row, bpp);
        intended += rows[row];
        wire.setRow(row, rows[row]);
    }
    wire.callbacks({0, 1, 2, 3});
    QVector<QByteArray> expected{intended};
    constexpr unsigned seed = 0x5eed480;
    std::mt19937 random(seed);
    constexpr int iterations = 64;
    for (int iteration = 0; iteration < iterations; iteration++)
    {
        QTest::qSleep(20);
        // Hold three rows in schedules 0..3; schedule 4 changes all four.
        const int firstRow = schedule == 3 ? iteration % 4 : 0;
        const int endRow = schedule == 4 ? 4 : firstRow + 1;
        for (int row = firstRow; row < endRow; row++)
        {
            rows[row][5 * bpp] = char(31 + iteration + row * 11);
            rows[row][71 * bpp + 1] = char(181 - iteration - row * 13);
            wire.setRow(row, rows[row]);
            intended.replace(row * 80 * bpp, 80 * bpp, rows[row]);
        }
        QVector<int> order{0, 1, 2, 3};
        if (schedule == 1)
            order = {3, 2, 1, 0};
        else if (schedule >= 3)
            std::shuffle(order.begin(), order.end(), random);
        wire.callbacks(order, schedule >= 2);
        expected.append(intended);
    }
    // If the last round crossed the fallback deadline, finish its pending
    // reports without changing the faders. Input stopping is not a timer tick.
    wire.callbacks({0, 1, 2, 3});
    QTRY_COMPARE(receiver.snapshot().size(), qsizetype(wire.controller->getPacketSentNumber()));
    QCOMPARE(receiver.errors.load(), 0);
    verifyPushes(receiver.snapshot(), expected, bpp);
    const auto packets = receiver.snapshot();
    const auto pushes = std::count_if(packets.cbegin(), packets.cend(),
                                    [](const QByteArray &packet) { return packet[0] & 1; });
    qInfo("seed=0x%X iterations=%d schedule=%d callbacks=%d packets=%lld pushes=%lld receiveErrors=%d maxBatchMs=%lld",
          seed, iterations, schedule, (iterations + 2) * 4,
          static_cast<long long>(packets.size()), static_cast<long long>(pushes),
          receiver.errors.load(), static_cast<long long>(wire.maxCallbackBatchMs));
}

void DDP_Race_Test::ownedPendingBuffer_data()
{
    QTest::addColumn<int>("bpp");
    QTest::newRow("RGB") << 3;
    QTest::newRow("RGBW") << 4;
}

void DDP_Race_Test::ownedPendingBuffer()
{
    QFETCH(int, bpp);
    Receiver receiver;
    QVERIFY(receiver.port != 0);
    EngineWire wire(bpp, receiver.port);
    QVERIFY(wire.controller);
    QByteArray intended;
    for (int row = 0; row < 4; row++)
    {
        const auto data = rowPixels(row, bpp);
        intended += data;
        wire.setRow(row, data);
    }
    wire.callbacks({0, 1, 2, 3});
    QVector<QByteArray> expected{intended};
    QTest::qSleep(20);
    wire.callbacks({0});
    QByteArray latest = rowPixels(0, bpp);
    latest[11 * bpp] = char(233);
    latest[69 * bpp + 2] = char(199);
    wire.setRow(0, latest);
    wire.callbacks({0});
    // Universe passes fromRawData. Overwrite that exact storage after return,
    // before any completer. Neither the old report nor this poison may win.
    for (int channel = 0; channel < latest.size(); channel++)
        wire.universes[0]->write(channel, 0, true);
    QCOMPARE(wire.universes[0]->postGMValues()->left(latest.size()), QByteArray(latest.size(), 0));
    wire.callbacks({1, 2, 3}, true);
    intended.replace(0, latest.size(), latest);
    expected.append(intended);
    QTRY_COMPARE(receiver.snapshot().size(), qsizetype(wire.controller->getPacketSentNumber()));
    QCOMPARE(receiver.errors.load(), 0);
    verifyPushes(receiver.snapshot(), expected, bpp);
}

void DDP_Race_Test::pendingEndpointChange_data()
{
    QTest::addColumn<int>("bpp");
    QTest::newRow("RGB") << 3;
    QTest::newRow("RGBW") << 4;
}

void DDP_Race_Test::pendingEndpointChange()
{
    QFETCH(int, bpp);
    Receiver receiver, second;
    QVERIFY(receiver.port != 0 && second.port != 0);
    EngineWire wire(bpp, receiver.port);
    QVERIFY(wire.controller);
    QByteArray intended;
    for (int row = 0; row < 4; row++)
    {
        auto data = rowPixels(row, bpp);
        intended += data;
        wire.setRow(row, data);
    }
    wire.callbacks({0, 1, 2, 3});
    QVector<QByteArray> oldExpected{intended};
    QTest::qSleep(20);
    wire.callbacks({0});
    // Configuration is serialized between completed and barrier-held callbacks.
    wire.universes[0]->outputPatch(0)->setPluginParameter(DDP_DESTPORT, second.port.load());
    QByteArray changed = rowPixels(1, bpp);
    changed[7 * bpp] = char(223);
    wire.setRow(1, changed);
    wire.callbacks({1, 2, 3}, true);
    intended.replace(80 * bpp, 80 * bpp, changed);
    oldExpected.append(intended);
    QTest::qSleep(20);
    changed[9 * bpp] = char(219);
    wire.setRow(1, changed);
    wire.callbacks({1, 2}, true); // row 3 is stalled
    const auto oldCount = wire.controller->getPacketSentNumber();
    wire.callbacks({0});
    QVector<QByteArray> newExpected{rowPixels(0, bpp) + QByteArray(240 * bpp, 0)};
    QVERIFY(wire.controller->getPacketSentNumber() > oldCount);
    QByteArray independent = rowPixels(0, bpp);
    independent[23 * bpp] = char(247);
    wire.setRow(0, independent);
    wire.callbacks({0});
    newExpected.append(independent + QByteArray(240 * bpp, 0));
    QTest::qSleep(90);
    // No callback, no timer-driven flush. The next report releases the fallback.
    const auto beforeFallback = wire.controller->getPacketSentNumber();
    wire.callbacks({1, 2}, true);
    QVERIFY(wire.controller->getPacketSentNumber() > beforeFallback);
    intended.replace(80 * bpp, 80 * bpp, changed);
    oldExpected.append(intended);
    QByteArray late = rowPixels(3, bpp);
    late[37 * bpp + 1] = char(239);
    wire.setRow(3, late);
    wire.callbacks({3});
    wire.callbacks({1, 2}, true);
    intended.replace(240 * bpp, 80 * bpp, late);
    oldExpected.append(intended);
    QTRY_COMPARE(receiver.snapshot().size() + second.snapshot().size(),
                 qsizetype(wire.controller->getPacketSentNumber()));
    QCOMPARE(receiver.errors.load() + second.errors.load(), 0);
    // Export the two endpoint streams independently. Each starts at sequence 1.
    verifyPushes(receiver.snapshot(), oldExpected, bpp);
    verifyPushes(second.snapshot(), newExpected, bpp, "-second");
}

void DDP_Race_Test::throttleAndKeepAlive_data()
{
    ownedPendingBuffer_data();
}

void DDP_Race_Test::throttleAndKeepAlive()
{
    QFETCH(int, bpp);
    Receiver receiver;
    QVERIFY(receiver.port != 0);
    EngineWire wire(bpp, receiver.port);
    QVERIFY(wire.controller);
    QByteArray initial, changed;
    std::array<QByteArray, 4> rows;
    for (int row = 0; row < 4; row++)
    {
        rows[row] = rowPixels(row, bpp);
        initial += rows[row];
        wire.setRow(row, rows[row]);
    }
    wire.universes[0]->outputPatch(0)->setPluginParameter(DDP_KEEPALIVEMS, 100);
    wire.universes[0]->outputPatch(0)->setPluginParameter(DDP_MAXFPS, 1);
    wire.universes[3]->outputPatch(0)->setPluginParameter(DDP_TRANSMITMODE, "Full");
    QTest::qSleep(20); // a nonzero timestamp is needed to exercise the throttle
    wire.callbacks({0, 1, 2, 3});
    const auto primed = wire.controller->getPacketSentNumber();
    for (int row = 0; row < 4; row++)
    {
        rows[row][17 * bpp] = char(207 + row);
        changed += rows[row];
        wire.setRow(row, rows[row]);
    }
    wire.callbacks({3, 1, 0, 2}, true);
    QCOMPARE(wire.controller->getPacketSentNumber(), primed);
    QTest::qSleep(110);
    wire.callbacks({2, 0, 3, 1}, true);
    QByteArray partialRefresh = changed;
    partialRefresh.replace(240 * bpp, 80 * bpp, rowPixels(3, bpp));
    QTest::qSleep(1000);
    wire.callbacks({1, 3, 2, 0}, true);
    QTRY_COMPARE(receiver.snapshot().size(), qsizetype(wire.controller->getPacketSentNumber()));
    QCOMPARE(receiver.errors.load(), 0);
    verifyPushes(receiver.snapshot(), {initial, partialRefresh, changed}, bpp);
}

void DDP_Race_Test::pixelCountChangeDropsPending_data()
{
    ownedPendingBuffer_data();
}

void DDP_Race_Test::pixelCountChangeDropsPending()
{
    QFETCH(int, bpp);
    Receiver receiver;
    QVERIFY(receiver.port != 0);
    EngineWire wire(bpp, receiver.port);
    QVERIFY(wire.controller);
    QByteArray initial, configured;
    for (int row = 0; row < 4; row++)
    {
        auto data = rowPixels(row, bpp);
        initial += data;
        wire.setRow(row, data);
    }
    wire.callbacks({0, 1, 2, 3});
    const auto primed = wire.controller->getPacketSentNumber();
    QTest::qSleep(100);
    QByteArray changed = rowPixels(0, bpp);
    changed[10 * bpp] = char(251);
    wire.setRow(0, changed);
    wire.callbacks({0}); // accepted using the old 80-pixel coverage
    wire.universes[0]->outputPatch(0)->setPluginParameter(DDP_PIXELCOUNT, 40);
    wire.callbacks({1, 2, 3}, true);
    QTRY_COMPARE(receiver.snapshot().size(), qsizetype(wire.controller->getPacketSentNumber()));
    QCOMPARE(receiver.snapshot().size(), qsizetype(primed));
    QCOMPARE(wire.controller->getPacketSentNumber(), primed);
    wire.callbacks({0});
    for (int row = 0; row < 4; row++)
        configured += (row == 0 ? changed : rowPixels(row, bpp)).left(40 * bpp)
                      + QByteArray(40 * bpp, 0);
    QTRY_COMPARE(receiver.snapshot().size(), qsizetype(wire.controller->getPacketSentNumber()));
    QCOMPARE(receiver.errors.load(), 0);
    verifyPushes(receiver.snapshot(), {initial, configured}, bpp);
}

QTEST_GUILESS_MAIN(DDP_Race_Test)
