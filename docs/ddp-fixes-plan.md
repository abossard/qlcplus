# DDP Plugin Fixes Plan

Scope: `plugins/ddp/` — `ddpplugin.{h,cpp}`, `ddpcontroller.{h,cpp}`, `ddppacketizer.{h,cpp}`, `configureddp.{h,cpp}`.

Base class signature (verified): `void QLCIOPlugin::removeFromMap(quint32 universe, quint32 line, Capability type)` — `plugins/interfaces/qlcioplugin.h:422`.

---

## Fix 1: UAF + dangling pointer + removeFromMap arg swap

### Problem

Three bugs in `DDPPlugin::closeOutput()` (`ddpplugin.cpp:199-215`) and `DDPController::getUniverseInfo()` (`ddpcontroller.cpp:148-153`):

1. **Arg swap**: `removeFromMap(output, universe, Output)` at `ddpplugin.cpp:204` passes `(line, universe, ...)` but the base API is `(universe, line, ...)`. Silently corrupts the I/O map (wrong universe gets unmapped).
2. **Use-after-free**: `closeOutput()` does `delete m_IOmapping[output].controller` while another thread is inside `writeUniverse()` → `controller->sendDmx()`. The check `if (controller != nullptr)` in `writeUniverse` is racy: the pointer can be valid at check time and freed before `sendDmx` returns. `m_dataMutex` inside `sendDmx` does not protect the controller object lifetime itself.
3. **Dangling pointer**: `getUniverseInfo()` returns `&m_universeMap[u]`. The caller (`configureddp.cpp:90`) holds it across no lock; a concurrent `removeUniverse()` / `addUniverse()` (via `QMap::remove` / `QMap::insert`) would invalidate the reference. Caller only reads scalar fields once, so return by value.

### Changes

**ddpplugin.h** — change `m_IOmapping` to hold a shared controller:

Before (`ddpplugin.h:32-37`):
```cpp
typedef struct
{
    QNetworkInterface iface;
    QNetworkAddressEntry address;
    DDPController* controller;
} DDPIO;
```
After:
```cpp
#include <QSharedPointer>
typedef struct
{
    QNetworkInterface iface;
    QNetworkAddressEntry address;
    QSharedPointer<DDPController> controller;
} DDPIO;
```

**ddpplugin.cpp** — adapt all controller accesses. The crucial property: `writeUniverse` takes a local `QSharedPointer` copy, which keeps the controller alive for the duration of the call even if `closeOutput` resets the slot.

Before (`ddpplugin.cpp:46-49`):
```cpp
DDPIO tmpIO;
tmpIO.iface = iface;
tmpIO.address = entry;
tmpIO.controller = nullptr;
```
After:
```cpp
DDPIO tmpIO;
tmpIO.iface = iface;
tmpIO.address = entry;
tmpIO.controller.reset();
```

Before (`ddpplugin.cpp:114-119` — pluginDiagnostics):
```cpp
if (io.controller)
{
    quint64 sent = io.controller->getPacketSentNumber();
```
After: unchanged (`QSharedPointer::operator bool` and `->` work the same).

Before (`ddpplugin.cpp:154`):
```cpp
DDPController *ctrl = m_IOmapping.at(output).controller;
```
After:
```cpp
QSharedPointer<DDPController> ctrl = m_IOmapping.at(output).controller;
```

Before (`ddpplugin.cpp:184-194` — openOutput):
```cpp
if (m_IOmapping[output].controller == nullptr)
{
    DDPController *controller = new DDPController(
        m_IOmapping.at(output).iface,
        m_IOmapping.at(output).address,
        output, this);
    m_IOmapping[output].controller = controller;
}

m_IOmapping[output].controller->addUniverse(universe);
addToMap(universe, output, Output);
```
After:
```cpp
if (m_IOmapping[output].controller.isNull())
{
    m_IOmapping[output].controller.reset(new DDPController(
        m_IOmapping.at(output).iface,
        m_IOmapping.at(output).address,
        output, nullptr));   // no QObject parent — lifetime managed by QSharedPointer
}

m_IOmapping[output].controller->addUniverse(universe);
addToMap(universe, output, Output);
```

