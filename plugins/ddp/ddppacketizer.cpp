/*
  Q Light Controller Plus
  ddppacketizer.cpp

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

#include "ddppacketizer.h"
#include <QtEndian>
#include <cstring>

QByteArray DDPPacketizer::buildPacket(const QByteArray &data, quint32 dataOffset,
                                      quint8 sequence, bool push,
                                      quint8 dataType, quint8 destId)
{
    QByteArray packet(DDP_HEADER_LEN + data.size(), 0);
    char *h = packet.data();

    h[0] = static_cast<char>(DDP_FLAGS_VER1 | (push ? DDP_FLAGS_PUSH : 0));
    h[1] = static_cast<char>(sequence);
    h[2] = static_cast<char>(dataType);
    h[3] = static_cast<char>(destId);

    qToBigEndian<quint32>(dataOffset, h + 4);
    qToBigEndian<quint16>(static_cast<quint16>(data.size()), h + 8);

    memcpy(h + DDP_HEADER_LEN, data.constData(), data.size());

    return packet;
}

void DDPPacketizer::writePacketInPlace(char *buf,
                                       const char *srcData, int srcLen,
                                       int chunkStart, int chunkLen,
                                       quint32 dataOffset, quint8 sequence,
                                       bool push, quint8 dataType, quint8 destId)
{
    // Header (10 bytes)
    buf[0] = static_cast<char>(DDP_FLAGS_VER1 | (push ? DDP_FLAGS_PUSH : 0));
    buf[1] = static_cast<char>(sequence);
    buf[2] = static_cast<char>(dataType);
    buf[3] = static_cast<char>(destId);

    qToBigEndian<quint32>(dataOffset, buf + 4);
    qToBigEndian<quint16>(static_cast<quint16>(chunkLen), buf + 8);

    // Payload: copy available data, zero-pad remainder
    char *payload = buf + DDP_HEADER_LEN;
    const int availableFromSrc = qMax(0, qMin(chunkLen, srcLen - chunkStart));
    if (availableFromSrc > 0)
        memcpy(payload, srcData + chunkStart, availableFromSrc);
    if (availableFromSrc < chunkLen)
        memset(payload + availableFromSrc, 0, chunkLen - availableFromSrc);
}

int DDPPacketizer::packetsRequired(int dataLength)
{
    if (dataLength <= 0)
        return 0;
    return (dataLength + DDP_MAX_DATALEN - 1) / DDP_MAX_DATALEN;
}

quint8 DDPPacketizer::sequenceForFrame(quint64 frameCount)
{
    if (frameCount == 0)
        return 1;
    return static_cast<quint8>(((frameCount - 1) % 15) + 1);
}
