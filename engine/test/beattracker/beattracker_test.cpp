/*
  Q Light Controller Plus - Unit test
  beattracker_test.cpp

  Copyright (c) varghele

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

#include <QtTest>
#include <cmath>
#include <vector>

#include "beattracker.h"
#include "beattracker_test.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 44100
#define REFERENCE_BLOCK_FRAMES 2048
#define CAPTURE_BLOCK_FRAMES 512

/** Mix a synthetic kick drum (150->45 Hz sweep, exponential decay)
 *  into the buffer at the given start frame */
static void addKick(std::vector<int16_t> &buffer, int startFrame)
{
    const int length = int(0.09 * SAMPLE_RATE);
    double phase = 0.0;
    for (int i = 0; i < length; i++)
    {
        int pos = startFrame + i;
        if (pos >= int(buffer.size()))
            break;
        double t = double(i) / SAMPLE_RATE;
        double freq = 150.0 * std::exp(-t * 25.0) + 45.0;
        phase += 2.0 * M_PI * freq / SAMPLE_RATE;
        double v = std::sin(phase) * std::exp(-t * 30.0) * 0.9;
        int sample = int(buffer[pos]) + int(v * 20000.0);
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        buffer[pos] = int16_t(sample);
    }
}

/** Render a mono kick track at the given tempo */
static std::vector<int16_t> makeKickTrack(double bpm, double seconds)
{
    std::vector<int16_t> buffer(size_t(seconds * SAMPLE_RATE), 0);
    double beat = 0.0;
    while (beat < seconds)
    {
        addKick(buffer, int(beat * SAMPLE_RATE));
        beat += 60.0 / bpm;
    }
    return buffer;
}

/** Feed a mono track through the tracker in AudioCapture-sized blocks;
 *  returns the block index of every emitted beat */
static std::vector<int> runTracker(BeatTracker &tracker,
                                   const std::vector<int16_t> &audio,
                                   int blockFrames,
                                   int channels = 1)
{
    std::vector<int> beatBlocks;
    const int blockSamples = blockFrames * channels;
    std::vector<int16_t> block(blockSamples);
    int blockIndex = 0;
    for (size_t start = 0; start + blockFrames <= audio.size();
         start += blockFrames, blockIndex++)
    {
        for (int i = 0; i < blockFrames; i++)
            for (int c = 0; c < channels; c++)
                block[i * channels + c] = audio[start + i];

        if (tracker.processAudio(block.data(), blockSamples))
            beatBlocks.push_back(blockIndex);
    }
    return beatBlocks;
}

void BeatTracker_Test::detectsSteadyTempo_data()
{
    QTest::addColumn<double>("bpm");
    QTest::addColumn<int>("blockFrames");
    QTest::newRow("90 BPM, 2048 frames") << 90.0 << REFERENCE_BLOCK_FRAMES;
    QTest::newRow("120 BPM, 2048 frames") << 120.0 << REFERENCE_BLOCK_FRAMES;
    QTest::newRow("140 BPM, 2048 frames") << 140.0 << REFERENCE_BLOCK_FRAMES;
    QTest::newRow("174 BPM, 2048 frames") << 174.0 << REFERENCE_BLOCK_FRAMES;
    QTest::newRow("90 BPM, 512 frames") << 90.0 << CAPTURE_BLOCK_FRAMES;
    QTest::newRow("120 BPM, 512 frames") << 120.0 << CAPTURE_BLOCK_FRAMES;
    QTest::newRow("140 BPM, 512 frames") << 140.0 << CAPTURE_BLOCK_FRAMES;
    QTest::newRow("174 BPM, 512 frames") << 174.0 << CAPTURE_BLOCK_FRAMES;
}

void BeatTracker_Test::detectsSteadyTempo()
{
    QFETCH(double, bpm);
    QFETCH(int, blockFrames);

    BeatTracker tracker(SAMPLE_RATE, 1);
    std::vector<int16_t> audio = makeKickTrack(bpm, 20.0);
    runTracker(tracker, audio, blockFrames);

    QVERIFY(tracker.bpm() > 0.0);
    QVERIFY2(std::fabs(tracker.bpm() - bpm) / bpm < 0.02,
             qPrintable(QString("detected %1, expected %2")
                        .arg(tracker.bpm()).arg(bpm)));
}