Before (`ddpplugin.cpp:199-215` — closeOutput, fix arg order + UAF):
```cpp
void DDPPlugin::closeOutput(quint32 output, quint32 universe)
{
    if (output >= (quint32)m_IOmapping.length())
        return;

    removeFromMap(output, universe, Output);
    DDPController *controller = m_IOmapping.at(output).controller;
    if (controller != nullptr)
    {
        controller->removeUniverse(universe);
        if (controller->universesList().count() == 0)
        {
            delete m_IOmapping[output].controller;
            m_IOmapping[output].controller = nullptr;
        }
    }
}
```
After:
```cpp
void DDPPlugin::closeOutput(quint32 output, quint32 universe)
{
    if (output >= (quint32)m_IOmapping.length())
        return;

    removeFromMap(universe, output, Output);   // fixed arg order

    QSharedPointer<DDPController> controller = m_IOmapping.at(output).controller;
    if (!controller.isNull())
    {
        controller->removeUniverse(universe);
        if (controller->universesList().isEmpty())
        {
            // Dropping the slot's strong ref. Any in-flight writeUniverse()
            // still holds its own copy and will keep the controller alive
            // until it returns, then the controller is destroyed safely.
            m_IOmapping[output].controller.reset();
        }
    }
}
```

Before (`ddpplugin.cpp:217-228` — writeUniverse, take a strong ref):
```cpp
void DDPPlugin::writeUniverse(quint32 universe, quint32 output,
                               const QByteArray &data, bool dataChanged)
{
    Q_UNUSED(dataChanged)

    if (output >= (quint32)m_IOmapping.count())
        return;

    DDPController *controller = m_IOmapping[output].controller;
    if (controller != nullptr)
        controller->sendDmx(universe, data);
}
```
After:
```cpp
void DDPPlugin::writeUniverse(quint32 universe, quint32 output,
                               const QByteArray &data, bool dataChanged)
{
    if (output >= (quint32)m_IOmapping.count())
        return;

    // Local strong ref — keeps controller alive even if closeOutput()
    // resets the map slot mid-call.
    QSharedPointer<DDPController> controller = m_IOmapping[output].controller;
    if (!controller.isNull())
        controller->sendDmx(universe, data, dataChanged);   // see Fix 3
}
```

Before (`ddpplugin.cpp:271-302` — setParameter):
```cpp
DDPController *controller = m_IOmapping.at(line).controller;
if (controller == nullptr)
    return;
```
After:
```cpp
QSharedPointer<DDPController> controller = m_IOmapping.at(line).controller;
if (controller.isNull())
    return;
```

**ddpcontroller.h** — return `DDPUniverseInfo` by value:

Before (`ddpcontroller.h:71-72`):
```cpp
/** Return per-universe info (or nullptr if not found) */
DDPUniverseInfo *getUniverseInfo(quint32 universe);
```
After:
```cpp
/** Return a thread-safe copy of per-universe info. `found` is set to true
 *  if the universe exists, false otherwise (returned struct is default-init). */
DDPUniverseInfo getUniverseInfo(quint32 universe, bool *found = nullptr) const;
```

Also mark `m_dataMutex` `mutable` so `const` getters can lock:

Before (`ddpcontroller.h:100`):
```cpp
QMutex m_dataMutex;
```
After:
```cpp
mutable QMutex m_dataMutex;
```

**ddpcontroller.cpp** — implementation + `universesList()` lock:

Before (`ddpcontroller.cpp:143-153`):
```cpp
QList<quint32> DDPController::universesList() const
{
    return m_universeMap.keys();
}

DDPUniverseInfo *DDPController::getUniverseInfo(quint32 universe)
{
    if (m_universeMap.contains(universe))
        return &m_universeMap[universe];
    return nullptr;
}
```
After:
```cpp
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
```

**configureddp.cpp** — adapt caller (line 90):

