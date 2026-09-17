/*
  Q Light Controller Plus
  ddpcontroller.cpp

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

#include <QDebug>
#include <QLoggingCategory>
#include <QVarLengthArray>
#include <QVector>
#include <algorithm>
#include <cstring>

#ifdef Q_OS_UNIX
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "ddpcontroller.h"

namespace {
QString normalizedAddressKey(const QHostAddress &address)
{
    if (address.isNull())
        return QStringLiteral("null");
    bool isV4 = false;
    const quint32 v4 = address.toIPv4Address(&isV4);
    if (isV4)
        return QStringLiteral("v4:%1").arg(v4);
    const Q_IPV6ADDR v6 = address.toIPv6Address();
    return QStringLiteral("v6:%1").arg(QString::fromLatin1(
        QByteArray(reinterpret_cast<const char *>(v6.c), 16).toHex()));
}
}

// DDP UDP write failures (interface flap, no listener, blocked datagram) are
// expected at runtime and must NOT spam qWarning. Filter via
// QT_LOGGING_RULES="qlcplus.plugins.ddp.debug=true" to inspect.
Q_LOGGING_CATEGORY(ddpLog, "qlcplus.plugins.ddp")

DDPController::DDPController(QNetworkInterface const& iface,
                             QNetworkAddressEntry const& address,
                             quint32 line, QObject *parent)
    : QObject(parent)
    , m_ipAddr(address.ip())
    , m_line(line)
{
    Q_UNUSED(iface)

    m_udpSocket.reset(new QUdpSocket());  // no QObject parent — QSharedPointer owns it
    if (!m_udpSocket->bind(m_ipAddr, 0))
    {
        qWarning() << "[DDP] Failed to bind UDP socket on" << m_ipAddr.toString()
                   << ":" << m_udpSocket->errorString();
    }

    // Monotonic clock for FPS throttling and keep-alive.
    m_sendTimer.start();

    qCDebug(ddpLog) << "Controller created on" << m_ipAddr.toString();
}

DDPController::~DDPController()
{
    qCDebug(ddpLog) << "Controller destroyed on" << m_ipAddr.toString();
    m_udpSocket->close();
}

// =====================================================================
//  DDP send path — WLED firmware compatibility notes
// =====================================================================
//
// All wire-level decisions here are deliberately matched to the WLED
// firmware's DDP receive path so a QLC+ Partial-mode stream renders
// identically to a full-snapshot stream. References use upstream WLED
// branch `main`, files wled00/e131.cpp and wled00/udp.cpp.
//
//  WLED reference                       │ enforced in QLC+
//  ─────────────────────────────────────┼────────────────────────────────
//  Header layout: flags|seq|type|destId │ DDPPacketizer::writePacketInPlace
//  + offset(BE32 bytes) + len(BE16)     │   (ddppacketizer.cpp)
//  see e131.cpp handleDDPPacket header  │
//  parsing.                             │
//                                       │
//  destId ∈ {246,250,251} rejected as   │ sendDmx() pre-flight rejects
//  pixel writes (CONTROL/CONFIG/STATUS).│   identical destIds, logs &
//  e131.cpp early-returns these.        │   returns.
//                                       │
//  bpp = 4 iff ((dataType>>3)&0b111) == │ DDPController emits 0x1B for
//  0b011, else 3.                       │   RGBW, 0x0B for RGB — both
//  e131.cpp `ddpChannelsPerLed`.        │   satisfy the bit test.
//                                       │
//  dataOffset is BYTES; receiver        │ sendDmx() rejects ddpOffset
//  computes startPixel = offset/bpp.    │   not divisible by bpp; partial
//  Misaligned offsets silently floor.   │   runs are bpp-aligned.
//                                       │
//  dataLen is BYTES; receiver rejects   │ partial diff scan strides by
//  packets where numLeds*bpp > dataLen  │   bpp, so emitted dataLen is
//  (i.e. non-multiple of bpp).          │   always a multiple of bpp.
//                                       │
//  Max payload 1440 B (≈480 RGB px /    │ DDP_MAX_DATALEN = 1440; every
//  360 RGBW px). e131.cpp's union has   │   chunk loop caps at 1440.
//  1458 B raw buffer (10 hdr + 1448).   │
//                                       │
//  Realtime session: udp.cpp's          │ Partial mode REQUIRES this
//  realtimeLock() clears the strip ONCE │   behavior — the first frame
//  on entry (`strip.fill(BLACK)` if not │   after any identity change is
//  already in realtimeMode), then       │   a full snapshot, then diffs
//  PRESERVES un-addressed pixels.       │   only update the dirty range.
//                                       │
//  PUSH flag: `ddpSeenPush` latches on  │ ONE sequence per emitted destination
//  first PUSH; thereafter only PUSH     │   burst. PUSH is set only on the
//  triggers strip.show(). Pre-PUSH,     │   final packet of the burst (or a
//  every packet triggers show().        │   standalone len=0 PUSH when the
//                                       │   terminal completer had no payload).
//                                       │
//  Render rate-cap: udp.cpp limits      │ Our max FPS cap is 50; well
//  show() to ~66 fps (15 ms gap).       │   under WLED's render rate cap.
//                                       │
//  Realtime timeout: udp.cpp uses       │ Partial keep-alive default
//  realtimeTimeoutMs (~2500 ms WLED     │   1000 ms, capped 2400 ms in
//  default). If no DDP arrives in       │   setKeepAliveIntervalMs to
//  that window, exitRealtime() runs     │   stay safely under the WLED
//  and the strip leaves DDP mode.       │   timeout even on lossy links.
//                                       │
//  Sequence: low 4 bits, 1..15; 0 is    │ DDPPacketizer::sequenceForFrame
//  "unused". Out-of-order reject window │   never returns 0; cycles 1..15.
//  is optional in WLED (off by default).│
// =====================================================================

void DDPController::sendDmx(quint32 universe, const QByteArray &data, bool dataChanged)
{
    QMutexLocker locker(&m_dataMutex);

    // T9: one-shot socket tuning on first send
    if (Q_UNLIKELY(!m_socketTuned))
    {
        m_socketTuned = true;
        qintptr fd = m_udpSocket->socketDescriptor();
        if (fd >= 0)
        {
#ifdef Q_OS_UNIX
            int sndbuf = 1 << 20; // 1 MB
            setsockopt(static_cast<int>(fd), SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
            int tos = 0xB8; // DSCP EF — WMM Voice priority on Wi-Fi
            setsockopt(static_cast<int>(fd), IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
#endif
#ifdef Q_OS_WIN
            int sndbuf = 1 << 20;
            setsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_SNDBUF,
                       reinterpret_cast<const char*>(&sndbuf), sizeof(sndbuf));
            int tos = 0xB8;
            setsockopt(static_cast<SOCKET>(fd), IPPROTO_IP, IP_TOS,
                       reinterpret_cast<const char*>(&tos), sizeof(tos));
#endif
            qCDebug(ddpLog) << "Socket tuned: SO_SNDBUF=1MB, IP_TOS=0xB8";
        }
    }

    // T10: single map lookup with find()
    auto it = m_universeMap.find(universe);
    if (it == m_universeMap.end())
    {
        qWarning() << Q_FUNC_INFO << "universe" << universe << "unknown";
        return;
    }
    DDPUniverseInfo &info = it.value();

    const qint64 now = m_sendTimer.elapsed();
    const QString previousDestKey = info.participatesInRefresh ? info.refreshDestKey : QString();

    auto dropFromRefresh = [&]() {
        info.hasPendingReport = false;
        info.pendingData.clear();
        info.pendingTotalLen = 0;
        info.pendingBpp = 0;
        if (info.participatesInRefresh)
        {
            info.participatesInRefresh = false;
            info.refreshDestKey.clear();
            if (!previousDestKey.isEmpty())
                maybeEmitDestination(previousDestKey, now, -1, /*allowFallback=*/false);
        }
    };

    // ---- Pre-flight validation (apply to both Full and Partial paths) ----
    if (info.destAddress.isNull())
    {
        qCDebug(ddpLog) << "sendDmx: skipping universe" << universe
                        << "no destination IP configured";
        dropFromRefresh();
        return;
    }
    // WLED firmware rejects these reserved destIds as pixel writes:
    // wled00/e131.cpp handleDDPPacket():
    //   if (p->destination == DDP_ID_CONTROL
    //    || p->destination == DDP_ID_STATUS
    //    || p->destination == DDP_ID_CONFIG) return;
    if (info.destId == 246 || info.destId == 250 || info.destId == 251)
    {
        qCDebug(ddpLog) << "sendDmx: refusing reserved destId" << info.destId
                        << "for universe" << universe;
        dropFromRefresh();
        return;
    }

    // ---- Pixel framing ---------------------------------------------------
    const int bpp = (info.components == RGBW) ? 4 : 3;
    const int srcLen = data.size();
    int totalLen = 0;
    if (m_pixelCount > 0)
        totalLen = m_pixelCount * bpp;            // explicit: zero-pad if needed
    else
        totalLen = (srcLen / bpp) * bpp;          // auto: whole pixels only

    // ddpOffset must be a whole pixel — WLED divides by bpp internally:
    // wled00/e131.cpp handleDDPPacket():
    //   uint32_t start = htonl(p->channelOffset) / ddpChannelsPerLed;
    // A non-aligned offset would silently floor on the receiver.
    const bool alignedOffset = ((info.ddpOffset % static_cast<quint32>(bpp)) == 0);
    const bool eligible = alignedOffset && totalLen > 0;

    if (!eligible)
    {
        if (!alignedOffset)
        {
            qCDebug(ddpLog) << "sendDmx: ddpOffset" << info.ddpOffset
                            << "not aligned to bpp" << bpp << "for universe" << universe;
        }

        dropFromRefresh();
        return;
    }

    const quint8 dataType = (info.components == RGBW)
        ? DDP_DATATYPE_RGBW888
        : DDP_DATATYPE_RGB888;

    QByteArray prepared(totalLen, char(0));
    if (srcLen > 0)
        memcpy(prepared.data(), data.constData(), qMin(srcLen, totalLen));

    const QString currentDestKey = destinationKey(info);

    if (info.participatesInRefresh && previousDestKey != currentDestKey)
    {
        info.participatesInRefresh = false;
        info.hasPendingReport = false;
        info.refreshDestKey.clear();
        maybeEmitDestination(previousDestKey, now, -1, /*allowFallback=*/false);
    }

    info.participatesInRefresh = true;
    info.refreshDestKey = currentDestKey;
    info.hasPendingReport = true;
    info.pendingDataChanged = dataChanged;
    info.pendingData = std::move(prepared);
    info.pendingTotalLen = totalLen;
    info.pendingBpp = bpp;
    info.pendingDataType = dataType;

    DestinationCycleState &cycle = m_destinationCycles[currentDestKey];
    if (cycle.lastReportAt > 0 && now > cycle.lastReportAt)
        cycle.tickPeriodMs = qMax<qint64>(1, now - cycle.lastReportAt);
    cycle.lastReportAt = now;
    if (!cycle.active)
    {
        cycle.active = true;
        cycle.startedAt = now;
        cycle.activeTickPeriodMs = qMax<qint64>(1, cycle.tickPeriodMs);
        if (cycle.cycle == 0)
            cycle.cycle = 1;
    }

    info.reportedCycle = cycle.cycle;

    maybeEmitDestination(currentDestKey, now, static_cast<qint32>(universe), /*allowFallback=*/true);
}