void BeatTracker_Test::beatSpacingIsRegular_data()
{
    QTest::addColumn<int>("blockFrames");
    QTest::newRow("2048 frames") << REFERENCE_BLOCK_FRAMES;
    QTest::newRow("512 frames") << CAPTURE_BLOCK_FRAMES;
}

void BeatTracker_Test::beatSpacingIsRegular()
{
    QFETCH(int, blockFrames);

    const double bpm = 120.0;
    BeatTracker tracker(SAMPLE_RATE, 1);
    std::vector<int16_t> audio = makeKickTrack(bpm, 30.0);
    std::vector<int> beats = runTracker(tracker, audio, blockFrames);

    // Skip the warmup (first estimate needs ~4s = ~86 blocks + lock time)
    const double blockSeconds = double(blockFrames) / SAMPLE_RATE;
    const int warmupBlocks = int(8.0 / blockSeconds);
    std::vector<double> intervals;
    int previous = -1;
    for (size_t i = 0; i < beats.size(); i++)
    {
        if (beats[i] < warmupBlocks)
            continue;
        if (previous >= 0)
            intervals.push_back((beats[i] - previous) * blockSeconds);
        previous = beats[i];
    }
    QVERIFY2(intervals.size() >= 30,
             qPrintable(QString("only %1 post-warmup beat intervals")
                        .arg(intervals.size())));

    // Each interval within one block of the true beat period, mean within 2%
    const double period = 60.0 / bpm;
    double sum = 0.0;
    for (size_t i = 0; i < intervals.size(); i++)
    {
        sum += intervals[i];
        QVERIFY2(std::fabs(intervals[i] - period) < 2.0 * blockSeconds,
                 qPrintable(QString("interval %1 s, expected %2 s")
                            .arg(intervals[i]).arg(period)));
    }
    double mean = sum / intervals.size();
    QVERIFY2(std::fabs(mean - period) / period < 0.02,
             qPrintable(QString("mean interval %1 s, expected %2 s")
                        .arg(mean).arg(period)));
}

void BeatTracker_Test::silenceGivesNoEstimate_data()
{
    QTest::addColumn<int>("blockFrames");
    QTest::newRow("2048 frames") << REFERENCE_BLOCK_FRAMES;
    QTest::newRow("512 frames") << CAPTURE_BLOCK_FRAMES;
}

void BeatTracker_Test::silenceGivesNoEstimate()
{
    QFETCH(int, blockFrames);

    BeatTracker tracker(SAMPLE_RATE, 1);
    std::vector<int16_t> silence(size_t(15.0 * SAMPLE_RATE), 0);
    std::vector<int> beats = runTracker(tracker, silence, blockFrames);

    QCOMPARE(tracker.bpm(), 0.0);
    QVERIFY(beats.empty());
}

void BeatTracker_Test::stereoMatchesMono_data()
{
    QTest::addColumn<int>("blockFrames");
    QTest::newRow("2048 frames") << REFERENCE_BLOCK_FRAMES;
    QTest::newRow("512 frames") << CAPTURE_BLOCK_FRAMES;
}

void BeatTracker_Test::stereoMatchesMono()
{
    QFETCH(int, blockFrames);

    const double bpm = 120.0;
    std::vector<int16_t> audio = makeKickTrack(bpm, 20.0);

    BeatTracker mono(SAMPLE_RATE, 1);
    runTracker(mono, audio, blockFrames, 1);

    BeatTracker stereo(SAMPLE_RATE, 2);
    runTracker(stereo, audio, blockFrames, 2);

    QVERIFY(mono.bpm() > 0.0);
    QCOMPARE(stereo.bpm(), mono.bpm());
}

QTEST_GUILESS_MAIN(BeatTracker_Test)