Before:
```cpp
DDPUniverseInfo *info = controller->getUniverseInfo(universe);
if (info == nullptr)
    continue;
```
After:
```cpp
bool found = false;
DDPUniverseInfo info = controller->getUniverseInfo(universe, &found);
if (!found)
    continue;
```
And update all `info->field` to `info.field` (lines 103, 109, 115, 122, 130, 138 in `configureddp.cpp`).

### Files affected

- `plugins/ddp/ddpplugin.h` (struct, include)
- `plugins/ddp/ddpplugin.cpp` (init, openOutput, closeOutput, writeUniverse, outputInfo, setParameter, pluginDiagnostics — all `controller` accesses)
- `plugins/ddp/ddpcontroller.h` (signature, mutable mutex)
- `plugins/ddp/ddpcontroller.cpp` (`getUniverseInfo`, `universesList`)
- `plugins/ddp/configureddp.cpp` (`fillMappingTree`)

### Test scenarios

1. Open output, start sending DMX in a tight loop, call `closeOutput` repeatedly from another thread → no crash, no ASAN/TSAN warnings.
2. Configure dialog while DMX is flowing → no crash, values shown correctly.
3. Close output #0 universe X while output #0 also serves universe Y → only Y kept, X correctly unmapped from `addToMap` table (fixed arg order means correct universe is removed; verifiable with debug log of `m_universeMap`).

---

## Fix 2: Configurable frame rate limit

### Problem

`sendDmx()` (`ddpcontroller.cpp:46`) sends every time the universe thread calls it (up to ~50Hz from MasterTimer). WLED recommends ≤44 FPS over the wire; faster floods the LED bus and causes glitches. No throttling exists.

### Changes

**ddpplugin.h** — add parameter constant:

Before (`ddpplugin.h:40-45`):
```cpp
#define DDP_IP          "ddpIP"
#define DDP_DESTPORT    "ddpPort"
#define DDP_OFFSET      "ddpOffset"
#define DDP_DESTID      "ddpDestId"
#define DDP_TRANSMITMODE "ddpTransmitMode"
#define DDP_COMPONENTS   "ddpComponents"
```
After: add
```cpp
#define DDP_MAXFPS       "ddpMaxFps"
```

**ddpcontroller.h** — add per-universe field + setter:

Before (`ddpcontroller.h:33-41`):
```cpp
typedef struct
{
    QHostAddress destAddress;
    quint16 destPort;
    quint8 destId;
    quint32 ddpOffset;
    int transmissionMode;
    int components;
} DDPUniverseInfo;
```
After:
```cpp
typedef struct
{
    QHostAddress destAddress;
    quint16 destPort;
    quint8 destId;
    quint32 ddpOffset;
    int transmissionMode;
    int components;
    int maxFps;          // 1..240, default 44 (WLED recommended max)
    qint64 lastSendMs;   // monotonic ms of last successful send (0 = never)
    qint64 lastDataChangeMs; // see Fix 3
    bool offline;        // see Fix 4
    int consecutiveFailures; // see Fix 4
} DDPUniverseInfo;
```
Add public setter:
```cpp
void setMaxFps(quint32 universe, int fps);
```

**ddpcontroller.cpp**

Before (`ddpcontroller.cpp:120-135` — addUniverse defaults):
```cpp
DDPUniverseInfo info;
info.destAddress = QHostAddress::Broadcast;
info.destPort = DDP_PORT;
info.destId = DDP_DEST_DEFAULT;
info.ddpOffset = 0;
info.transmissionMode = Full;
info.components = RGB;
m_universeMap[universe] = info;
```
After:
```cpp
DDPUniverseInfo info;
info.destAddress = QHostAddress();   // no IP — see Fix 5
info.destPort = DDP_PORT;
info.destId = DDP_DEST_DEFAULT;
info.ddpOffset = 0;
info.transmissionMode = Full;
info.components = RGB;
info.maxFps = 44;
info.lastSendMs = 0;
info.lastDataChangeMs = 0;
info.offline = false;
info.consecutiveFailures = 0;
m_universeMap[universe] = info;
```