QString DDPController::destinationKey(const DDPUniverseInfo &info) const
{
    return QStringLiteral("%1|%2|%3")
        .arg(normalizedAddressKey(info.destAddress))
        .arg(info.destPort)
        .arg(info.destId);
}

QString DDPController::endpointKey(const DDPUniverseInfo &info) const
{
    return QStringLiteral("%1|%2")
        .arg(normalizedAddressKey(info.destAddress))
        .arg(info.destPort);
}

quint8 DDPController::nextEndpointSequence(const DDPUniverseInfo &info)
{
    const QString key = endpointKey(info);
    quint64 &frame = m_endpointFrameCounts[key];
    frame++;
    return DDPPacketizer::sequenceForFrame(frame);
}

void DDPController::maybeEmitDestination(const QString &destKey, qint64 now,
                                         qint32 preferredFinalUniverse, bool allowFallback)
{
    auto stateIt = m_destinationCycles.find(destKey);
    if (stateIt == m_destinationCycles.end() || !stateIt.value().active)
        return;

    auto isDestinationMember = [&](const DDPUniverseInfo &info) -> bool {
        if (info.destAddress.isNull())
            return false;
        if (destinationKey(info) != destKey)
            return false;
        if (info.destId == 246 || info.destId == 250 || info.destId == 251)
            return false;
        const int bpp = (info.components == RGBW) ? 4 : 3;
        if ((info.ddpOffset % static_cast<quint32>(bpp)) != 0)
            return false;
        return true;
    };

    int participants = 0;
    int reported = 0;
    for (auto it = m_universeMap.constBegin(); it != m_universeMap.constEnd(); ++it)
    {
        const DDPUniverseInfo &info = it.value();
        if (!isDestinationMember(info))
            continue;
        participants++;
        if (info.reportedCycle == stateIt.value().cycle)
            reported++;
    }

    if (participants == 0)
    {
        stateIt.value().active = false;
        stateIt.value().startedAt = 0;
        return;
    }

    const bool allReported = (reported == participants);
    bool fallbackDue = false;
    if (!allReported && allowFallback)
    {
        const qint64 tick = qMax<qint64>(1, stateIt.value().activeTickPeriodMs);
        const qint64 fallbackWindow = qMax<qint64>(2, tick * kFallbackTickPeriods);
        fallbackDue = (now - stateIt.value().startedAt) >= fallbackWindow;
    }

    if (allReported || fallbackDue)
        emitDestination(destKey, now, preferredFinalUniverse);
}

