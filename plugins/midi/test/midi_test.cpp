/*
  Q Light Controller Plus
  midi_test.cpp

  Copyright (c) Jano Svitok

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

#include <QTest>

#define private public
#include "midi_test.h"
#include "midiprotocol.h"

#undef private

/****************************************************************************
 * MIDI tests
 ****************************************************************************/

void Midi_Test::midiToInput()
{
    quint32 channel = 0;
    uchar value = 0;

    uchar midiChannel = 7;
    uchar cmd = MIDI_NOTE_ON | midiChannel;
    uchar data1 = 10;
    uchar data2 = 127;

    QLCMIDIProtocol::midiToInput(cmd, data1, data2, midiChannel, &channel, &value);

    QCOMPARE(channel, 138U);
    QCOMPARE(value, uchar(255U));
}

void Midi_Test::describeInputChannel_data()
{
    QTest::addColumn<quint32>("rawChannel");
    QTest::addColumn<int>("expectedType");
    QTest::addColumn<bool>("expectedHasValue");
    QTest::addColumn<int>("expectedValue");
    QTest::addColumn<int>("expectedPackedIndex");

    QTest::newRow("control-change") << quint32(CHANNEL_OFFSET_CONTROL_CHANGE + 12)
                                     << int(QLCMIDIProtocol::MidiInputMessageType::ControlChange)
                                     << true << 12 << 0;
    QTest::newRow("note-with-omni-packed-channel") << (quint32(CHANNEL_OFFSET_NOTE + 7) | (quint32(3) << 12))
                                                    << int(QLCMIDIProtocol::MidiInputMessageType::Note)
                                                    << true << 7 << 3;
    QTest::newRow("pitch-wheel") << quint32(CHANNEL_OFFSET_PITCH_WHEEL)
                                 << int(QLCMIDIProtocol::MidiInputMessageType::PitchWheel)
                                 << false << 0 << 0;
    QTest::newRow("beat-stop") << quint32(CHANNEL_OFFSET_MBC_STOP)
                               << int(QLCMIDIProtocol::MidiInputMessageType::BeatStop)
                               << false << 0 << 0;
    QTest::newRow("unknown-offset") << quint32(900)
                                    << int(QLCMIDIProtocol::MidiInputMessageType::Unknown)
                                    << false << 0 << 0;
}

void Midi_Test::describeInputChannel()
{
    QFETCH(quint32, rawChannel);
    QFETCH(int, expectedType);
    QFETCH(bool, expectedHasValue);
    QFETCH(int, expectedValue);
    QFETCH(int, expectedPackedIndex);

    const QLCMIDIProtocol::MidiInputChannelDescriptor desc =
        QLCMIDIProtocol::describeInputChannel(rawChannel);

    QCOMPARE(desc.rawChannel, rawChannel);
    QCOMPARE(int(desc.type), expectedType);
    QCOMPARE(desc.hasMessageValue, expectedHasValue);
    QCOMPARE(int(desc.messageValue), expectedValue);
    QCOMPARE(int(desc.packedMidiChannelIndex), expectedPackedIndex);
}

void Midi_Test::describeChannelSelection_data()
{
    QTest::addColumn<bool>("routeResolved");
    QTest::addColumn<bool>("parameterPresent");
    QTest::addColumn<int>("parameterValue");
    QTest::addColumn<int>("expectedState");
    QTest::addColumn<int>("expectedIndex");

    QTest::newRow("unresolved-route") << false << false << -1
                                      << int(QLCMIDIProtocol::MidiChannelSelectionState::Unknown) << 0;
    QTest::newRow("missing-parameter-default-fixed-1") << true << false << -1
                                                        << int(QLCMIDIProtocol::MidiChannelSelectionState::Fixed) << 0;
    QTest::newRow("fixed-channel-6") << true << true << 5
                                     << int(QLCMIDIProtocol::MidiChannelSelectionState::Fixed) << 5;
    QTest::newRow("omni") << true << true << 16
                          << int(QLCMIDIProtocol::MidiChannelSelectionState::Omni) << 0;
    QTest::newRow("invalid-negative") << true << true << -1
                                      << int(QLCMIDIProtocol::MidiChannelSelectionState::Invalid) << 0;
    QTest::newRow("invalid-overflow") << true << true << 17
                                      << int(QLCMIDIProtocol::MidiChannelSelectionState::Invalid) << 0;
}

void Midi_Test::describeChannelSelection()
{
    QFETCH(bool, routeResolved);
    QFETCH(bool, parameterPresent);
    QFETCH(int, parameterValue);
    QFETCH(int, expectedState);
    QFETCH(int, expectedIndex);

    const QLCMIDIProtocol::MidiChannelSelectionDescriptor desc =
        QLCMIDIProtocol::describeChannelSelection(routeResolved, parameterPresent, parameterValue);

    QCOMPARE(int(desc.state), expectedState);
    QCOMPARE(int(desc.channelIndex), expectedIndex);
}

void Midi_Test::describeFeedbackChannel_data()
{
    QTest::addColumn<bool>("routeResolved");
    QTest::addColumn<bool>("parameterPresent");
    QTest::addColumn<int>("parameterValue");
    QTest::addColumn<int>("modifier");
    QTest::addColumn<int>("expectedState");
    QTest::addColumn<int>("expectedEffectiveIndex");

    QTest::newRow("unknown-route") << false << false << -1 << 0
                                   << int(QLCMIDIProtocol::MidiFeedbackChannelState::Unknown) << 0;
    QTest::newRow("missing-param-default-channel") << true << false << -1 << 0
                                                   << int(QLCMIDIProtocol::MidiFeedbackChannelState::Fixed) << 0;
    QTest::newRow("fixed-with-modifier") << true << true << 3 << 2
                                         << int(QLCMIDIProtocol::MidiFeedbackChannelState::Fixed) << 5;
    QTest::newRow("fixed-overflow") << true << true << 15 << 1
                                    << int(QLCMIDIProtocol::MidiFeedbackChannelState::Invalid) << 0;
    QTest::newRow("omni-no-modifier") << true << true << 16 << 0
                                      << int(QLCMIDIProtocol::MidiFeedbackChannelState::Omni) << 0;
    QTest::newRow("omni-positive-modifier-invalid") << true << true << 16 << 1
                                                    << int(QLCMIDIProtocol::MidiFeedbackChannelState::Invalid) << 0;
    QTest::newRow("negative-modifier-not-applied") << true << true << 4 << -1
                                                   << int(QLCMIDIProtocol::MidiFeedbackChannelState::Fixed) << 4;
}

void Midi_Test::describeFeedbackChannel()
{
    QFETCH(bool, routeResolved);
    QFETCH(bool, parameterPresent);
    QFETCH(int, parameterValue);
    QFETCH(int, modifier);
    QFETCH(int, expectedState);
    QFETCH(int, expectedEffectiveIndex);

    const QLCMIDIProtocol::MidiChannelSelectionDescriptor selection =
        QLCMIDIProtocol::describeChannelSelection(routeResolved, parameterPresent, parameterValue);
    const QLCMIDIProtocol::MidiFeedbackChannelDescriptor desc =
        QLCMIDIProtocol::describeFeedbackChannel(selection, modifier);

    QCOMPARE(int(desc.state), expectedState);
    QCOMPARE(int(desc.channelIndex), expectedEffectiveIndex);
}

QTEST_MAIN(Midi_Test)
