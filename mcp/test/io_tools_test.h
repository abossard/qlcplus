/*
  Q Light Controller Plus - Unit test
  io_tools_test.h

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

#ifndef IO_TOOLS_TEST_H
#define IO_TOOLS_TEST_H

#include <QObject>

class Doc;

class McpIoTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    // configure_universes — growth
    void configureUniverses_idBeyondCount_createsUniverses_data();
    void configureUniverses_idBeyondCount_createsUniverses();
    void configureUniverses_createdUniverseIsUsable();
    void configureUniverses_createdUniversesAreStarted();
    void configureUniverses_existingId_doesNotGrow();
    void configureUniverses_invalidId_rejected_data();
    void configureUniverses_invalidId_rejected();

    // delete_universes
    void deleteUniverses_trailing_removesAndReports();
    void deleteUniverses_batchOutOfOrder_removesBoth();
    void deleteUniverses_wouldLeaveGap_rejected();
    void deleteUniverses_duplicateIds_deletedOnce();
    void deleteUniverses_crossUniverseFixture_rejected();
    void deleteUniverses_withPatchedFixtures_rejected();
    void deleteUniverses_lastRemaining_rejected();
    void deleteUniverses_unknownId_notFound();

    // persistence
    void createdUniverses_surviveXmlRoundTrip();

private:
    Doc *m_doc = nullptr;
};

#endif // IO_TOOLS_TEST_H