void DDPController::emitDestination(const QString &destKey, qint64 now, qint32 preferredFinalUniverse)
{
    auto stateIt = m_destinationCycles.find(destKey);
    if (stateIt == m_destinationCycles.end() || !stateIt.value().active)
        return;

    const quint64 cycleId = stateIt.value().cycle;

    QVector<quint32> universes;
    universes.reserve(m_universeMap.size());
    for (auto it = m_universeMap.constBegin(); it != m_universeMap.constEnd(); ++it)
    {
        const DDPUniverseInfo &info = it.value();
        if (!info.participatesInRefresh || info.refreshDestKey != destKey)
            continue;
        if (destinationKey(info) != destKey)
            continue;
        if (info.reportedCycle != cycleId || !info.hasPendingReport)
            continue;
        universes.push_back(it.key());
    }

    std::sort(universes.begin(), universes.end());

    qint32 finalUniverse = -1;
    for (quint32 uni : universes)
    {
        if (static_cast<qint32>(uni) == preferredFinalUniverse)
        {
            finalUniverse = preferredFinalUniverse;
            break;
        }
    }
    if (finalUniverse < 0 && !universes.isEmpty())
        finalUniverse = static_cast<qint32>(universes.back());

    bool seqAssigned = false;
    quint8 seq = 1;
    bool anyPackets = false;
    bool anyFailure = false;
    qint32 lastEmitterUniverse = -1;
    bool finalUniverseEmitted = false;

    auto emitOne = [&](quint32 uni, bool allowPush, bool isFinal) {
        auto uit = m_universeMap.find(uni);
        if (uit == m_universeMap.end())
            return;
        EmitResult r = emitUniverse(uit.value(), now, seq, seqAssigned, allowPush);
        anyPackets = anyPackets || r.emittedPackets;
        anyFailure = anyFailure || !r.ok;
        if (r.emittedPackets)
            lastEmitterUniverse = static_cast<qint32>(uni);
        if (isFinal)
            finalUniverseEmitted = r.emittedPackets;
    };

    for (quint32 uni : universes)
    {
        if (static_cast<qint32>(uni) == finalUniverse)
            continue;
        emitOne(uni, /*allowPush=*/false, /*isFinal=*/false);
    }
    if (finalUniverse >= 0)
        emitOne(static_cast<quint32>(finalUniverse), /*allowPush=*/true, /*isFinal=*/true);

    if (seqAssigned && anyPackets && !anyFailure && !finalUniverseEmitted && lastEmitterUniverse >= 0)
    {
        auto emitterIt = m_universeMap.find(static_cast<quint32>(lastEmitterUniverse));
        if (emitterIt != m_universeMap.end())
            (void) sendPushOnly(emitterIt.value(), seq);
    }

    for (auto it = m_universeMap.begin(); it != m_universeMap.end(); ++it)
    {
        DDPUniverseInfo &info = it.value();
        if (!info.participatesInRefresh || info.refreshDestKey != destKey)
            continue;
        if (info.reportedCycle == cycleId)
            info.hasPendingReport = false;
    }

    stateIt.value().active = false;
    stateIt.value().startedAt = 0;
    stateIt.value().cycle++;
}