Add at top of `sendDmx()` (after the `info` copy is taken — see Fix 3 for full reorganisation):
```cpp
const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
const int fps = qBound(1, info.maxFps, 240);
const qint64 minIntervalMs = 1000 / fps;
if (info.lastSendMs != 0 && (nowMs - info.lastSendMs) < minIntervalMs)
    return;   // throttled
```
Add `#include <QDateTime>` to `ddpcontroller.cpp`.

After a successful send, write back:
```cpp
m_universeMap[universe].lastSendMs = nowMs;
```

Add setter:
```cpp
void DDPController::setMaxFps(quint32 universe, int fps)
{
    QMutexLocker locker(&m_dataMutex);
    if (m_universeMap.contains(universe))
        m_universeMap[universe].maxFps = qBound(1, fps, 240);
}
```

**ddpplugin.cpp** — wire parameter:

In `setParameter()` (`ddpplugin.cpp:281-299`), add:
```cpp
else if (name == DDP_MAXFPS)
    controller->setMaxFps(universe, value.toInt());
```

**configureddp.{ui,cpp}** — add a `KColumnMaxFps` `QSpinBox` (range 1–240, default 44). Plumb it like the other columns in `fillMappingTree` and `accept()`.

### Files affected

- `plugins/ddp/ddpplugin.h` (define)
- `plugins/ddp/ddpplugin.cpp` (setParameter)
- `plugins/ddp/ddpcontroller.h` (struct, setter)
- `plugins/ddp/ddpcontroller.cpp` (defaults, throttle, setter, include)
- `plugins/ddp/configureddp.ui` (extra column header)
- `plugins/ddp/configureddp.cpp` (column index, fill, accept)

### Test scenarios

1. Set `maxFps=10`, run a flowing scene → packet rate ≈10/s (verify with `getPacketSentNumber()` or `tcpdump`).
2. Set `maxFps=44` (default), MasterTimer 50Hz → packet rate ≈44/s.
3. Set `maxFps=240` → throttle disabled in practice.

---

## Fix 3: Skip resends when data unchanged + 1 s keep-alive

### Problem

`writeUniverse()` discards `dataChanged` (`ddpplugin.cpp:220` — `Q_UNUSED(dataChanged)`). Every MasterTimer tick re-sends identical data, wasting bandwidth on static scenes.

### Changes

**ddpcontroller.h** — extend `sendDmx`:

Before (`ddpcontroller.h:57`):
```cpp
void sendDmx(quint32 universe, const QByteArray &data);
```
After:
```cpp
void sendDmx(quint32 universe, const QByteArray &data, bool dataChanged = true);
```
Add constant:
```cpp
static constexpr qint64 DDP_KEEPALIVE_MS = 1000;
```

**ddpcontroller.cpp** — in `sendDmx` (`ddpcontroller.cpp:46`):

After taking the universe info copy and computing `nowMs` (Fix 2):
```cpp
DDPUniverseInfo info;
{
    QMutexLocker locker(&m_dataMutex);
    auto it = m_universeMap.constFind(universe);
    if (it == m_universeMap.constEnd())
    {
        qWarning() << Q_FUNC_INFO << "universe" << universe << "unknown";
        return;
    }
    info = it.value();
}

const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

// FPS throttle (Fix 2)
const int fps = qBound(1, info.maxFps, 240);
const qint64 minIntervalMs = 1000 / fps;
if (info.lastSendMs != 0 && (nowMs - info.lastSendMs) < minIntervalMs)
    return;

// Change-or-keepalive gate (Fix 3)
const bool keepaliveDue =
    (info.lastSendMs == 0) ||
    (nowMs - info.lastSendMs) >= DDP_KEEPALIVE_MS;
if (!dataChanged && !keepaliveDue)
    return;
```

Note: the working copy (`info`) is read once under lock, then released. State-back writes after send happen under a fresh lock.

**ddpplugin.cpp** — already updated in Fix 1 to forward `dataChanged`.

