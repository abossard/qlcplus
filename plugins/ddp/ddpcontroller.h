/*
  Q Light Controller Plus
  ddpcontroller.h

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

#ifndef DDPCONTROLLER_H
#define DDPCONTROLLER_H

#include <QNetworkInterface>
#include <QHostAddress>
#include <QUdpSocket>
#include <QByteArray>
#include <QElapsedTimer>
#include <QMutex>
#include <QMap>
#include <atomic>

#include "ddppacketizer.h"

/** Per-universe output configuration */
struct DDPUniverseInfo
{
    QHostAddress destAddress;
    quint16 destPort;
    quint8 destId;
    quint32 ddpOffset;        // byte offset into the device's pixel buffer
    int transmissionMode;     // 0 = Full, 1 = Partial (dirty-range sends + keep-alive)
    int components;           // 0 = RGB (3 bytes/pixel), 1 = RGBW (4 bytes/pixel)
    qint64 lastSendElapsed = 0;      // per-universe rate limit timestamp
    qint64 lastSendDataElapsed = 0;  // per-universe keepalive timestamp
    quint64 frameCount = 0;          // per-universe sequence counter (T7)

    // ----- Partial-mode baseline state -----
    // Snapshot of bytes last successfully transmitted (length == lastCoverageLen).
    // Diff scans compare new src against this buffer to find dirty runs.
    QByteArray lastSentData;
    // Monotonic ms timestamp of the last full-snapshot send (keep-alive anchor).
    qint64 lastFullFrameElapsed = 0;
    // True only when lastSentData is a faithful copy of what the receiver currently shows
    // for the identity captured below. Cleared on any identity/config change or send failure.
    bool baselineValid = false;
    // Identity snapshot at the moment the baseline was captured. A change to any of these
    // forces the next Partial send to be a full snapshot (mustFull).
    QHostAddress lastDestAddress;
    quint16      lastDestPort = 0;
    quint8       lastDestId = 0;
    quint32      lastDdpOffset = 0;
    int          lastCoverageLen = 0; // bytes of payload covered by the baseline
    int          lastBpp = 0;         // 3 or 4
};

class DDPController final : public QObject
{
    Q_OBJECT

public:
    enum TransmissionMode { Full, Partial };
    enum Components { RGB, RGBW };

    explicit DDPController(QNetworkInterface const& iface,
                           QNetworkAddressEntry const& address,
                           quint32 line, QObject *parent = nullptr);
    ~DDPController();

    /** Send DMX data for a specific QLC+ universe.
     *  When dataChanged is false, the send is suppressed unless the
     *  keep-alive interval (kKeepAliveMs) has elapsed since the last send. */
    void sendDmx(quint32 universe, const QByteArray &data, bool dataChanged = true);

    /** Return the controller IP address */
    QString getNetworkIP() const;

    /** Add a universe to this controller's output map */
    void addUniverse(quint32 universe);

    /** Remove a universe from this controller's output map */
    void removeUniverse(quint32 universe);

    /** Return the list of universes handled by this controller */
    QList<quint32> universesList() const;

    /** Return a thread-safe copy of per-universe info. `found` (if non-null)
     *  is set to true if the universe exists, false otherwise. */
    DDPUniverseInfo getUniverseInfo(quint32 universe, bool *found = nullptr) const;

    // Per-universe setters
    void setDestAddress(quint32 universe, const QString &address);
    void setDestPort(quint32 universe, quint16 port);
    void setDestId(quint32 universe, quint8 id);
    void setDDPOffset(quint32 universe, quint32 offset);
    void setTransmissionMode(quint32 universe, TransmissionMode mode);
    void setComponents(quint32 universe, Components components);

    static QString transmissionModeToString(TransmissionMode mode);
    static TransmissionMode stringToTransmissionMode(const QString &mode);
    static QString componentsToString(Components components);
    static Components stringToComponents(const QString &str);

    /** Return the plugin line associated to this controller */
    quint32 line() const;

    /** Get the number of packets sent by this controller */
    quint64 getPacketSentNumber() const;

    /** Maximum frames per second sent over the wire (0 = no limit). */
    void setMaxFps(int fps);
    int maxFps() const;

    /** Explicit pixel count (0 = auto: send exactly what the engine produced). */
    void setPixelCount(int pixels);
    int pixelCount() const;