DDPController::EmitResult DDPController::emitUniverse(DDPUniverseInfo &info, qint64 now,
                                                       quint8 &seq, bool &seqAssigned, bool allowPush)
{
    const bool partialMode = (info.transmissionMode == Partial);

    bool mustFull = false;
    if (partialMode)
    {
        mustFull = !info.baselineValid
            || info.lastCoverageLen != info.pendingTotalLen
            || info.lastBpp != info.pendingBpp
            || info.lastDestAddress != info.destAddress
            || info.lastDestPort != info.destPort
            || info.lastDestId != info.destId
            || info.lastDdpOffset != info.ddpOffset
            || (now - info.lastFullFrameElapsed) >= m_keepAliveMs;
    }

    if (!(partialMode && mustFull) && !m_maxFpsBypassForTest && info.lastSendElapsed != 0)
    {
        const qint64 minInterval = (m_maxFps > 0) ? (1000 / m_maxFps) : 0;
        if (minInterval > 0 && (now - info.lastSendElapsed) < minInterval)
            return EmitResult{};
    }

    if (!partialMode && m_skipUnchanged && !info.pendingDataChanged
            && (now - info.lastSendDataElapsed) < kKeepAliveMs)
        return EmitResult{};

    if (partialMode && !mustFull && info.lastSentData.size() == info.pendingTotalLen
            && memcmp(info.pendingData.constData(), info.lastSentData.constData(), info.pendingTotalLen) == 0)
        return EmitResult{};

    if (!seqAssigned)
    {
        seq = nextEndpointSequence(info);
        seqAssigned = true;
    }

    bool emitted = false;
    const char *srcData = info.pendingData.constData();
    const bool wantsFullSnapshot = !partialMode || mustFull;
    const bool ok = wantsFullSnapshot
        ? sendFullSnapshot(info, srcData, info.pendingTotalLen, info.pendingBpp, info.pendingDataType,
                           now, seq, allowPush, &emitted)
        : sendPartialDiff(info, srcData, info.pendingTotalLen, info.pendingBpp, info.pendingDataType,
                          now, seq, allowPush, &emitted);
    return EmitResult{emitted, ok};
}

bool DDPController::sendPushOnly(const DDPUniverseInfo &info, quint8 seq)
{
    const quint8 dataType = (info.components == RGBW)
        ? DDP_DATATYPE_RGBW888
        : DDP_DATATYPE_RGB888;
    return writeChunk(info, nullptr, 0, 0, 0, info.ddpOffset, seq, /*push=*/true, dataType);
}

// =====================================================================
//  Wire-level helpers
// =====================================================================