### Files affected

- `plugins/ddp/ddpcontroller.h` (signature, constant)
- `plugins/ddp/ddpcontroller.cpp` (`sendDmx` gating)
- `plugins/ddp/ddpplugin.cpp` (`writeUniverse` already forwards in Fix 1)

### Test scenarios

1. Open output, set static scene → packets sent ≈1/s (keep-alive only).
2. Trigger a value change → next send is immediate (subject to FPS throttle).
3. Disconnect + reconnect WLED → keepalive ensures resync within ≤1 s after device reboot.

---

## Fix 4: Auto-reconnect on network failure

### Problem

`sendDmx()` already breaks the per-frame loop on `writeDatagram` failure (`ddpcontroller.cpp:106-110`) but never tracks state. Repeated failures spam `qWarning` once per packet; transient recoveries are silent; no UI signal.

### Changes

**ddpcontroller.h** — add constants & helpers:

```cpp
static constexpr int DDP_OFFLINE_THRESHOLD = 5;   // consecutive failures
bool isOffline(quint32 universe) const;
```
(Fields `offline` and `consecutiveFailures` already added in Fix 2.)

**ddpcontroller.cpp** — replace the inner send loop (`ddpcontroller.cpp:89-112`):

Before:
```cpp
for (int i = 0; i < totalPackets; i++)
{
    ...
    qint64 sent = m_udpSocket->writeDatagram(...);
    if (sent < 0)
    {
        qWarning() << "[DDP] sendDmx failed:" << m_udpSocket->errorString();
        break;
    }
    m_packetSent++;
}
```
After:
```cpp
bool sendFailed = false;
for (int i = 0; i < totalPackets; i++)
{
    int chunkStart = i * DDP_MAX_DATALEN;
    int chunkLen = qMin(DDP_MAX_DATALEN, txData.size() - chunkStart);
    bool isLastChunk = (i == totalPackets - 1);

    QByteArray chunk = txData.mid(chunkStart, chunkLen);
    QByteArray packet = DDPPacketizer::buildPacket(
        chunk,
        info.ddpOffset + static_cast<quint32>(chunkStart),
        seq, isLastChunk, dataType, info.destId);

    qint64 sent = m_udpSocket->writeDatagram(
        packet.data(), packet.size(),
        info.destAddress, info.destPort);

    if (sent < 0)
    {
        sendFailed = true;
        break;
    }
    m_packetSent++;
}

{
    QMutexLocker locker(&m_dataMutex);
    DDPUniverseInfo &live = m_universeMap[universe];
    live.lastSendMs = nowMs;   // even on failure, throttle next attempt
    if (sendFailed)
    {
        live.consecutiveFailures++;
        if (!live.offline && live.consecutiveFailures >= DDP_OFFLINE_THRESHOLD)
        {
            live.offline = true;
            qWarning().noquote()
                << "[DDP] output offline:"
                << info.destAddress.toString()
                << "(" << m_udpSocket->errorString() << ")";
        }
    }
    else
    {
        if (live.offline)
        {
            qInfo().noquote()
                << "[DDP] output reconnected:" << info.destAddress.toString();
        }
        live.offline = false;
        live.consecutiveFailures = 0;
    }
}
```

Add accessor:
```cpp
bool DDPController::isOffline(quint32 universe) const
{
    QMutexLocker locker(&m_dataMutex);
    auto it = m_universeMap.constFind(universe);
    return it != m_universeMap.constEnd() && it.value().offline;
}
```

**ddpplugin.cpp** — surface in `outputInfo()` (`ddpplugin.cpp:145-171`):

After the existing `Status: Open` line, append per-universe status:
```cpp
str += QString("<BR>");
str += tr("Universes:");
foreach (quint32 u, ctrl->universesList())
{
    bool found = false;
    DDPUniverseInfo info = ctrl->getUniverseInfo(u, &found);
    if (!found) continue;
    QString state = ctrl->isOffline(u) ? tr("OFFLINE") : tr("online");
    str += QString("<BR>&nbsp;&nbsp;%1 → %2 [%3]")
              .arg(u + 1)
              .arg(info.destAddress.toString().isEmpty()
                   ? tr("(no IP)") : info.destAddress.toString())
              .arg(state);
}
```