    /** Skip sending when data is unchanged (saves bandwidth on Wi-Fi). */
    void setSkipUnchanged(bool skip);
    bool skipUnchanged() const;

    /** Keep-alive interval: re-send unchanged data at least every kKeepAliveMs.
     *  Default chosen well under WLED's ~2500 ms realtime timeout
     *  (wled00/udp.cpp `realtimeTimeoutMs`); the receiver leaves DDP mode if
     *  no packet arrives within that window. */
    static constexpr qint64 kKeepAliveMs = 1000;
    static constexpr qint64 kMinKeepAliveMs = 100;
    static constexpr qint64 kMaxKeepAliveMs = 2400;  // safely under WLED's ~2.5s realtime timeout

    /** Partial-mode keep-alive interval (ms). Clamped to [kMinKeepAliveMs, kMaxKeepAliveMs].
     *  In Partial mode, a full snapshot is forced at least this often regardless of
     *  whether the payload changed, so the receiver stays in realtime mode and
     *  any dropped diff packet self-heals within one interval. */
    void setKeepAliveIntervalMs(qint64 ms);
    qint64 keepAliveIntervalMs() const;

    // ---- Test hooks ----------------------------------------------------
    /** Override the Partial-mode keep-alive interval, bypassing the [min, max]
     *  clamp. Test-only: lets unit tests use sub-100ms intervals. */
    void setKeepAliveIntervalMsForTest(qint64 ms);
    /** Bypass the FPS throttle entirely. Test-only. */
    void setMaxFpsBypassForTest(bool bypass);

private:
    // Helpers (definitions in ddpcontroller.cpp)
    void invalidateBaseline(quint32 universe);     // single universe
    void invalidateAllBaselines();                 // all universes (e.g. pixelCount change)
    // Wire-level senders. Return true if every datagram was accepted by the kernel.
    bool sendFullSnapshot(DDPUniverseInfo &info, const char *srcData, int totalLen,
                          int bpp, quint8 dataType, qint64 now);
    bool sendPartialDiff(DDPUniverseInfo &info, const char *srcData, int totalLen,
                         int bpp, quint8 dataType, qint64 now);
    bool writeChunk(const DDPUniverseInfo &info,
                    const char *srcData, int srcLen,
                    int chunkStart, int chunkLen,
                    quint32 dataOffset, quint8 seq, bool push, quint8 dataType);

private:
    QHostAddress m_ipAddr;
    quint32 m_line;

    QSharedPointer<QUdpSocket> m_udpSocket;
    QMap<quint32, DDPUniverseInfo> m_universeMap;
    mutable QMutex m_dataMutex;

    std::atomic<quint64> m_packetSent{0};

    // Default 0 (no limit): forward every frame the engine produces.
    // MasterTimer caps at ~50 Hz, so 0 simply means "never drop frames".
    // Users can set 1–50 in the config dialog for bandwidth-limited links
    // (e.g. Wi-Fi WLED). The throttle is per-universe inside sendDmx().
    static constexpr int kMaxFpsLimit = 50;
    static constexpr int kDefaultFps = 0;
public:
    static constexpr int maxFpsLimit() { return kMaxFpsLimit; }
    static constexpr int defaultFps() { return kDefaultFps; }
private:
    int m_maxFps = kDefaultFps;
    bool m_skipUnchanged = false;
    QElapsedTimer m_sendTimer;
    bool m_socketTuned = false;  // one-shot SO_SNDBUF + IP_TOS setup

    // Explicit pixel framing — 0 means "auto/legacy" (send what the engine produced)
    int m_pixelCount = 0;

    // ---- Partial-mode tuning -------------------------------------------
    // Keep-alive: in Partial mode, force a full snapshot at least this often
    // (independent of payload changes) to (a) stay well under WLED's ~2.5s
    // realtime timeout and (b) bound the visible damage of any dropped diff
    // packet to one keep-alive interval. Configurable for tests.
    qint64 m_keepAliveMs = kKeepAliveMs;
    // Coalesce two dirty runs separated by ≤ this many clean bytes into one
    // packet. 48 ≈ DDP header (10) + IPv4/UDP overhead (28) per extra packet,
    // and is a multiple of lcm(3,4)=12 so the boundary stays bpp-aligned.
    static constexpr int kPartialMinGapBytes = 48;
    // Test hook: bypass FPS throttle entirely.
    bool m_maxFpsBypassForTest = false;
};

#endif // DDPCONTROLLER_H
