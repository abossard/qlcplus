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
// Reference values come from the WLED firmware DDP receiver:
//   wled00/src/dependencies/e131/ESPAsyncE131.h  (header layout, type bytes)
//   wled00/e131.cpp handleDDPPacket()            (parse + dispatch)
// Spec:  http://www.3waylabs.com/ddp/
#define DDP_PORT            4048
#define DDP_HEADER_LEN      10
#define DDP_MAX_DATALEN     1440   // WLED's DDP_CHANNELS_PER_PACKET (480 RGB / 360 RGBW pixels)

// Header byte 0: flags
#define DDP_FLAGS_VER1      0x40   // Version 1; WLED checks via DDP_FLAGS_VER mask
#define DDP_FLAGS_PUSH      0x01   // Display update; gates strip.show() in WLED once any PUSH seen

// Header byte 2: data type (TTT=type, SSS=size, 8-bit channels)
// WLED selects RGBW (bpp=4) only when ((dataType>>3) & 0b111) == 0b011, else RGB (bpp=3).
// e131.cpp:  if ((p->dataType & 0b00111000)>>3 == 0b011) ddpChannelsPerLed = 4;
#define DDP_DATATYPE_RGB888  0x0B   // type=001 (RGB),  size=011 (8-bit) → 3 bytes/pixel
#define DDP_DATATYPE_RGBW888 0x1B   // type=011 (RGBW), size=011 (8-bit) → 4 bytes/pixel

// Header byte 3: destination ID. WLED rejects 246 (CONTROL), 250 (CONFIG), 251 (STATUS)
// as pixel-write targets (e131.cpp early return).
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
     * Write a DDP packet directly into a caller-provided buffer (T3: zero-alloc).
     *
     * @param buf        Output buffer, must be at least DDP_HEADER_LEN + chunkLen bytes
     * @param srcData    Full payload data pointer
     * @param srcLen     Total length of srcData
     * @param chunkStart Offset into srcData for this chunk
     * @param chunkLen   Bytes to send in this packet
     * @param dataOffset DDP byte offset into the device's pixel buffer
     * @param sequence   Sequence number (1–15)
     * @param push       True if this is the last packet of a frame
     * @param dataType   DDP data type
     * @param destId     Destination ID
     */
    static void writePacketInPlace(char *buf,
                                   const char *srcData, int srcLen,
                                   int chunkStart, int chunkLen,
                                   quint32 dataOffset, quint8 sequence,
                                   bool push, quint8 dataType, quint8 destId);

    /**
     * Compute how many DDP packets are needed for the given data length.
     */
    static int packetsRequired(int dataLength);

    /**
     * Compute the sequence number for a given frame count.
     * frameCount is 1-based; output cycles 1..15 (0 is reserved by the DDP spec).
     */
    static quint8 sequenceForFrame(quint64 frameCount);
};

#endif // DDPPACKETIZER_H