bool DDPController::writeChunk(const DDPUniverseInfo &info,
                               const char *srcData, int srcLen,
                               int chunkStart, int chunkLen,
                               quint32 dataOffset, quint8 seq, bool push,
                               quint8 dataType)
{
    char buf[DDP_HEADER_LEN + DDP_MAX_DATALEN];
    DDPPacketizer::writePacketInPlace(
        buf, srcData, srcLen,
        chunkStart, chunkLen,
        dataOffset, seq, push, dataType, info.destId);
    qint64 sent = m_udpSocket->writeDatagram(
        buf, DDP_HEADER_LEN + chunkLen, info.destAddress, info.destPort);
    if (sent < 0)
    {
        qCDebug(ddpLog) << "writeChunk: writeDatagram failed:"
                        << m_udpSocket->errorString();
        return false;
    }
    m_packetSent++;
    return true;
}

bool DDPController::sendFullSnapshot(DDPUniverseInfo &info, const char *srcData,
                                     int totalLen, int bpp, quint8 dataType,
                                     qint64 now, quint8 seq, bool allowPush,
                                     bool *emittedPackets)
{
    if (emittedPackets)
        *emittedPackets = false;

    // Capture old identity BEFORE we mutate it, so we can clear an old-only
    // tail/head if coverage shifted on this same destination.
    // NOTE: we use lastBpp>0 (set on first successful send, never cleared by
    // invalidateBaseline) — NOT baselineValid — because baselineValid is
    // about diff-correctness, while coverage-clear cares about wire history.
    const bool hadBaseline   = (info.lastBpp > 0);
    const quint32 oldOffset  = info.lastDdpOffset;
    const int    oldCoverage = info.lastCoverageLen;
    const int    oldBpp      = info.lastBpp;
    const QHostAddress oldAddr = info.lastDestAddress;
    const quint16 oldPort      = info.lastDestPort;

    // Coverage-clear: only safe when destination IDENTITY is unchanged. The
    // old-only byte range must be addressed at the OLD bpp/destId because that
    // is how the receiver previously interpreted those bytes.
    //
    // Required because WLED preserves un-addressed pixels indefinitely within
    // a session (wled00/udp.cpp realtimeLock only clears on session entry).
    // Without explicit zero-clears, shrinking the coverage would leave the
    // "abandoned" pixels lit at their last colour until the realtime timeout.
    bool ok = true;
    const bool sameDest = hadBaseline
        && oldAddr == info.destAddress
        && oldPort == info.destPort
        && info.lastDestId == info.destId;     // destId may have changed via setter
    if (sameDest && oldBpp > 0 && oldCoverage > 0)
    {
        // Compute old-only intervals = old_range \ new_range.
        const quint64 oldStart = oldOffset;
        const quint64 oldEnd   = oldStart + static_cast<quint64>(oldCoverage);
        const quint64 newStart = info.ddpOffset;
        const quint64 newEnd   = newStart + static_cast<quint64>(totalLen);

        struct Iv { quint64 s, e; };
        Iv ivs[2];
        int n = 0;
        if (newStart > oldStart)
            ivs[n++] = { oldStart, qMin(oldEnd, newStart) };
        if (newEnd < oldEnd)
            ivs[n++] = { qMax(oldStart, newEnd), oldEnd };
        const quint8 oldDataType = (oldBpp == 4)
            ? DDP_DATATYPE_RGBW888 : DDP_DATATYPE_RGB888;
        // One reusable zero-chunk scratch (≤ DDP_MAX_DATALEN) avoids large
        // allocations on big coverage shrinks.
        static char zeroChunk[DDP_MAX_DATALEN] = {0};
        for (int k = 0; k < n && ok; k++)
        {
            const int len = static_cast<int>(ivs[k].e - ivs[k].s);
            if (len <= 0) continue;
            int sent = 0;
            while (sent < len && ok)
            {
                const int chunk = qMin(DDP_MAX_DATALEN, len - sent);
                ok = writeChunk(info, zeroChunk, chunk, 0, chunk,
                                static_cast<quint32>(ivs[k].s) + static_cast<quint32>(sent),
                                seq, /*push=*/false, oldDataType);
                if (ok && emittedPackets)
                    *emittedPackets = true;
                sent += chunk;
            }
        }
    }

    // Full snapshot for one universe. allowPush is true only on the burst's
    // designated terminal universe.
    //
    // PUSH gating in WLED: wled00/e131.cpp handleDDPPacket() maintains
    // `static bool ddpSeenPush` per realtime session. Once any PUSH arrives,
    // only PUSH packets trigger e131NewData (and thus strip.show()). Grouped
    // emission keeps PUSH only on the destination burst's terminal packet.
    const int totalPackets = DDPPacketizer::packetsRequired(totalLen);
    for (int i = 0; i < totalPackets; i++)
    {
        const int chunkStart = i * DDP_MAX_DATALEN;
        const int chunkLen   = qMin(DDP_MAX_DATALEN, totalLen - chunkStart);
        const bool push      = allowPush && (i == totalPackets - 1);
        ok = writeChunk(info, srcData, totalLen, chunkStart, chunkLen,
                        info.ddpOffset + static_cast<quint32>(chunkStart),
                        seq, push, dataType) && ok;
        if (ok && emittedPackets)
            *emittedPackets = true;
        if (!ok) break;
    }

    // qMax(now,1) avoids the 0 sentinel meaning "never sent".
    info.lastSendElapsed = qMax<qint64>(now, 1);
    info.lastSendDataElapsed = info.lastSendElapsed;
    if (!ok)
    {
        // Keep the just-bumped lastSendElapsed (so we don't tight-loop retry),
        // but force baseline invalidation so the next attempt re-snapshots.
        info.baselineValid = false;
        return false;
    }

    // Commit baseline (all-or-nothing).
    info.lastSentData = QByteArray(srcData, totalLen);
    info.lastFullFrameElapsed = now;
    info.baselineValid     = true;
    info.lastDestAddress   = info.destAddress;
    info.lastDestPort      = info.destPort;
    info.lastDestId        = info.destId;
    info.lastDdpOffset     = info.ddpOffset;
    info.lastCoverageLen   = totalLen;
    info.lastBpp           = bpp;
    return true;
}