### Files affected

- `plugins/ddp/ddpcontroller.h` (constants, accessor)
- `plugins/ddp/ddpcontroller.cpp` (`sendDmx` failure tracking, `isOffline`)
- `plugins/ddp/ddpplugin.cpp` (`outputInfo`)

### Test scenarios

1. Configure unreachable IP → after 5 ticks, single warning logged; no further spam.
2. `outputInfo` shows `OFFLINE` for that universe.
3. Bring device online → next successful send logs `reconnected`, status flips to `online`.
4. Throttle (Fix 2) still applies to retries — no full-rate retry storm.

---

## Fix 5: Default to unicast (no broadcast)

### Problem

`addUniverse()` defaults `info.destAddress = QHostAddress::Broadcast` (`ddpcontroller.cpp:127`). On a typical home/studio LAN this floods every host on the subnet with DDP traffic and is hostile to other devices. WLED users almost always want unicast.

### Changes

**ddpcontroller.cpp** — already updated in Fix 2 to default `destAddress = QHostAddress()` (null). On top of that, in `sendDmx`, add an early guard:

```cpp
if (info.destAddress.isNull() || info.destAddress == QHostAddress::Any)
{
    static thread_local bool warned = false;
    if (!warned)
    {
        qWarning().noquote()
            << "[DDP] no destination IP configured for universe"
            << universe << "— skipping send. Configure an IP in the DDP plugin dialog.";
        warned = true;
    }
    return;
}
```

(The `static thread_local` flag prevents log flooding at the engine tick rate.)

**ddpplugin.cpp** — surface in `outputInfo()` (covered by Fix 4 snippet — empty IP shown as `(no IP)`).

**configureddp.cpp** — in `accept()` (`configureddp.cpp:175-180`), reject empty IPs:

Before:
```cpp
QHostAddress addr(ipEdit->text());
if (addr.isNull() && ipEdit->text() != "255.255.255.255")
{
    showIPAlert(ipEdit->text());
    return;
}
```
After:
```cpp
QHostAddress addr(ipEdit->text());
if (ipEdit->text().trimmed().isEmpty() ||
    (addr.isNull() && ipEdit->text() != "255.255.255.255"))
{
    showIPAlert(ipEdit->text().isEmpty() ? tr("<empty>") : ipEdit->text());
    return;
}
```

### Files affected

- `plugins/ddp/ddpcontroller.cpp` (default + guard)
- `plugins/ddp/ddpplugin.cpp` (status string, via Fix 4)
- `plugins/ddp/configureddp.cpp` (validation)

### Test scenarios

1. Open output without configuring IP → no UDP packets emitted, single warning logged once.
2. `outputInfo` shows `(no IP)` for the universe.
3. Configure valid unicast IP → packets flow only to that host (verify with `tcpdump host <ip>`).
4. Save-as-broadcast (255.255.255.255) still allowed for power users.

---

## Fix 6: Send exact pixel length (no 512-byte pad)

### Problem

In `Full` mode, `sendDmx` builds a 512-byte buffer and zero-pads (`ddpcontroller.cpp:59-63`). DDP is a pixel protocol; padding wastes bandwidth and forces WLED to overwrite real pixel data with zeros if `pixelCount * bpp > data.length()`. DMX's 512-channel framing is irrelevant to DDP.

### Changes

**ddpplugin.h** — new parameter:
```cpp
#define DDP_PIXELCOUNT   "ddpPixelCount"
```

**ddpcontroller.h** — add field + setter:

In `DDPUniverseInfo`:
```cpp
quint32 pixelCount;   // 0 = auto (use data.length())
```
Add:
```cpp
void setPixelCount(quint32 universe, quint32 pixels);
```

**ddpcontroller.cpp**

In `addUniverse` defaults (already touched in Fix 2):
```cpp
info.pixelCount = 0;
```

