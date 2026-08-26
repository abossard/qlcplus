/*
  Q Light Controller Plus - Unit test
  stage_tools_test.h

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

#ifndef STAGE_TOOLS_TEST_H
#define STAGE_TOOLS_TEST_H

#include <QObject>

class Doc;

class McpStageTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // set_fixture_placement / query_fixture_placement
    void setPlacement_roundTripsThroughQuery();
    void setPlacement_millimetreUnitsPreserved();
    void setPlacement_omittedAxesKeepValue();
    void setPlacement_multiHead_perHeadIsolation();
    void setPlacement_gelColour_data();
    void setPlacement_gelColour();
    void setPlacement_unknownFixture_error();
    void setPlacement_headOutOfRange_error();
    void setPlacement_rejectedItem_leavesNoPhantomEntry();
    void setPlacement_missingFixtureId_isPerItemError();
    void setPlacement_noGel_omitsGelColor();
    void queryPlacement_unplacedFixture_omitted();
    void placement_survivesXmlRoundTrip();

    // configure_stage
    void configureStage_setsSizeAndUnits();
    void configureStage_degenerateSize_rejected();
    void configureStage_doesNotMoveFixtures();
    void configureStage_pointOfViewNotAccepted();
    void configureStage_unknownEnum_rejected();

    // channel groups
    void createChannelGroups_upsertsByNameKeepingId();
    void createChannelGroups_replacesMembership();
    void createChannelGroups_updateKeepsOrderAndInputSource();
    void createChannelGroups_preservesChannelOrder();
    void createChannelGroups_channelOutOfRange_rejectedWholesale();
    void createChannelGroups_unknownFixture_rejected();
    void queryChannelGroups_reportsMembers();
    void deleteChannelGroups_removesGroupKeepsFixtures();
    void channelGroups_surviveXmlRoundTrip();

private:
    Doc *m_doc = nullptr;
};

#endif // STAGE_TOOLS_TEST_H
