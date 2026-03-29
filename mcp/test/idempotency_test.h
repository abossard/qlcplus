/*
  Q Light Controller Plus - Unit test
  idempotency_test.h

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

#ifndef IDEMPOTENCY_TEST_H
#define IDEMPOTENCY_TEST_H

#include <QObject>

class Doc;

class McpIdempotency_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    // mcp::findFunction
    void findFunction_notFoundInEmptyDoc();
    void findFunction_findsSceneByName();
    void findFunction_findsChaserByName();
    void findFunction_findsEfxByName();
    void findFunction_findsCollectionByName();
    void findFunction_findsRgbMatrixByName();
    void findFunction_findsScriptByName();
    void findFunction_discriminatesByType();
    void findFunction_returnsNullForWrongName();

    // mcp::findFixture
    void findFixture_notFoundInEmptyDoc();
    void findFixture_findsExactMatch();
    void findFixture_noMatchDifferentName();
    void findFixture_noMatchDifferentUniverse();
    void findFixture_noMatchDifferentAddress();

    // mcp::findFixtureGroup
    void findFixtureGroup_notFoundInEmptyDoc();
    void findFixtureGroup_findsExactMatch();
    void findFixtureGroup_noMatchDifferentName();

    // ChaserStep per-step timing
    void chaserStep_carriesPerStepTiming();

    // Upsert behavior
    void upsert_sceneUpdatesValues();
    void upsert_collectionReplacesFunctions();
    void upsert_fixtureGroupReplacesFixtures();

    // Name resolution helpers
    void resolveFixtures_globMatchesStar();
    void resolveFixtures_exactMatch();
    void resolveFixtures_noMatch();
    void resolveFunction_findsByName();
    void resolveFunction_findsByNameAndType();
    void resolveFunction_returnsInvalidWhenNotFound();

    // Script deduplication
    void scriptDedup_removesStopForStartedFunction();
    void scriptDedup_removesDuplicateStops();
    void scriptDedup_preservesAcrossWait();

private:
    Doc *m_doc;

    /** Helper: builds a deduped script data string from a command list (mirrors tool logic) */
    static QString buildDedupedScript(const QVector<QPair<QString,int>> &commands);
};

#endif // IDEMPOTENCY_TEST_H
