/*
  Q Light Controller Plus
  ddppacketizer.h

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

#ifndef DDPPACKETIZER_H
#define DDPPACKETIZER_H

#include <QByteArray>

// DDP protocol constants
#define DDP_PORT            4048
#define DDP_HEADER_LEN      10
#define DDP_MAX_DATALEN     1440   // 480 RGB pixels × 3 bytes
#define DDP_MAX_PIXELS      480

// Header byte 0: flags
#define DDP_FLAGS_VER1      0x40   // Version 1
#define DDP_FLAGS_PUSH      0x01   // Display update (set on last packet of frame)

// Header byte 2: data type (TTT=type, SSS=size, 8-bit channels)
#define DDP_DATATYPE_RGB888  0x0B   // type=001 (RGB),  size=011 (8-bit) → 3 bytes/pixel
#define DDP_DATATYPE_RGBW888 0x1B   // type=011 (RGBW), size=011 (8-bit) → 4 bytes/pixel

// Header byte 3: destination ID
#define DDP_DEST_DEFAULT    1

/**
 * Stateless packet builder for the DDP (Distributed Display Protocol).
 *
 * All methods are pure calculations: they take inputs and return a packet
 * QByteArray without reading or modifying any external state.
 */
class DDPPacketizer final
{
public:
    /**
     * Build a single DDP packet.
     *
     * @param data       Pixel data payload (RGB or RGBW bytes)
     * @param dataOffset Byte offset into the device's pixel buffer
     * @param sequence   Sequence number (1–15)
     * @param push       True if this is the last packet of a frame
     * @param dataType   DDP data type (DDP_DATATYPE_RGB888 or DDP_DATATYPE_RGBW888)
     * @param destId     Destination ID (default 1)
     * @return Complete DDP packet (header + data) ready to send
     */
    static QByteArray buildPacket(const QByteArray &data, quint32 dataOffset,
                                  quint8 sequence, bool push,
                                  quint8 dataType = DDP_DATATYPE_RGB888,
                                  quint8 destId = DDP_DEST_DEFAULT);

    /**
     * Compute how many DDP packets are needed for the given data length.
     */
    static int packetsRequired(int dataLength);

    /**
     * Compute the sequence number for a given frame count.
     * Cycles 1–15 as per the DDP spec.
     */
    static quint8 sequenceForFrame(quint64 frameCount);
};

#endif // DDPPACKETIZER_H
