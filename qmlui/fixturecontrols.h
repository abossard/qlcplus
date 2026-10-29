/*
  Q Light Controller Plus
  fixturecontrols.h

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

#ifndef FIXTURECONTROLS_H
#define FIXTURECONTROLS_H

#include <QList>

class QLCFixtureMode;

/** Decides which controls a console page needs to operate every channel of
 *  a fixture mode. Pure: reads the mode, creates nothing. */
class FixtureControls final
{
public:
    /** How a control is operated */
    enum Kind
    {
        Fader,      ///< One continuous channel
        Presets,    ///< One channel whose capabilities are the selectable values
        ColorMixer, ///< The colour component channels of one head
        PanTilt     ///< The pan and tilt channels of one head
    };

    struct Control
    {
        Kind kind;
        /** Head index in the mode, -1 for channels outside every head */
        int head;
        /** Mode channel indices in mode order. Fine channels sit in the same
         *  control as their coarse channel */
        QList<quint32> channels;
    };

    /** The controls for $mode, ordered by their first channel. Every channel
     *  is in exactly one control, except channels of the Nothing group, which
     *  are in none */
    static QList<Control> analyze(const QLCFixtureMode *mode);
};

#endif