bool DDPController::sendPartialDiff(DDPUniverseInfo &info, const char *srcData,
                                    int totalLen, int bpp, quint8 dataType,
                                    qint64 now, quint8 seq, bool allowPush,
                                    bool *emittedPackets)
{
    if (emittedPackets)
        *emittedPackets = false;

    // Defensive: baseline must match totalLen (mustFull guards this, but check).
    if (info.lastSentData.size() != totalLen)
        return sendFullSnapshot(info, srcData, totalLen, bpp, dataType, now,
                                seq, allowPush, emittedPackets);

    const char *baseData = info.lastSentData.constData();

    // 1) Scan for dirty pixel runs (aligned to bpp).
    struct Run { int start; int end; }; // [start, end), bytes
    QVarLengthArray<Run, 16> runs;
    int p = 0;
    while (p < totalLen)
    {
        // skip clean pixels
        while (p < totalLen && memcmp(srcData + p, baseData + p, bpp) == 0)
            p += bpp;
        if (p >= totalLen) break;
        const int runStart = p;
        // extend over consecutive dirty pixels
        while (p < totalLen && memcmp(srcData + p, baseData + p, bpp) != 0)
            p += bpp;
        runs.append({ runStart, p });
    }

    // 2) Coalesce neighbouring runs whose gap is ≤ kPartialMinGapBytes
    //    (saves per-packet overhead). Gap is already bpp-aligned.
    {
        int w = 0;
        for (int r = 0; r < runs.size(); r++)
        {
            if (w > 0 && (runs[r].start - runs[w - 1].end) <= kPartialMinGapBytes)
                runs[w - 1].end = runs[r].end;
            else
                runs[w++] = runs[r];
        }
        runs.resize(w);
    }

    // 3) Nothing to send → no packets. Do NOT bump send timestamps
    //    (so a real change next call goes out immediately at full FPS).
    if (runs.isEmpty())
        return true;

    // 4) Wire-cost comparison. Per packet overhead ≈ 10 (DDP) + 28 (IPv4/UDP) = 38.
    //    Plus payload bytes. If partial ≥ full estimate, upgrade to full snapshot.
    auto packetCost = [](int payloadBytes) -> int {
        int pkts = DDPPacketizer::packetsRequired(payloadBytes);
        if (pkts == 0) pkts = 1; // standalone PUSH still costs 1 packet
        return pkts * 38 + payloadBytes;
    };
    int partialCost = 0;
    for (auto &r : runs) { partialCost += packetCost(r.end - r.start); }
    const int fullCost = packetCost(totalLen);
    if (partialCost >= fullCost)
        return sendFullSnapshot(info, srcData, totalLen, bpp, dataType, now,
                                seq, allowPush, emittedPackets);

    // 5) Emit packets for one universe. Grouped emission keeps one sequence
    //    for the destination burst and enables PUSH only on the terminal send.
    //
    //    WLED expects same-seq packets ending with one PUSH, which triggers
    //    strip.show() (wled00/e131.cpp handleDDPPacket sets e131NewData on
    //    the PUSH).
    bool ok = true;
    for (int r = 0; r < runs.size(); r++)
    {
        const int runLen = runs[r].end - runs[r].start;
        int sent = 0;
        while (sent < runLen)
        {
            const int chunk = qMin(DDP_MAX_DATALEN, runLen - sent);
            const bool lastChunk = (sent + chunk >= runLen);
            const bool lastRun   = (r == runs.size() - 1);
            const bool push      = allowPush && lastChunk && lastRun;
            const int  absStart  = runs[r].start + sent;
            ok = writeChunk(info, srcData, totalLen, absStart, chunk,
                            info.ddpOffset + static_cast<quint32>(absStart),
                            seq, push, dataType) && ok;
            if (ok && emittedPackets)
                *emittedPackets = true;
            if (!ok) break;
            sent += chunk;
        }
        if (!ok) break;
    }

    info.lastSendElapsed = qMax<qint64>(now, 1);
    info.lastSendDataElapsed = info.lastSendElapsed;
    if (!ok)
    {
        // Force a full retry next call.
        info.baselineValid = false;
        return false;
    }

    // 6) Commit: update baseline only for the regions we actually sent.
    for (auto &r : runs)
        memcpy(info.lastSentData.data() + r.start, srcData + r.start, r.end - r.start);
    return true;
}

