/*
  Q Light Controller Plus - Unit test
  delete_tools_test.h

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

#ifndef DELETE_TOOLS_TEST_H
#define DELETE_TOOLS_TEST_H

#include <QObject>

class Doc;

class McpDeleteTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // delete_fixtures
    void deleteFixtures_removesFromDocAndFreesAddress();
    void deleteFixtures_referencedByScene_scrubsSceneValues();
    void deleteFixtures_batchWithUnknownId_reportsPerItemError();
    void deleteFixtures_savedXmlHasNoOrphanReference();
    void deleteFixtures_emptiedGroup_isRemoved();

    // delete_fixture_groups
    void deleteFixtureGroups_removesGroupButKeepsFixtures();
    void deleteFixtureGroups_boundMatrix_rejected();
    void deleteFixtureGroups_unboundGroup_deleted();
    void deleteFixtureGroups_unknownId_notFound();

    // vc_delete_pages
    void vcDeletePages_removesPage();
    void vcDeletePages_batchAscending_deletesHighestFirst();
    void vcDeletePages_lastRemaining_rejected();
    void vcDeletePages_batchDownToLastPage_keepsOne();
    void vcDeletePages_outOfRange_notFound();

private:
    Doc *m_doc = nullptr;
};

#endif // DELETE_TOOLS_TEST_H