Replace the txData construction (`ddpcontroller.cpp:58-67`):

Before:
```cpp
QByteArray txData;
if (info.transmissionMode == Full)
{
    txData = QByteArray(512, 0);
    txData.replace(0, data.length(), data);
}
else
{
    txData = QByteArray(data.constData(), data.size());
}

if (txData.isEmpty())
    return;
```
After:
```cpp
const int bpp = (info.components == RGBW) ? 4 : 3;

int targetLen;
if (info.pixelCount > 0)
{
    targetLen = static_cast<int>(info.pixelCount) * bpp;
}
else
{
    // Auto: send exactly what the engine produced. Round down to a whole
    // pixel so we never send a partial RGB(W) tuple.
    targetLen = (data.size() / bpp) * bpp;
}

if (targetLen <= 0)
    return;

QByteArray txData;
if (targetLen <= data.size())
{
    txData = QByteArray(data.constData(), targetLen);
}
else
{
    // pixelCount declares more pixels than the universe carries → pad zeros.
    txData = QByteArray(targetLen, 0);
    txData.replace(0, data.size(), data);
}
```

This removes the 512-byte cap and the `Full`/`Partial` distinction for the buffer (the modes can still differ in semantics but no longer in padding behaviour). Optionally deprecate `transmissionMode` later — out of scope here.

Add setter:
```cpp
void DDPController::setPixelCount(quint32 universe, quint32 pixels)
{
    QMutexLocker locker(&m_dataMutex);
    if (m_universeMap.contains(universe))
        m_universeMap[universe].pixelCount = pixels;
}
```

**ddpplugin.cpp** — wire parameter in `setParameter`:
```cpp
else if (name == DDP_PIXELCOUNT)
    controller->setPixelCount(universe, value.toUInt());
```

**configureddp.{ui,cpp}** — add a `KColumnPixelCount` `QSpinBox` (range 0–10000, 0 = auto, tooltip explaining auto). Plumb into `fillMappingTree` and `accept()`.

### Files affected

- `plugins/ddp/ddpplugin.h` (define)
- `plugins/ddp/ddpplugin.cpp` (`setParameter`)
- `plugins/ddp/ddpcontroller.h` (field, setter)
- `plugins/ddp/ddpcontroller.cpp` (defaults, txData logic, setter)
- `plugins/ddp/configureddp.ui` (column)
- `plugins/ddp/configureddp.cpp` (column index, fill, accept)

### Test scenarios

1. RGB universe, scene uses 30 channels → packet payload = 30 bytes, not 512 (verify via `tcpdump`/Wireshark length).
2. Set `pixelCount=10` RGB → exactly 30 bytes payload regardless of QLC+ channel count.
3. Set `pixelCount=10` RGBW → exactly 40 bytes.
4. `pixelCount > universe size` → payload zero-padded to declared length.
5. WLED with 100 LEDs sees no spurious black overwrites past the controlled range.

---

## Cross-cutting concerns

- **Build**: pure plugin changes; rebuild with `cmake --build build --target ddp -j8` (target name follows existing plugin convention; verify in `plugins/ddp/CMakeLists.txt`).
- **Backwards compatibility**: new params (`ddpMaxFps`, `ddpPixelCount`) default to safe values; existing workspaces load fine — missing fields fall back to defaults.
- **Default change** (Fix 5): broadcast → null IP. Existing saved workspaces with explicit IPs are unaffected; new outputs require user configuration. Document in release notes.
- **Threading invariants** after fixes:
  - `m_IOmapping` is only touched on the thread that called `init()/openOutput()/closeOutput()` (engine main thread).
  - `DDPController` lifetime is governed by `QSharedPointer`; safe across threads.
  - `m_universeMap` is always accessed under `m_dataMutex`.
  - `m_udpSocket` write is single-shot per call; multiple universes hitting `sendDmx` concurrently is safe because `QUdpSocket::writeDatagram` is thread-safe for the same socket per Qt docs (verify; if not, lock around the write).
