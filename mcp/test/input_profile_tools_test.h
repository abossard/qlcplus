/*
  Q Light Controller Plus - Unit test
  input_profile_tools_test.h

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

#ifndef INPUT_PROFILE_TOOLS_TEST_H
#define INPUT_PROFILE_TOOLS_TEST_H

#include <QObject>
#include <QByteArray>
#include <QTemporaryDir>

class Doc;

class McpInputProfileTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void createProfile_writesReloadableFile();
    void createProfile_channelNumbersAreOneBased();
    void createProfile_channelTypes_data();
    void createProfile_channelTypes();
    void createProfile_duplicateChannelNumber_rejectedWholesale();
    void createProfile_emptyChannels_rejected();
    void createProfile_upsertsByManufacturerAndModel();
    void createProfile_updateReplacesInMemoryProfile();
    void createProfile_distinctPairsDoNotCollide();
    void createProfile_badMovement_isPerItemError();
    void createProfile_registeredForSetInputProfile();
    void queryProfileChannels_reportsMap();
    void queryProfileChannels_unknownProfile_error();

private:
    Doc *m_doc = nullptr;
    QTemporaryDir m_home;
    QByteArray m_realHome;
};

#endif // INPUT_PROFILE_TOOLS_TEST_H
