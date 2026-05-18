// DDP packetizer microbench: old buildPacket() loop vs new writePacketInPlace() loop.
// Mirrors the exact per-frame work done in DDPController::sendDmx.
#include <QByteArray>
#include <QElapsedTimer>
#include <QtEndian>
#include <QUdpSocket>
#include <QHostAddress>
#include <QCoreApplication>
#include <cstdio>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>

#define DDP_HEADER_LEN   10
#define DDP_MAX_DATALEN  1440
#define DDP_FLAGS_VER1   0x40
#define DDP_FLAGS_PUSH   0x01
#define DDP_DATATYPE_RGB888 0x0B
#define DDP_DEST_DEFAULT 1

// --- OLD path (pre-T3) ---------------------------------------------------
static QByteArray buildPacket_old(const QByteArray &data, quint32 dataOffset,
                                  quint8 seq, bool push, quint8 dt, quint8 id) {
    QByteArray pkt(DDP_HEADER_LEN + data.size(), 0);
    char *h = pkt.data();
    h[0] = char(DDP_FLAGS_VER1 | (push ? DDP_FLAGS_PUSH : 0));
    h[1] = char(seq); h[2] = char(dt); h[3] = char(id);
    qToBigEndian<quint32>(dataOffset, h + 4);
    qToBigEndian<quint16>(quint16(data.size()), h + 8);
    memcpy(h + DDP_HEADER_LEN, data.constData(), data.size());
    return pkt;
}

static void sendFrame_old(QUdpSocket &s, const QByteArray &frame,
                          const QHostAddress &dst, quint16 port,
                          quint32 baseOff, quint8 seq) {
    // Reproduces the pre-T3 hot path: txData copy + per-chunk mid() + buildPacket
    QByteArray txData = frame; // assume auto mode
    int total = (txData.size() + DDP_MAX_DATALEN - 1) / DDP_MAX_DATALEN;
    for (int i = 0; i < total; i++) {
        int start = i * DDP_MAX_DATALEN;
        int len = qMin(DDP_MAX_DATALEN, txData.size() - start);
        bool push = (i == total - 1);
        QByteArray chunk = txData.mid(start, len);
        QByteArray pkt = buildPacket_old(chunk, baseOff + quint32(start),
                                         seq, push, DDP_DATATYPE_RGB888,
                                         DDP_DEST_DEFAULT);
        s.writeDatagram(pkt.data(), pkt.size(), dst, port);
    }
}

// --- NEW path (T3) -------------------------------------------------------
static void writePacketInPlace(char *buf, const char *src, int srcLen,
                               int start, int len, quint32 off, quint8 seq,
                               bool push, quint8 dt, quint8 id) {
    buf[0] = char(DDP_FLAGS_VER1 | (push ? DDP_FLAGS_PUSH : 0));
    buf[1] = char(seq); buf[2] = char(dt); buf[3] = char(id);
    qToBigEndian<quint32>(off, buf + 4);
    qToBigEndian<quint16>(quint16(len), buf + 8);
    char *pay = buf + DDP_HEADER_LEN;
    int avail = qMax(0, qMin(len, srcLen - start));
    if (avail > 0) memcpy(pay, src + start, avail);
    if (avail < len) memset(pay + avail, 0, len - avail);
}

static void sendFrame_new(QUdpSocket &s, const QByteArray &frame,
                          const QHostAddress &dst, quint16 port,
                          quint32 baseOff, quint8 seq) {
    const char *src = frame.constData();
    int srcLen = frame.size();
    int total = (srcLen + DDP_MAX_DATALEN - 1) / DDP_MAX_DATALEN;
    char buf[DDP_HEADER_LEN + DDP_MAX_DATALEN];
    for (int i = 0; i < total; i++) {
        int start = i * DDP_MAX_DATALEN;
        int len = qMin(DDP_MAX_DATALEN, srcLen - start);
        bool push = (i == total - 1);
        writePacketInPlace(buf, src, srcLen, start, len,
                           baseOff + quint32(start), seq, push,
                           DDP_DATATYPE_RGB888, DDP_DEST_DEFAULT);
        s.writeDatagram(buf, DDP_HEADER_LEN + len, dst, port);
    }
}

// -------------------------------------------------------------------------
struct Result { double usPerFrame; double pktsPerSec; };

template<typename F>
static Result run(const char *label, int frameBytes, int frames, F &&fn,
                  bool tuneSocket) {
    QUdpSocket s;
    s.bind(QHostAddress::LocalHost, 0);
    if (tuneSocket) {
        int fd = int(s.socketDescriptor());
        int snd = 1 << 20;
        ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &snd, sizeof(snd));
        int tos = 0xB8;
        ::setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
    }
    // sink socket so the kernel actually has somewhere to deliver
    QUdpSocket sink; sink.bind(QHostAddress::LocalHost, 0);
    quint16 port = sink.localPort();

    QByteArray frame(frameBytes, char(0x55));
    int chunks = (frameBytes + DDP_MAX_DATALEN - 1) / DDP_MAX_DATALEN;

    // warmup
    for (int i = 0; i < 100; i++) fn(s, frame, QHostAddress::LocalHost, port, 0, 1);

    QElapsedTimer t; t.start();
    for (int i = 0; i < frames; i++)
        fn(s, frame, QHostAddress::LocalHost, port, 0, quint8((i % 15) + 1));
    qint64 ns = t.nsecsElapsed();

    Result r;
    r.usPerFrame = double(ns) / 1000.0 / frames;
    r.pktsPerSec = double(frames) * chunks * 1e9 / double(ns);
    std::printf("  %-22s %8.2f us/frame   %10.0f pkt/s   (%d chunks/frame)\n",
                label, r.usPerFrame, r.pktsPerSec, chunks);
    return r;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const int FRAMES = 20000;

    struct Case { const char *name; int bytes; };
    Case cases[] = {
        {"512B   (170 RGB px)",    512},
        {"1440B  (480 RGB px,1pk)",1440},
        {"3000B  (1000 px, 3 pk)", 3000},
        {"6000B  (2000 px, 5 pk)", 6000},
    };

    for (auto &c : cases) {
        std::printf("\nFrame %s, %d frames:\n", c.name, FRAMES);
        Result o = run("OLD buildPacket",     c.bytes, FRAMES, sendFrame_old, false);
        Result n = run("NEW writePacketInPl", c.bytes, FRAMES, sendFrame_new, false);
        Result t = run("NEW + sock tuned",    c.bytes, FRAMES, sendFrame_new, true);
        std::printf("  -> CPU speedup (new/old):    %5.2fx\n", o.usPerFrame / n.usPerFrame);
        std::printf("  -> CPU speedup (tuned/old):  %5.2fx\n", o.usPerFrame / t.usPerFrame);
    }
    return 0;
}
