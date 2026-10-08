/*
  Q Light Controller Plus - Unit test
  function_authoring_test.h

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

#ifndef FUNCTION_AUTHORING_TEST_H
#define FUNCTION_AUTHORING_TEST_H

#include <QObject>
#include <nlohmann/json.hpp>

class Doc;
namespace fastmcpp { namespace tools { class ToolManager; } }

class McpFunctionAuthoring_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void deleteFunctions_admission_data();
    void deleteFunctions_admission();
    void deleteFunctions_itemsIndependent();

    void deleteFunctions_boundScene_data();
    void deleteFunctions_boundScene();
    void deleteFixtureGroups_indexedOutcomes_data();
    void deleteFixtureGroups_indexedOutcomes();
    void createScenes_preflightAndAdmission_data();
    void createScenes_preflightAndAdmission();
    void createChasers_preflightAndAdmission_data();
    void createChasers_preflightAndAdmission();
    void createSequences_binding_data();
    void createSequences_binding();
    void createScenes_boundSceneChannelSet_data();
    void createScenes_boundSceneChannelSet();
    void createEfxs_preflightAndAdmission_data();
    void createEfxs_preflightAndAdmission();
    void createCollections_preflightAndAdmission_data();
    void createCollections_preflightAndAdmission();
    void createScripts_preflightAndAdmission_data();
    void createScripts_preflightAndAdmission();
    void createFixtureGroups_preflightAndAdmission_data();
    void createFixtureGroups_preflightAndAdmission();
    void createRgbMatrices_preflightAndAdmission_data();
    void createRgbMatrices_preflightAndAdmission();
    void queryFunctionDetails_targets_data();
    void queryFunctionDetails_targets();
    void queryFunctionDetails_perType_data();
    void queryFunctionDetails_perType();
    void updateFunctions_common_data();
    void updateFunctions_common();
    void updateFunctions_perType_data();
    void updateFunctions_perType();
    void updateFunctions_extendedFields_data();
    void updateFunctions_extendedFields();
    void updateFunctions_sceneMembership_data();
    void updateFunctions_sceneMembership();
    void updateFunctions_sequenceSteps_data();
    void updateFunctions_sequenceSteps();

    void functionReferences_containmentCycles_data();
    void functionReferences_containmentCycles();
    void transport_typedObjectOutput_data();
    void transport_typedObjectOutput();
    void transport_efxModeAdvertisedAndResubmittable_data();
    void transport_efxModeAdvertisedAndResubmittable();

private:
    nlohmann::json call(const std::string &tool, const nlohmann::json &args);

    Doc *m_doc = nullptr;
    fastmcpp::tools::ToolManager *m_tm = nullptr;
};

#endif
