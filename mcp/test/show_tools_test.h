/*
  Q Light Controller Plus - Unit test
  show_tools_test.h

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

#ifndef SHOW_TOOLS_TEST_H
#define SHOW_TOOLS_TEST_H

#include <QObject>

class Doc;

class McpShowTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // create_shows
    void createShows_createsShowWithTracks();
    void createShows_upsertsByNameKeepingId();
    void createShows_existingTracksNotDuplicated();
    void createShows_beatDivision_setsBpm();

    // add_show_items
    void addShowItems_placesOnTimeline();
    void addShowItems_overlap_data();
    void addShowItems_overlap();
    void addShowItems_defaultDurationFromFunction();
    void addShowItems_missingTrack_isCreated();
    void addShowItems_showOnItsOwnTimeline_rejected();
    void addShowItems_unknownFunction_error();

    // query / delete
    void queryShows_reportsTracksAndItems();
    void queryShows_tempoTypeFeedsBackIntoCreate();
    void showItemIds_areDistinctAcrossTracks();
    void deleteShowItems_deletesTheNamedItemOnly();
    void createShows_updatePath_marksModified();
    void createShows_invalidField_leavesNothingBehind();
    void createShows_missingName_isPerItemError();
    void addShowItems_ambiguousFunctionName_rejected();
    void deleteShowItems_removesItemKeepsFunction();
    void deleteShowItems_removesWholeTrack();

    // persistence
    void show_survivesXmlRoundTrip();

private:
    Doc *m_doc = nullptr;
};

#endif // SHOW_TOOLS_TEST_H
