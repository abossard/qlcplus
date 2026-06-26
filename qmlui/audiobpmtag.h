/*
  Q Light Controller Plus
  audiobpmtag.h

  Read the BPM from an audio file's ID3v2 tag (TBPM frame) and apply it to a
  Show's time-division settings. Self-contained: no third-party tag library.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef AUDIOBPMTAG_H
#define AUDIOBPMTAG_H

#include <QByteArray>
#include <QString>
#include <optional>

class Show;

namespace AudioBpmTag
{

/** Parse the BPM from an ID3v2 tag at the start of the given bytes.
 *  Returns the rounded BPM if a TBPM (ID3v2.3/2.4) or TBP (ID3v2.2) text frame
 *  is present and numeric; std::nullopt otherwise. Pure function. */
std::optional<int> parseId3v2Bpm(const QByteArray &data);

/** Read the BPM from a file's ID3v2 tag. Returns std::nullopt when the file is
 *  missing/unreadable or has no usable BPM frame. */
std::optional<int> readFileBpm(const QString &filePath);

/** If the file exposes a BPM tag, set the Show's time division to BPM_4_4 at
 *  that BPM. No-op when no tag is found or show is null. */
void applyFileBpmToShow(Show *show, const QString &filePath);

} // namespace AudioBpmTag

#endif // AUDIOBPMTAG_H
