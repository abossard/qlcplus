// Packet-build only (no socket): isolates T3 effect.
#include <QByteArray>
#include <QElapsedTimer>
#include <QtEndian>
#include <cstdio>
#include <cstring>

#define DDP_HEADER_LEN 10
#define DDP_MAX_DATALEN 1440
#define DDP_FLAGS_VER1 0x40
#define DDP_FLAGS_PUSH 0x01
#define DDP_DATATYPE_RGB888 0x0B

static QByteArray buildPacket_old(const QByteArray &d, quint32 off, quint8 s, bool p) {
    QByteArray pkt(DDP_HEADER_LEN + d.size(), 0);
    char *h = pkt.data();
    h[0] = char(DDP_FLAGS_VER1 | (p ? DDP_FLAGS_PUSH : 0));
    h[1] = char(s); h[2] = char(DDP_DATATYPE_RGB888); h[3] = 1;
    qToBigEndian<quint32>(off, h + 4);
    qToBigEndian<quint16>(quint16(d.size()), h + 8);
    memcpy(h + DDP_HEADER_LEN, d.constData(), d.size());
    return pkt;
}

static void writePacketInPlace(char *buf, const char *src, int srcLen,
                               int start, int len, quint32 off, quint8 s, bool p) {
    buf[0] = char(DDP_FLAGS_VER1 | (p ? DDP_FLAGS_PUSH : 0));
    buf[1] = char(s); buf[2] = char(DDP_DATATYPE_RGB888); buf[3] = 1;
    qToBigEndian<quint32>(off, buf + 4);
    qToBigEndian<quint16>(quint16(len), buf + 8);
    int avail = qMax(0, qMin(len, srcLen - start));
    if (avail > 0) memcpy(buf + DDP_HEADER_LEN, src + start, avail);
    if (avail < len) memset(buf + DDP_HEADER_LEN + avail, 0, len - avail);
}

int main() {
    const int FRAMES = 500000;
    int sizes[] = {512, 1440, 3000, 6000};
    for (int sz : sizes) {
        QByteArray frame(sz, char(0x55));
        int chunks = (sz + DDP_MAX_DATALEN - 1) / DDP_MAX_DATALEN;

        // OLD
        volatile qsizetype sink = 0;
        QElapsedTimer t; t.start();
        for (int f = 0; f < FRAMES; f++) {
            QByteArray tx = frame; // copy step from old hot path
            for (int i = 0; i < chunks; i++) {
                int st = i * DDP_MAX_DATALEN;
                int ln = qMin(DDP_MAX_DATALEN, tx.size() - st);
                QByteArray ch = tx.mid(st, ln);
                QByteArray pkt = buildPacket_old(ch, quint32(st), 1, i == chunks - 1);
                asm volatile("" : : "r"(pkt.constData()) : "memory");
                sink += pkt.constData()[0] + pkt.constData()[pkt.size() - 1];
            }
        }
        double oldNs = double(t.nsecsElapsed()) / FRAMES;

        // NEW
        char buf[DDP_HEADER_LEN + DDP_MAX_DATALEN];
        t.restart();
        for (int f = 0; f < FRAMES; f++) {
            const char *src = frame.constData();
            int srcLen = frame.size();
            for (int i = 0; i < chunks; i++) {
                int st = i * DDP_MAX_DATALEN;
                int ln = qMin(DDP_MAX_DATALEN, srcLen - st);
                writePacketInPlace(buf, src, srcLen, st, ln, quint32(st), 1, i == chunks - 1);
                asm volatile("" : : "r"(buf) : "memory");
                sink += buf[0] + buf[DDP_HEADER_LEN + ln - 1];
            }
        }
        double newNs = double(t.nsecsElapsed()) / FRAMES;

        std::printf("Frame %5d B (%d chunks): OLD %7.0f ns  NEW %7.0f ns  speedup %.2fx  saved %.0f ns/frame\n",
                    sz, chunks, oldNs, newNs, oldNs / newNs, oldNs - newNs);
    }
    return 0;
}
