/*
  Q Light Controller Plus - Unit test
  live_tools_test.h

  Copyright (C) Massimo Callegari

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

#ifndef LIVE_TOOLS_TEST_H
#define LIVE_TOOLS_TEST_H

#include <QObject>

class Doc;

class McpLiveTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // grand master
    void setGrandMaster_roundTripsThroughQuery();
    void setGrandMaster_modes_data();
    void setGrandMaster_modes();
    void setGrandMaster_valueOutOfRange_rejected();
    void setGrandMaster_scalesUniverseOutputNotPreGmValues();
    void grandMaster_isSessionStateNotSaved();

    // blackout
    void setBlackout_togglesAndReports();
    void setBlackout_suppressesAtOutputKeepingValues();

    // write_dmx
    void writeDmx_setsChannelOnOutput();
    void writeDmx_channelOutOfRange_rejected();
    void writeDmx_unknownFixture_rejected();
    void writeDmx_release_clearsHeldChannels();
    void writeDmx_releaseWithItems_rejected();
    void writeDmx_heldValuesClearedWhenProjectCleared();

    // run_functions
    void runFunctions_startThenStop();
    void runFunctions_unknownFunction_notFound();
    void runFunctions_missingAction_isPerItemError();
    void queryRunningFunctions_listsStarted();

private:
    Doc *m_doc = nullptr;
};

#endif // LIVE_TOOLS_TEST_H