void DDPController::invalidateBaseline(quint32 universe)
{
    auto it = m_universeMap.find(universe);
    if (it == m_universeMap.end()) return;
    it.value().baselineValid = false;
    // Force a keep-alive immediately on the next eligible call.
    it.value().lastFullFrameElapsed = 0;
}

void DDPController::invalidateAllBaselines()
{
    for (auto it = m_universeMap.begin(); it != m_universeMap.end(); ++it)
    {
        it.value().baselineValid = false;
        it.value().lastFullFrameElapsed = 0;
    }
}

QString DDPController::getNetworkIP() const
{
    return m_ipAddr.toString();
}

void DDPController::addUniverse(quint32 universe)
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_universeMap.contains(universe))
    {
        DDPUniverseInfo info;
        // Unicast by default: null = "user must configure".
        // sendDmx() will skip silently (logged via qlcplus.plugins.ddp).
        info.destAddress = QHostAddress();
        info.destPort = DDP_PORT;
        info.destId = DDP_DEST_DEFAULT;
        info.ddpOffset = 0;
        info.transmissionMode = Full;
        info.components = RGB;
        m_universeMap[universe] = info;
    }
}

void DDPController::removeUniverse(quint32 universe)
{
    QMutexLocker locker(&m_dataMutex);
    m_universeMap.remove(universe);
}

QList<quint32> DDPController::universesList() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_universeMap.keys();
}

DDPUniverseInfo DDPController::getUniverseInfo(quint32 universe, bool *found) const
{
    QMutexLocker locker(&m_dataMutex);
    auto it = m_universeMap.constFind(universe);
    if (it == m_universeMap.constEnd())
    {
        if (found) *found = false;
        return DDPUniverseInfo{};
    }
    if (found) *found = true;
    return it.value();
}

void DDPController::setDestAddress(quint32 universe, const QString &address)
{
    QMutexLocker locker(&m_dataMutex);
    auto it = m_universeMap.find(universe);
    if (it == m_universeMap.end()) return;
    const QString previousDestKey = it.value().participatesInRefresh ? it.value().refreshDestKey : QString();
    it.value().destAddress = QHostAddress(address);
    it.value().participatesInRefresh = false;
    it.value().refreshDestKey.clear();
    it.value().hasPendingReport = false;
    it.value().pendingData.clear();
    it.value().pendingTotalLen = 0;
    it.value().pendingBpp = 0;
    if (!previousDestKey.isEmpty())
        maybeEmitDestination(previousDestKey, m_sendTimer.elapsed(), -1, /*allowFallback=*/false);
    invalidateBaseline(universe);
}

void DDPController::setDestPort(quint32 universe, quint16 port)
{
    QMutexLocker locker(&m_dataMutex);
    auto it = m_universeMap.find(universe);
    if (it == m_universeMap.end()) return;
    const QString previousDestKey = it.value().participatesInRefresh ? it.value().refreshDestKey : QString();
    it.value().destPort = port;
    it.value().participatesInRefresh = false;
    it.value().refreshDestKey.clear();
    it.value().hasPendingReport = false;
    it.value().pendingData.clear();
    it.value().pendingTotalLen = 0;
    it.value().pendingBpp = 0;
    if (!previousDestKey.isEmpty())
        maybeEmitDestination(previousDestKey, m_sendTimer.elapsed(), -1, /*allowFallback=*/false);
    invalidateBaseline(universe);
}

void DDPController::setDestId(quint32 universe, quint8 id)
{
    QMutexLocker locker(&m_dataMutex);
    auto it = m_universeMap.find(universe);
    if (it == m_universeMap.end()) return;
    const QString previousDestKey = it.value().participatesInRefresh ? it.value().refreshDestKey : QString();
    it.value().destId = id;
    it.value().participatesInRefresh = false;
    it.value().refreshDestKey.clear();
    it.value().hasPendingReport = false;
    it.value().pendingData.clear();
    it.value().pendingTotalLen = 0;
    it.value().pendingBpp = 0;
    if (!previousDestKey.isEmpty())
        maybeEmitDestination(previousDestKey, m_sendTimer.elapsed(), -1, /*allowFallback=*/false);
    invalidateBaseline(universe);
}

