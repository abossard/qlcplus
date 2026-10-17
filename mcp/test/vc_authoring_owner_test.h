/*
  Q Light Controller Plus
  vc_authoring_owner_test.h
*/

#ifndef VC_AUTHORING_OWNER_TEST_H
#define VC_AUTHORING_OWNER_TEST_H

#include <QObject>
#include <QPointer>
#include <QTemporaryDir>
#include <memory>

#include "vcbridgev5.h"

class App;
class QLCIOPlugin;

class VCAuthoringOwner_Test : public QObject
{
    Q_OBJECT

public:
    ~VCAuthoringOwner_Test() override;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void authoringGeometry_profileDigest_doesNotLoadWorkspaceProfile();
    void queryPreviewApply_keepRuntimeEffectsAtZero();
    void authoringMethods_requireProductionContext();
    void authoringQueries_unimplementedReturnUnsupported_data();
    void authoringQueries_unimplementedReturnUnsupported();
    void authoringPassS1_querySetup_widgetsAndHydration_redDraft();
    void authoringPassS1_queryRecording_validationRows_redDraft_data();
    void authoringPassS1_queryRecording_validationRows_redDraft();
    void authoringPassS1_queryRecording_semanticRows_redDraft();
    void authoringCore_querySetup_payloadEnvelopeRows_redDraft_data();
    void authoringCore_querySetup_payloadEnvelopeRows_redDraft();
    void authoringCore_queryRecording_semanticsAndReadiness_redDraft();
    void authoringCore_queryRecording_continuationUnfiltered_500x10_redDraft();
    void authoringCore_queryRecording_continuationWindowed_500x10_redDraft();
    void authoringCore_querySetup_cursorCodeRows_redDraft_data();
    void authoringCore_querySetup_cursorCodeRows_redDraft();
    void authoringCore_querySetup_censusContinuation_1000x20_redDraft();
    void authoringCore_querySetup_censusMeasurements_1000x20_redDraft();
    void authoringCore_querySetup_recursiveTraversalMeasurements_1000x20_redDraft();
    void authoringCore_querySetup_recursiveUnfilteredCensus_continuation_redDraft();
    void authoringCore_querySetup_scopeOrderDigest_redDraft();
    void authoringCore_querySetup_recursiveFrameSubpageJoin_redDraft();
    void authoringCore_registeredToolNativeRoundtrip_redDraft();
    void authoringCore_registeredBindingApply_roundtrip_redDraft();
    void authoringCore_functionBinding_captionSideEffect_data();
    void authoringCore_functionBinding_captionSideEffect();
    void authoringPreparation_boundsAcrossFamilies_data();
    void authoringPreparation_boundsAcrossFamilies();
    void authoringLayout_singleOperationRows_data();
    void authoringLayout_singleOperationRows();
    void authoringCanonical_optionalAndDelimiterRows_data();
    void authoringCanonical_optionalAndDelimiterRows();
    void authoringGeometry_diffDetailRows_data();
    void authoringGeometry_diffDetailRows();
    void authoringGeometry_selectedIdentity_exactValues();
    void authoringGeometry_fullDetailSnapToOriginal_noop();
    void authoringGeometry_applyOrder_matchesRequest();
    void authoringGeometry_internalRecordShape_andScopedStale();
    void authoringGeometry_frameVolatileState_doesNotStaleSelection();
    void authoringGeometry_mixedGuardKinds_enforceGuardCap();
    void authoringGeometry_applyRefusals_functionAndSourceGuardStale();
    void authoringGeometry_prepareFailures_noTokenAndAllFindings();
    void authoringGeometry_preconditionRows_sourcesProtectionAndPlayback();
    void authoringGeometry_preconditionRows_recorderAndPanel_data();
    void authoringGeometry_preconditionRows_recorderAndPanel();
    void authoringCore_layoutFramePages_andRecordPanelConfig_persistAcrossLoad();
    void authoringCore_layoutPageOps_persistAcrossLoad();
    void authoringGeometry_applyRefusals_afterStateChange_data();
    void authoringGeometry_applyRefusals_afterStateChange();
    void authoringGeometry_interdependentGroup_swap();
    void authoringPreparation_predicateObservationRows_data();
    void authoringPreparation_predicateObservationRows();
    void authoringPassB_gateMetUnsupported_data();
    void authoringPassB_gateMetUnsupported();
    void authoringPassB_destinationPackedChannelLookup();
    void authoringPassB_destinationPredicateMatrix_data();
    void authoringPassB_destinationPredicateMatrix();
    void authoringPassB_functionBinding_feedbackPredicate();
    void authoringPassB_feedbackPredicates_useAffectedSourcesAndDescendants();
    void authoringPassB_reorderSameParent_levelSlider_noWriterSafeRefusal();
    void authoringPassB_mixedBindings_doNotLeakObservationsAcrossItems();
    void authoringPassB_assignmentA_inputValidationAndCodes_data();
    void authoringPassB_assignmentA_inputValidationAndCodes();
    void authoringPassB_assignmentA2_inputValidation_data();
    void authoringPassB_assignmentA2_inputValidation();
    void authoringPassB_assignmentA2_gateMetUnsupported_data();
    void authoringPassB_assignmentA2_gateMetUnsupported();
    void authoringPassB_assignmentB_feedbackClosure_data();
    void authoringPassB_assignmentB_feedbackClosure();
    void authoringPassB_assignmentB_writerSafeClosure_data();
    void authoringPassB_assignmentB_writerSafeClosure();
    void authoringPassB_assignmentC_gateMetMatrix_data();
    void authoringPassB_assignmentC_gateMetMatrix();
    void authoringPassB_assignmentC_residencyAndRouting_data();
    void authoringPassB_assignmentC_residencyAndRouting();
    void authoringGeometry_prepareApply_guardsAndLimits();

private:
    QByteArray serializeConsole() const;
    int counter(const char *name) const;
    void resetProbeCounters() const;
    void verifyOnlyProbePatchesActive() const;

private:
    QTemporaryDir m_settingsDir;
    QTemporaryDir m_workspaceDir;
    QTemporaryDir m_runtimeCwdDir;
    std::unique_ptr<App> m_app;
    std::unique_ptr<VCBridgeV5> m_bridge;
    QPointer<QLCIOPlugin> m_probePlugin;
    QString m_probeName;
    QString m_previousCwd;
    bool m_cwdChanged = false;
    int m_initialAccessMask = 0;
};

#endif // VC_AUTHORING_OWNER_TEST_H
