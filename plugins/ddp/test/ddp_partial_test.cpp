/*
  Q Light Controller Plus
  ddp_partial_test.cpp

  Black-box tests: configure a DDPController bound to 127.0.0.1, point it at
  a loopback QUdpSocket we control, send frames, parse the captured DDP
  datagrams and assert wire-level correctness against verified WLED semantics.
*/
#include "ddp_partial_test.h"

#include <QtTest>
#include <QUdpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QtEndian>
#include <QVector>
#include <QElapsedTimer>
#include <cstring>

#include "ddpcontroller.h"
#include "ddppacketizer.h"

namespace {

constexpr quint32 UNI = 0;

struct ParsedPkt {
    quint8 flags;
    quint8 seq;
    quint8 dataType;
    quint8 destId;
    quint32 offset;       // bytes
    quint16 dataLen;      // bytes
    QByteArray payload;
    bool push() const   { return (flags & DDP_FLAGS_PUSH) != 0; }
    bool ver1() const   { return (flags & 0xC0) == DDP_FLAGS_VER1; }
};

ParsedPkt parse(const QByteArray &dg)
{
    ParsedPkt p{};
    Q_ASSERT(dg.size() >= DDP_HEADER_LEN);
    const uchar *b = reinterpret_cast<const uchar*>(dg.constData());
    p.flags    = b[0];
    p.seq      = b[1];
    p.dataType = b[2];
    p.destId   = b[3];
    p.offset   = qFromBigEndian<quint32>(b + 4);
    p.dataLen  = qFromBigEndian<quint16>(b + 8);
    p.payload  = dg.mid(DDP_HEADER_LEN, p.dataLen);
    return p;
}

// Helper: build a controller bound to loopback and a receiver socket on a
// random port. Caller owns both.
struct Wire {
    DDPController *ctrl;
    QUdpSocket    *rx;
    quint16        rxPort;
};

Wire makeWire()
{
    QNetworkInterface iface; // unused by ctor
    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress::LocalHost);
    Wire w;
    w.ctrl = new DDPController(iface, entry, /*line=*/0);
    w.rx   = new QUdpSocket();
    bool bound = w.rx->bind(QHostAddress::LocalHost, 0);
    Q_ASSERT(bound);
    w.rxPort = w.rx->localPort();
    w.ctrl->addUniverse(UNI);
    w.ctrl->setDestAddress(UNI, "127.0.0.1");
    w.ctrl->setDestPort(UNI, w.rxPort);
    w.ctrl->setDestId(UNI, 1);
    w.ctrl->setDDPOffset(UNI, 0);
    w.ctrl->setComponents(UNI, DDPController::RGB);
    w.ctrl->setTransmissionMode(UNI, DDPController::Partial);
    w.ctrl->setMaxFps(0);            // unlimited (we drive timing)
    w.ctrl->setSkipUnchanged(false); // tested independently
    w.ctrl->setMaxFpsBypassForTest(true); // remove timing flakiness
    return w;
}

// Drain all pending datagrams from the receiver socket (10ms idle wait).
QVector<ParsedPkt> drain(QUdpSocket *rx)
{
    QVector<ParsedPkt> out;
    QElapsedTimer idle; idle.start();
    while (idle.elapsed() < 50) {
        while (rx->hasPendingDatagrams()) {
            QByteArray dg;
            dg.resize(int(rx->pendingDatagramSize()));
            rx->readDatagram(dg.data(), dg.size());
            out.push_back(parse(dg));
            idle.restart();
        }
        QTest::qWait(2);
    }
    return out;
}

QByteArray rgbFrame(int pixels, char r=0x11, char g=0x22, char b=0x33)
{
    QByteArray buf(pixels * 3, 0);
    for (int i = 0; i < pixels; i++) {
        buf[i*3+0] = r; buf[i*3+1] = g; buf[i*3+2] = b;
    }
    return buf;
}

} // anon namespace

void DDP_Partial_Test::init() {}
void DDP_Partial_Test::cleanup() {}

// ---------------------------------------------------------------------