void DDPController::setDDPOffset(quint32 universe, quint32 offset)
{
    QMutexLocker locker(&m_dataMutex);
    auto it = m_universeMap.find(universe);
    if (it == m_universeMap.end()) return;
    const QString previousDestKey = it.value().participatesInRefresh ? it.value().refreshDestKey : QString();
    it.value().ddpOffset = offset;
    it.value().participatesInRefresh = false;
    it.value().refreshDestKey.clear();
    it.value().hasPendingReport = false;
    it.value().pendingData.clear();
    it.value().pendingTotalLen = 0;
    it.value().pendingBpp = 0;
    if (!previousDestKey.isEmpty())
        maybeEmitDestination(previousDestKey, m_sendTimer.elapsed(), -1, /*allowFallback=*/false);
    invalidateBaseline(universe);
}

void DDPController::setTransmissionMode(quint32 universe, TransmissionMode mode)
{
    QMutexLocker locker(&m_dataMutex);
    auto it = m_universeMap.find(universe);
    if (it == m_universeMap.end()) return;
    const QString previousDestKey = it.value().participatesInRefresh ? it.value().refreshDestKey : QString();
    it.value().transmissionMode = mode;
    it.value().participatesInRefresh = false;
    it.value().refreshDestKey.clear();
    it.value().hasPendingReport = false;
    it.value().pendingData.clear();
    it.value().pendingTotalLen = 0;
    it.value().pendingBpp = 0;
    if (!previousDestKey.isEmpty())
        maybeEmitDestination(previousDestKey, m_sendTimer.elapsed(), -1, /*allowFallback=*/false);
    invalidateBaseline(universe);
}

void DDPController::setComponents(quint32 universe, Components components)
{
    QMutexLocker locker(&m_dataMutex);
    auto it = m_universeMap.find(universe);
    if (it == m_universeMap.end()) return;
    const QString previousDestKey = it.value().participatesInRefresh ? it.value().refreshDestKey : QString();
    it.value().components = components;
    it.value().participatesInRefresh = false;
    it.value().refreshDestKey.clear();
    it.value().hasPendingReport = false;
    it.value().pendingData.clear();
    it.value().pendingTotalLen = 0;
    it.value().pendingBpp = 0;
    if (!previousDestKey.isEmpty())
        maybeEmitDestination(previousDestKey, m_sendTimer.elapsed(), -1, /*allowFallback=*/false);
    invalidateBaseline(universe);
}

QString DDPController::transmissionModeToString(TransmissionMode mode)
{
    switch (mode)
    {
        case Full: return "Full";
        case Partial: return "Partial";
    }
    return "Full";
}

DDPController::TransmissionMode DDPController::stringToTransmissionMode(const QString &mode)
{
    if (mode == "Partial")
        return Partial;
    return Full;
}

QString DDPController::componentsToString(Components components)
{
    switch (components)
    {
        case RGBW: return "RGBW";
        default: return "RGB";
    }
}

DDPController::Components DDPController::stringToComponents(const QString &str)
{
    if (str == "RGBW")
        return RGBW;
    return RGB;
}

quint32 DDPController::line() const
{
    return m_line;
}

quint64 DDPController::getPacketSentNumber() const
{
    return m_packetSent;
}

void DDPController::setMaxFps(int fps)
{
    QMutexLocker locker(&m_dataMutex);
    m_maxFps = qBound(0, fps, kMaxFpsLimit);
}

int DDPController::maxFps() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_maxFps;
}

void DDPController::setPixelCount(int pixels)
{
    QMutexLocker locker(&m_dataMutex);
    const int v = qMax(0, pixels);
    if (v == m_pixelCount) return;
    m_pixelCount = v;
    // pixelCount is controller-wide — every universe baseline becomes stale.
    invalidateAllBaselines();
    // Pending payloads were framed with the old count. Like per-universe
    // configuration changes, require fresh reports before publishing them.
    for (auto it = m_universeMap.begin(); it != m_universeMap.end(); ++it)
    {
        it.value().hasPendingReport = false;
        it.value().reportedCycle = 0;
        it.value().pendingData.clear();
    }
}

int DDPController::pixelCount() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_pixelCount;
}

void DDPController::setSkipUnchanged(bool skip)
{
    QMutexLocker locker(&m_dataMutex);
    m_skipUnchanged = skip;
}

bool DDPController::skipUnchanged() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_skipUnchanged;
}

void DDPController::setKeepAliveIntervalMs(qint64 ms)
{
    QMutexLocker locker(&m_dataMutex);
    m_keepAliveMs = qBound<qint64>(kMinKeepAliveMs, ms, kMaxKeepAliveMs);
}

qint64 DDPController::keepAliveIntervalMs() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_keepAliveMs;
}

void DDPController::setKeepAliveIntervalMsForTest(qint64 ms)
{
    QMutexLocker locker(&m_dataMutex);
    m_keepAliveMs = qMax<qint64>(1, ms);
}

void DDPController::setMaxFpsBypassForTest(bool bypass)
{
    QMutexLocker locker(&m_dataMutex);
    m_maxFpsBypassForTest = bypass;
}