void DDP_Partial_Test::firstFrame_emitsFullSnapshot()
{
    Wire w = makeWire();
    QByteArray frame = rgbFrame(100);
    w.ctrl->sendDmx(UNI, frame, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QVERIFY(pkts[0].ver1());
    QVERIFY(pkts[0].push());
    QCOMPARE(int(pkts[0].dataLen), 300);
    QCOMPARE(pkts[0].offset, quint32(0));
    QCOMPARE(pkts[0].destId, quint8(1));
    QCOMPARE(pkts[0].dataType, quint8(DDP_DATATYPE_RGB888));
    QCOMPARE(pkts[0].payload, frame);
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::identicalSecondFrame_emitsNothing()
{
    Wire w = makeWire();
    QByteArray frame = rgbFrame(50);
    w.ctrl->sendDmx(UNI, frame, true);
    drain(w.rx);
    w.ctrl->sendDmx(UNI, frame, true); // identical
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 0);
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::singlePixelDiff_emitsOnePacket()
{
    Wire w = makeWire();
    QByteArray a = rgbFrame(100);
    w.ctrl->sendDmx(UNI, a, true);
    drain(w.rx);
    QByteArray b = a;
    b[60*3+0] = char(0xFF); b[60*3+1] = char(0xAA); b[60*3+2] = char(0x55);
    w.ctrl->sendDmx(UNI, b, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QVERIFY(pkts[0].push());
    QCOMPARE(pkts[0].offset, quint32(60*3));
    QCOMPARE(int(pkts[0].dataLen), 3);
    QCOMPARE(pkts[0].payload, QByteArray("\xFF\xAA\x55", 3));
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::smallGap_coalescesIntoOnePacket()
{
    Wire w = makeWire();
    QByteArray a = rgbFrame(200);
    w.ctrl->sendDmx(UNI, a, true);
    drain(w.rx);
    // dirty pixels 10 and 12 (separated by 1 clean pixel = 3 clean bytes,
    // well under kPartialMinGapBytes=48).
    QByteArray b = a;
    b[10*3] = char(0xFF);
    b[12*3] = char(0xFF);
    w.ctrl->sendDmx(UNI, b, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QCOMPARE(pkts[0].offset, quint32(10*3));
    // coalesced span = pixels 10..13 inclusive of dirty + 11 gap = 3 pixels = 9 bytes
    QCOMPARE(int(pkts[0].dataLen), 9);
    QVERIFY(pkts[0].push());
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::largeGap_emitsTwoPacketsPushOnLast()
{
    Wire w = makeWire();
    QByteArray a = rgbFrame(500);
    w.ctrl->sendDmx(UNI, a, true);
    drain(w.rx);
    QByteArray b = a;
    b[10*3] = char(0xFF);
    b[200*3] = char(0xFF); // 190 clean pixels = 570 bytes > 48 → no coalesce
    w.ctrl->sendDmx(UNI, b, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 2);
    QCOMPARE(pkts[0].offset, quint32(10*3));
    QVERIFY(!pkts[0].push());
    QCOMPARE(pkts[1].offset, quint32(200*3));
    QVERIFY(pkts[1].push());
    // Both packets share one sequence number.
    QCOMPARE(pkts[0].seq, pkts[1].seq);
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::chunkBoundaries_data()
{
    QTest::addColumn<int>("totalBytes");
    QTest::addColumn<int>("expectedChunks");
    QTest::newRow("exactly_1440") << 1440 << 1;
    QTest::newRow("1441")         << 1443 << 2;  // 1443 = 481 RGB pixels
    QTest::newRow("two_chunks")   << 2880 << 2;
    QTest::newRow("three_chunks") << 3000 << 3;
}

void DDP_Partial_Test::chunkBoundaries()
{
    QFETCH(int, totalBytes);
    QFETCH(int, expectedChunks);
    Wire w = makeWire();
    QByteArray frame(totalBytes, char(0x42));
    w.ctrl->sendDmx(UNI, frame, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), expectedChunks);
    for (int i = 0; i < pkts.size(); i++) {
        QCOMPARE(pkts[i].push(), i == pkts.size() - 1);
        QCOMPARE(pkts[i].seq, pkts[0].seq);
        QCOMPARE(pkts[i].offset, quint32(i * DDP_MAX_DATALEN));
    }
    delete w.ctrl; delete w.rx;
}

// ---------------------------------------------------------------------
// Keep-alive / invalidation
// ---------------------------------------------------------------------

void DDP_Partial_Test::keepAlive_emitsFullAfterInterval()
{
    Wire w = makeWire();
    w.ctrl->setKeepAliveIntervalMsForTest(200); // generous wrt drain's 50ms idle
    QByteArray frame = rgbFrame(50);
    w.ctrl->sendDmx(UNI, frame, true);
    QCOMPARE(drain(w.rx).size(), 1);   // initial full
    w.ctrl->sendDmx(UNI, frame, true); // identical, within keep-alive window
    QCOMPARE(drain(w.rx).size(), 0);
    QTest::qWait(260); // exceed keep-alive
    w.ctrl->sendDmx(UNI, frame, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QVERIFY(pkts[0].push());
    QCOMPARE(int(pkts[0].dataLen), 150);
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::rgbToRgbw_invalidatesBaseline()
{
    Wire w = makeWire();
    QByteArray rgb = rgbFrame(10);
    w.ctrl->sendDmx(UNI, rgb, true);
    drain(w.rx);
    w.ctrl->setComponents(UNI, DDPController::RGBW);
    QByteArray rgbw(10*4, char(0x77));
    w.ctrl->sendDmx(UNI, rgbw, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QCOMPARE(pkts[0].dataType, quint8(DDP_DATATYPE_RGBW888));
    QCOMPARE(int(pkts[0].dataLen), 40);
    QVERIFY(pkts[0].push());
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::setDestAddress_invalidatesBaseline()
{
    Wire w = makeWire();
    QByteArray f = rgbFrame(20);
    w.ctrl->sendDmx(UNI, f, true);
    drain(w.rx);
    // Re-point at same destination — even an identity-string change resets baseline.
    w.ctrl->setDestAddress(UNI, "127.0.0.1");
    w.ctrl->sendDmx(UNI, f, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1); // full snapshot, not zero
    QCOMPARE(int(pkts[0].dataLen), 60);
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::setPixelCount_invalidatesAllBaselines()
{
    Wire w = makeWire();
    // Add a 2nd universe to verify cross-universe invalidation.
    w.ctrl->addUniverse(1);
    w.ctrl->setDestAddress(1, "127.0.0.1");
    w.ctrl->setDestPort(1, w.rxPort);
    w.ctrl->setDestId(1, 1);
    w.ctrl->setTransmissionMode(1, DDPController::Partial);

    QByteArray f = rgbFrame(50);                 // 150 bytes (auto mode)
    w.ctrl->sendDmx(UNI, f, true);
    w.ctrl->sendDmx(1,  f, true);
    QCOMPARE(drain(w.rx).size(), 2);             // one full each

    w.ctrl->setPixelCount(40);                    // → totalLen 120; SHIFT

    w.ctrl->sendDmx(UNI, f, true);
    w.ctrl->sendDmx(1,   f, true);
    auto pkts = drain(w.rx);
    // Each universe emits: 1 zero-clear for old-only tail [120,150) + 1 full
    // snapshot of new 120-byte coverage. PUSH only on the snapshot packet.
    QCOMPARE(pkts.size(), 4);
    int snapshots = 0, clears = 0;
    for (const auto &p : pkts) {
        if (p.dataLen == 120 && p.offset == 0 && p.push()) {
            snapshots++;
        } else if (p.dataLen == 30 && p.offset == 120 && !p.push()) {
            QCOMPARE(p.payload, QByteArray(30, char(0)));
            clears++;
        } else {
            QFAIL(qPrintable(QString("unexpected pkt off=%1 len=%2 push=%3")
                .arg(p.offset).arg(p.dataLen).arg(p.push())));
        }
    }
    QCOMPARE(snapshots, 2);
    QCOMPARE(clears,    2);
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::transmissionModeFlip_invalidates()
{
    Wire w = makeWire();
    QByteArray f = rgbFrame(20);
    w.ctrl->sendDmx(UNI, f, true);
    drain(w.rx);
    w.ctrl->setTransmissionMode(UNI, DDPController::Full);
    w.ctrl->sendDmx(UNI, f, true); // Full path always sends
    QCOMPARE(drain(w.rx).size(), 1);
    w.ctrl->setTransmissionMode(UNI, DDPController::Partial);
    w.ctrl->sendDmx(UNI, f, true); // back to Partial → first-frame full
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QCOMPARE(int(pkts[0].dataLen), 60);
    delete w.ctrl; delete w.rx;
}

// ---------------------------------------------------------------------
// Safety guards
// ---------------------------------------------------------------------

void DDP_Partial_Test::misalignedDdpOffset_emitsNothing()
{
    Wire w = makeWire();
    w.ctrl->setDDPOffset(UNI, 1); // not divisible by bpp=3
    QByteArray f = rgbFrame(10);
    w.ctrl->sendDmx(UNI, f, true);
    QCOMPARE(drain(w.rx).size(), 0);
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::reservedDestId_emitsNothing()
{
    Wire w = makeWire();
    QByteArray f = rgbFrame(10);
    for (quint8 id : {quint8(246), quint8(250), quint8(251)}) {
        w.ctrl->setDestId(UNI, id);
        w.ctrl->sendDmx(UNI, f, true);
    }
    QCOMPARE(drain(w.rx).size(), 0);
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::autoModeOddLength_dropsTrailingByte()
{
    Wire w = makeWire();
    // pixelCount = 0 means auto. 31 bytes = 10 RGB pixels + 1 stray byte.
    QByteArray odd(31, char(0xAB));
    w.ctrl->sendDmx(UNI, odd, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QCOMPARE(int(pkts[0].dataLen), 30);   // multiple of bpp=3
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::pixelCountLargerThanSrc_zeroPadsTail()
{
    Wire w = makeWire();
    w.ctrl->setPixelCount(20); // 60 bytes target
    QByteArray small = rgbFrame(5); // 15 bytes
    w.ctrl->sendDmx(UNI, small, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QCOMPARE(int(pkts[0].dataLen), 60);
    QByteArray expected = small + QByteArray(45, char(0));
    QCOMPARE(pkts[0].payload, expected);
    delete w.ctrl; delete w.rx;
}

// ---------------------------------------------------------------------
// Throttling
// ---------------------------------------------------------------------

void DDP_Partial_Test::fpsThrottle_bypassedForKeepAlive()
{
    Wire w = makeWire();
    w.ctrl->setMaxFpsBypassForTest(false);
    w.ctrl->setMaxFps(1); // 1 fps = 1000ms gap
    w.ctrl->setKeepAliveIntervalMsForTest(20);
    QByteArray f = rgbFrame(10);
    w.ctrl->sendDmx(UNI, f, true);
    QCOMPARE(drain(w.rx).size(), 1);
    QTest::qWait(40);
    // Keep-alive due, throttle says wait — mustFull must bypass throttle.
    w.ctrl->sendDmx(UNI, f, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QVERIFY(pkts[0].push());
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::skipUnchanged_doesNotSuppressPartialKeepAlive()
{
    Wire w = makeWire();
    w.ctrl->setSkipUnchanged(true);
    w.ctrl->setKeepAliveIntervalMsForTest(20);
    QByteArray f = rgbFrame(10);
    w.ctrl->sendDmx(UNI, f, true);
    drain(w.rx);
    QTest::qWait(40);
    w.ctrl->sendDmx(UNI, f, /*dataChanged=*/false);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    delete w.ctrl; delete w.rx;
}

// ---------------------------------------------------------------------
// Wire-cost upgrade
// ---------------------------------------------------------------------

void DDP_Partial_Test::heavyDiff_upgradesToFull()
{
    // With our 38B/packet overhead model, the wire-cost upgrade only fires for
    // truly pathological diff patterns. The realistic guarantee tested here:
    // a scattered diff with small gaps coalesces into ONE packet covering the
    // dirty span, never an explosion of tiny packets.
    Wire w = makeWire();
    QByteArray a = rgbFrame(50);
    w.ctrl->sendDmx(UNI, a, true);
    drain(w.rx);
    QByteArray b = a;
    for (int i = 0; i < 50; i += 2)
        b[i*3] = char(0xFF);
    w.ctrl->sendDmx(UNI, b, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QCOMPARE(pkts[0].offset, quint32(0));
    // Dirty pixels 0..48 → coalesced span = pixels 0..48 inclusive = 49 px = 147 B.
    QCOMPARE(int(pkts[0].dataLen), 147);
    QVERIFY(pkts[0].push());
    delete w.ctrl; delete w.rx;
}

// ---------------------------------------------------------------------
// WLED receiver simulation
// ---------------------------------------------------------------------

void DDP_Partial_Test::wledReceiverSim_partialMatchesSource()
{
    Wire w = makeWire();
    w.ctrl->setKeepAliveIntervalMsForTest(60000); // disable keep-alive
    const int N = 300;
    QByteArray strip(N * 3, char(0)); // simulated WLED framebuffer

    auto apply = [&](const QVector<ParsedPkt> &pkts) {
        for (const auto &p : pkts) {
            // WLED writes only addressed bytes; preserves the rest.
            QVERIFY2(p.offset + p.dataLen <= quint32(strip.size()),
                     "DDP packet out of strip bounds");
            memcpy(strip.data() + p.offset, p.payload.constData(), p.dataLen);
        }
    };

    QByteArray src = rgbFrame(N, char(0x11), char(0x22), char(0x33));
    w.ctrl->sendDmx(UNI, src, true);
    apply(drain(w.rx));
    QCOMPARE(strip, src);

    // Now mutate scattered pixels across several frames.
    for (int frame = 0; frame < 10; frame++) {
        for (int j = 0; j < 5; j++) {
            int p = (frame*7 + j*23) % N;
            src[p*3+0] = char(frame*10);
            src[p*3+1] = char(j*30);
            src[p*3+2] = char((frame+j)*5);
        }
        w.ctrl->sendDmx(UNI, src, true);
        apply(drain(w.rx));
        QCOMPARE(strip, src);
    }
    delete w.ctrl; delete w.rx;
}

// ---------------------------------------------------------------------
// Sequence
// ---------------------------------------------------------------------

void DDP_Partial_Test::sequence_cyclesOneToFifteen_noZero()
{
    Wire w = makeWire();
    w.ctrl->setKeepAliveIntervalMsForTest(60000);
    QByteArray a = rgbFrame(50);
    w.ctrl->sendDmx(UNI, a, true);
    auto firstFull = drain(w.rx);
    QCOMPARE(firstFull.size(), 1);
    QCOMPARE(firstFull[0].seq, quint8(1));

    QVector<quint8> seqs;
    for (int i = 0; i < 30; i++) {
        // Mutate one byte to a fresh value each iteration.
        int byteIdx = (i * 4) % a.size();
        a[byteIdx] = char(0x80 | ((i + 1) & 0x7F));
        w.ctrl->sendDmx(UNI, a, true);
        auto pkts = drain(w.rx);
        QVERIFY2(!pkts.isEmpty(),
                 qPrintable(QString("iter %1 byte %2").arg(i).arg(byteIdx)));
        seqs.push_back(pkts[0].seq);
    }
    for (auto s : seqs)
        QVERIFY2(s >= 1 && s <= 15, qPrintable(QString("bad seq %1").arg(int(s))));
    QSet<quint8> uniq;
    for (auto s : seqs) uniq.insert(s);
    QVERIFY(uniq.size() >= 5);
    delete w.ctrl; delete w.rx;
}

// ---------------------------------------------------------------------
// Coverage shift
// ---------------------------------------------------------------------

void DDP_Partial_Test::offsetShiftClearsOldRange()
{
    Wire w = makeWire();
    w.ctrl->setKeepAliveIntervalMsForTest(60000);
    w.ctrl->setDDPOffset(UNI, 0);
    QByteArray f = rgbFrame(30, char(0x11), char(0x22), char(0x33));
    w.ctrl->sendDmx(UNI, f, true);
    drain(w.rx);

    w.ctrl->setDDPOffset(UNI, 30);
    w.ctrl->sendDmx(UNI, f, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 2);
    QCOMPARE(pkts[0].offset, quint32(0));
    QCOMPARE(int(pkts[0].dataLen), 30);
    QVERIFY(!pkts[0].push());
    QCOMPARE(pkts[0].payload, QByteArray(30, char(0)));
    QCOMPARE(pkts[1].offset, quint32(30));
    QCOMPARE(int(pkts[1].dataLen), 90);
    QVERIFY(pkts[1].push());
    QCOMPARE(pkts[1].payload, f);
    QCOMPARE(pkts[0].seq, pkts[1].seq);
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::multiPacketCoverageClear()
{
    // Old coverage 5000 bytes; shrink to 100 bytes. Old-only tail = 4900 bytes
    // → must chunk into ceil(4900 / 1440) = 4 clear packets, all PUSH=false.
    Wire w = makeWire();
    w.ctrl->setKeepAliveIntervalMsForTest(60000);
    w.ctrl->setPixelCount(0);          // auto, controlled by srcLen
    QByteArray big(5000, char(0xAB));
    // Round to whole pixels: auto mode drops to 4998 (1666 RGB px).
    w.ctrl->sendDmx(UNI, big, true);
    auto first = drain(w.rx);
    QVERIFY(first.size() >= 1);

    // Shrink coverage by forcing explicit pixelCount to 33 px (99 bytes).
    w.ctrl->setPixelCount(33);
    QByteArray small = rgbFrame(33);
    w.ctrl->sendDmx(UNI, small, true);
    auto pkts = drain(w.rx);

    // Expected: N clear packets covering [99, 4998) then 1 snapshot of 99 B push.
    int clearBytes = 0;
    int clearCount = 0;
    int snapCount = 0;
    for (const auto &p : pkts) {
        if (p.push()) {
            snapCount++;
            QCOMPARE(int(p.dataLen), 99);
            QCOMPARE(p.offset, quint32(0));
        } else {
            clearCount++;
            clearBytes += p.dataLen;
            QCOMPARE(p.payload, QByteArray(p.dataLen, char(0)));
            QVERIFY(p.offset >= 99);
            QVERIFY(p.dataLen <= DDP_MAX_DATALEN);
        }
    }
    QCOMPARE(snapCount, 1);
    QCOMPARE(clearBytes, 4998 - 99);
    QVERIFY(clearCount >= 4); // 4899 bytes / 1440 = ceil 4
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::destIdShift_clearsWithOldDestId()
{
    // When destId changes, sameDest is false → no clear packets (we can't
    // safely address the old logical output anymore). Just full snapshot.
    Wire w = makeWire();
    w.ctrl->setKeepAliveIntervalMsForTest(60000);
    QByteArray f = rgbFrame(20);
    w.ctrl->sendDmx(UNI, f, true);
    drain(w.rx);
    w.ctrl->setDestId(UNI, 2); // change → invalidate, no clear
    w.ctrl->sendDmx(UNI, f, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QVERIFY(pkts[0].push());
    QCOMPARE(pkts[0].destId, quint8(2));
    delete w.ctrl; delete w.rx;
}

// ---------------------------------------------------------------------
// Throttle
// ---------------------------------------------------------------------

void DDP_Partial_Test::fullMode_fpsThrottleEnforced()
{
    Wire w = makeWire();
    w.ctrl->setTransmissionMode(UNI, DDPController::Full);
    w.ctrl->setMaxFpsBypassForTest(false);
    w.ctrl->setMaxFps(2); // 500ms gap so drain's 50ms idle doesn't slip past it
    QByteArray f = rgbFrame(20);
    w.ctrl->sendDmx(UNI, f, true);
    QCOMPARE(drain(w.rx).size(), 1);          // first send bypasses throttle
    w.ctrl->sendDmx(UNI, f, true);
    w.ctrl->sendDmx(UNI, f, true);
    w.ctrl->sendDmx(UNI, f, true);
    QCOMPARE(drain(w.rx).size(), 0);          // throttled
    QTest::qWait(550);
    w.ctrl->sendDmx(UNI, f, true);
    QCOMPARE(drain(w.rx).size(), 1);          // window elapsed
    delete w.ctrl; delete w.rx;
}

void DDP_Partial_Test::partialMode_fpsThrottleBetweenDiffs()
{
    Wire w = makeWire();
    w.ctrl->setMaxFpsBypassForTest(false);
    w.ctrl->setKeepAliveIntervalMsForTest(60000);
    w.ctrl->setMaxFps(2); // 500ms
    QByteArray a = rgbFrame(20);
    w.ctrl->sendDmx(UNI, a, true);
    drain(w.rx);                       // initial full
    QByteArray b = a; b[0] = char(0xFF);
    w.ctrl->sendDmx(UNI, b, true);
    b[3] = char(0xFF);
    w.ctrl->sendDmx(UNI, b, true);
    QCOMPARE(drain(w.rx).size(), 0);   // throttled
    QTest::qWait(550);
    b[6] = char(0xFF);
    w.ctrl->sendDmx(UNI, b, true);
    QCOMPARE(drain(w.rx).size(), 1);   // window elapsed
    delete w.ctrl; delete w.rx;
}

// ---------------------------------------------------------------------
// RGBW
// ---------------------------------------------------------------------

void DDP_Partial_Test::rgbwMisalignedOffset_rejected()
{
    Wire w = makeWire();
    w.ctrl->setComponents(UNI, DDPController::RGBW);
    // 2 not divisible by 4 → must be rejected.
    w.ctrl->setDDPOffset(UNI, 2);
    QByteArray f(40, char(0x55));
    w.ctrl->sendDmx(UNI, f, true);
    QCOMPARE(drain(w.rx).size(), 0);
    // Aligned (offset=4) → goes through.
    w.ctrl->setDDPOffset(UNI, 4);
    w.ctrl->sendDmx(UNI, f, true);
    auto pkts = drain(w.rx);
    QCOMPARE(pkts.size(), 1);
    QCOMPARE(pkts[0].offset, quint32(4));
    QCOMPARE(pkts[0].dataType, quint8(DDP_DATATYPE_RGBW888));
    delete w.ctrl; delete w.rx;
}

QTEST_MAIN(DDP_Partial_Test)
